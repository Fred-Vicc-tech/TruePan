// TruePanDSP.h - ljudmotorn i TruePan (panorering), utan Windows-beroenden
// så att den kan testas automatiskt på vilken dator som helst.
//
// Signalväg per sampel:
//   in L/R -> [LOW MONO: delning i bas (mono, i mitten) och resten]
//          -> WIDTH (mid/side) -> MODE (BALANCE eller STEREO)
//          -> [LESLIE: roterande högtalare]
//          -> panorering med vald PAN LAW (PAN + AUTO-PAN)
//          -> basen läggs tillbaka i mitten -> [TREMOLO] -> OUTPUT
//          -> MONITOR (STEREO / MONO / MID / SIDES)
//
// Grundregel: med standardvärden (PAN i mitten, WIDTH 100 %, LOW MONO av,
// AUTO-PAN 0 %, OUTPUT 0 dB) går ljudet igenom oförändrat.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

namespace truepan {

const double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------------------
// Parametrar (index får aldrig ändras: Live sparar automation per index)
// ---------------------------------------------------------------------------
enum {
    kParamPan = 0,
    kParamWidth,
    kParamMode,
    kParamLaw,
    kParamLowMono,
    kParamDepth,
    kParamRate,
    kParamShape,
    kParamOutput,
    kParamMonitor,      // nya parametrar läggs alltid sist
    kParamFxMode,
    kParamLesSpeed,
    kNumParams
};

// Högst 7 tecken (Live 9 visar inte fler).
static const char* const kParamNames[kNumParams] = {
    "Pan", "Width", "Mode", "Law", "LoMono", "APDepth", "APRate", "APShape", "Output",
    "Monitor", "APMode", "LesSpd"
};
static const char* const kParamLabels[kNumParams] = {
    "", "%", "", "dB", "Hz", "%", "", "", "dB", "", "", ""
};

// --- Val (knappar/väljare) ------------------------------------------------
enum { kModeBalance = 0, kModeStereo = 1 };
enum { kLaw0 = 0, kLaw3, kLaw45, kLaw6, kNumLaws };
enum { kShapeSine = 0, kShapeTriangle, kShapeSquare, kNumShapes };
const int kNumRates = 12;
enum { kMonStereo = 0, kMonMono, kMonMid, kMonSides, kNumMonitors };
enum { kFxPan = 0, kFxTremolo, kFxLeslie, kNumFxModes };
// LESLIE SPEED: en rotation (hornet) per notlängd, följer Lives tempo.
// Ratten går från långsamt (1/2) till snabbt (1/32), som RATE.
const int kNumLesSpeeds = 8;
static const char* const kLesSpeedNames[kNumLesSpeeds] = { "1/2", "1/4", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32" };
static const double kLesSpeedBeats[kNumLesSpeeds] = { 2.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
const int kLesSpeedSlowV11 = 0, kLesSpeedFastV11 = 5;   // v1.1 hade SLOW/FAST -> 1/2 / 1/16

static const char* const kLawNames[kNumLaws] = { "0.0", "-3.0", "-4.5", "-6.0" };
static const char* const kShapeNames[kNumShapes] = { "Sine", "Tri", "Square" };
static const char* const kShapeLongNames[kNumShapes] = { "SINE", "TRIANGLE", "SQUARE" };
static const char* const kMonitorNames[kNumMonitors] = { "Stereo", "Mono", "Mid", "Sides" };
static const char* const kMonitorLongNames[kNumMonitors] = { "STEREO", "MONO", "MID", "SIDES" };
static const char* const kFxNames[kNumFxModes] = { "Pan", "Trem", "Leslie" };
static const char* const kRateNames[kNumRates] = {
    "4 Bars", "2 Bars", "1 Bar", "1/2", "1/2T", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32"
};
// Periodens längd i taktslag (fjärdedelsnoter)
static const double kRateBeats[kNumRates] = {
    16.0, 8.0, 4.0, 2.0, 4.0 / 3.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125
};

inline double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline int choiceFromNorm(float n, int count)
{
    if (!(n >= 0.0f)) n = 0.0f;
    if (n > 1.0f) n = 1.0f;
    int i = (int)std::floor(n * (count - 1) + 0.5f);
    return i < 0 ? 0 : (i >= count ? count - 1 : i);
}
inline float choiceNorm(int i, int count) { return count > 1 ? (float)i / (float)(count - 1) : 0.0f; }

// PAN: -1 (helt vänster) .. +1 (helt höger)
inline double panFromNorm(float n) { return clampd(n * 2.0 - 1.0, -1.0, 1.0); }
inline float panNorm(double pan) { return (float)((clampd(pan, -1.0, 1.0) + 1.0) * 0.5); }

// WIDTH: 0 .. 200 %
inline double widthFromNorm(float n) { return clampd(n, 0.0f, 1.0f) * 2.0; }
inline float widthNorm(double w) { return (float)clampd(w * 0.5, 0.0, 1.0); }

inline int modeFromNorm(float n) { return n >= 0.5f ? kModeStereo : kModeBalance; }
inline int lawFromNorm(float n) { return choiceFromNorm(n, kNumLaws); }
inline int shapeFromNorm(float n) { return choiceFromNorm(n, kNumShapes); }
inline int rateFromNorm(float n) { return choiceFromNorm(n, kNumRates); }
inline int monitorFromNorm(float n) { return choiceFromNorm(n, kNumMonitors); }
inline int fxModeFromNorm(float n) { return choiceFromNorm(n, kNumFxModes); }
inline int lesSpeedFromNorm(float n) { return choiceFromNorm(n, kNumLesSpeeds); }
// Projekt/presets från v1.1: SLOW (0) / FAST (1) räknas om till ratten
inline float lesSpeedNormFromV11(float old) { return choiceNorm(old >= 0.5f ? kLesSpeedFastV11 : kLesSpeedSlowV11, kNumLesSpeeds); }

// LOW MONO: Off (ratten i botten) eller 40 .. 300 Hz (logaritmiskt)
const double kLowMonoMinHz = 40.0, kLowMonoMaxHz = 300.0;
inline bool lowMonoOn(float n) { return n >= 0.001f; }
inline double lowMonoHz(float n)
{
    return kLowMonoMinHz * std::pow(kLowMonoMaxHz / kLowMonoMinHz, clampd(n, 0.0, 1.0));
}
inline float lowMonoNorm(double hz)
{
    return (float)clampd(std::log(hz / kLowMonoMinHz) / std::log(kLowMonoMaxHz / kLowMonoMinHz), 0.001, 1.0);
}

inline double depthFromNorm(float n) { return clampd(n, 0.0f, 1.0f); }

// OUTPUT: -24 .. +12 dB
const double kOutputMinDb = -24.0, kOutputMaxDb = 12.0;
inline double outputNormToDb(float n) { return kOutputMinDb + clampd(n, 0.0f, 1.0f) * (kOutputMaxDb - kOutputMinDb); }
inline float outputDbToNorm(double db) { return (float)clampd((db - kOutputMinDb) / (kOutputMaxDb - kOutputMinDb), 0.0, 1.0); }

inline double dbToGain(double db) { return std::pow(10.0, db / 20.0); }
inline double gainToDb(double g) { return g > 1e-12 ? 20.0 * std::log10(g) : -240.0; }

inline float defaultParam(int index)
{
    switch (index) {
    case kParamPan:     return 0.5f;                            // mitten
    case kParamWidth:   return 0.5f;                            // 100 %
    case kParamMode:    return 1.0f;                            // STEREO
    case kParamLaw:     return choiceNorm(kLaw3, kNumLaws);     // -3 dB
    case kParamLowMono: return 0.0f;                            // Off
    case kParamDepth:   return 0.0f;                            // AUTO-PAN av
    case kParamRate:    return choiceNorm(5, kNumRates);        // 1/4
    case kParamShape:   return choiceNorm(kShapeSine, kNumShapes);
    case kParamOutput:  return outputDbToNorm(0.0);
    case kParamMonitor: return 0.0f;                            // STEREO
    case kParamFxMode:  return 0.0f;                            // PAN
    case kParamLesSpeed: return choiceNorm(1, kNumLesSpeeds);   // 1/4
    default:            return 0.0f;
    }
}

// PAN som text: "C", "L35", "R100" (spaced = "L 35" för fönstret)
inline void panText(double pan, char* out, size_t max, bool spaced)
{
    const int units = (int)std::floor(std::fabs(pan) * 100.0 + 0.5);
    if (units == 0) std::snprintf(out, max, "C");
    else std::snprintf(out, max, spaced ? "%c %d" : "%c%d", pan < 0 ? 'L' : 'R', units);
}

// Värdet som Live visar (utan enhet; enheten kommer från kParamLabels)
inline void paramDisplayText(int index, float n, char* out, size_t max)
{
    switch (index) {
    case kParamPan:     panText(panFromNorm(n), out, max, false); break;
    case kParamWidth:   std::snprintf(out, max, "%.0f", widthFromNorm(n) * 100.0); break;
    case kParamMode:    std::snprintf(out, max, "%s", modeFromNorm(n) == kModeStereo ? "Stereo" : "Balance"); break;
    case kParamLaw:     std::snprintf(out, max, "%s", kLawNames[lawFromNorm(n)]); break;
    case kParamLowMono:
        if (lowMonoOn(n)) std::snprintf(out, max, "%.0f", lowMonoHz(n));
        else std::snprintf(out, max, "Off");
        break;
    case kParamDepth:   std::snprintf(out, max, "%.0f", depthFromNorm(n) * 100.0); break;
    case kParamRate:    std::snprintf(out, max, "%s", kRateNames[rateFromNorm(n)]); break;
    case kParamShape:   std::snprintf(out, max, "%s", kShapeNames[shapeFromNorm(n)]); break;
    case kParamOutput:  std::snprintf(out, max, "%.1f", outputNormToDb(n)); break;
    case kParamMonitor: std::snprintf(out, max, "%s", kMonitorNames[monitorFromNorm(n)]); break;
    case kParamFxMode:  std::snprintf(out, max, "%s", kFxNames[fxModeFromNorm(n)]); break;
    case kParamLesSpeed: std::snprintf(out, max, "%s", kLesSpeedNames[lesSpeedFromNorm(n)]); break;
    default:            std::snprintf(out, max, "-"); break;
    }
}

// ---------------------------------------------------------------------------
// PAN LAW. Förstärkning för vänster/höger kanal vid position pos (-1..+1).
// Normerad så att mitten alltid är 0 dB (pluginet ändrar inte nivån när PAN
// står i mitten). Helt åt ena sidan: bortre kanalen tyst, närmaste kanalen
// höjs med lagens värde (0 / +3 / +4,5 / +6 dB). -3 dB = lika stark ton
// (konstant effekt) för monoljud oavsett position.
// ---------------------------------------------------------------------------
inline void panGains(double pos, int law, double& gl, double& gr)
{
    pos = clampd(pos, -1.0, 1.0);
    const double lin_l = (1.0 - pos) * 0.5, lin_r = (1.0 + pos) * 0.5;
    const double th = (pos + 1.0) * kPi * 0.25;
    const double c3 = std::cos(kPi * 0.25); // 0,7071 = -3 dB i mitten
    switch (law) {
    case kLaw0:
        gl = lin_l * 2.0 > 1.0 ? 1.0 : lin_l * 2.0;
        gr = lin_r * 2.0 > 1.0 ? 1.0 : lin_r * 2.0;
        break;
    case kLaw45: {
        const double cl = std::cos(th), cr = std::sin(th);
        const double c45 = std::sqrt(0.5 * c3); // -4,5 dB i mitten
        gl = std::sqrt(lin_l * (cl > 0.0 ? cl : 0.0)) / c45;
        gr = std::sqrt(lin_r * (cr > 0.0 ? cr : 0.0)) / c45;
        break;
    }
    case kLaw6:
        gl = lin_l * 2.0;
        gr = lin_r * 2.0;
        break;
    case kLaw3:
    default:
        gl = std::cos(th) / c3;
        gr = std::sin(th) / c3;
        break;
    }
    if (gl < 1e-12) gl = 0.0;
    if (gr < 1e-12) gr = 0.0;
}

// AUTO-PAN-vågform, fas 0..1 -> -1..+1 (börjar i mitten och går åt höger)
inline double lfoValue(int shape, double phase)
{
    phase -= std::floor(phase);
    switch (shape) {
    case kShapeTriangle:
        if (phase < 0.25) return 4.0 * phase;
        if (phase < 0.75) return 2.0 - 4.0 * phase;
        return 4.0 * phase - 4.0;
    case kShapeSquare:
        return phase < 0.5 ? 1.0 : -1.0;
    case kShapeSine:
    default:
        return std::sin(2.0 * kPi * phase);
    }
}

// ---------------------------------------------------------------------------
// Andra ordningens filter (Butterworth). Två i rad = Linkwitz-Riley 24 dB/okt:
// lågpass + högpass summerar till platt nivå, så LOW MONO tappar ingen bas.
// ---------------------------------------------------------------------------
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void set(bool highpass, double hz, double sr)
    {
        const double w0 = 2.0 * kPi * clampd(hz, 10.0, sr * 0.45) / sr;
        const double cw = std::cos(w0), alpha = std::sin(w0) / (2.0 * 0.70710678118654752);
        const double a0 = 1.0 + alpha;
        if (highpass) { b0 = (1.0 + cw) * 0.5; b1 = -(1.0 + cw); b2 = (1.0 + cw) * 0.5; }
        else          { b0 = (1.0 - cw) * 0.5; b1 = 1.0 - cw;    b2 = (1.0 - cw) * 0.5; }
        b0 /= a0; b1 /= a0; b2 /= a0;
        a1 = -2.0 * cw / a0;
        a2 = (1.0 - alpha) / a0;
    }
    void reset() { z1 = z2 = 0.0; }
    double process(double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

struct LR4
{
    Biquad s1, s2;
    void set(bool highpass, double hz, double sr) { s1.set(highpass, hz, sr); s2.set(highpass, hz, sr); }
    void reset() { s1.reset(); s2.reset(); }
    double process(double x) { return s2.process(s1.process(x)); }
};

// ---------------------------------------------------------------------------
// LESLIE: roterande högtalare i ett rum. Ljudet delas vid 800 Hz i bastrumma
// och diskanthorn som snurrar med var sin hastighet (trumman ca 13 % långsammare
// och tröttare, som på originalet). Två mikrofoner står på var sin sida
// (vänster och höger): när hornet pekar mot vänster hörs det starkt och nära
// där, samtidigt som höger mest hör reflexen från rummets vägg. Ljudet snurrar
// alltså runt i stereobilden, med dopplereffekt (tonhöjden svajar).
// Farten byts mjukt (hornet på ca 1 s, trumman på några sekunder).
// ---------------------------------------------------------------------------
class Leslie
{
public:
    static const int kBufSize = 16384;   // räcker till 768 kHz (högst ca 14 ms fördröjning)

    Leslie() : horn_(kBufSize, 0.0), drum_(kBufSize, 0.0) {}

    void prepare(double sr)
    {
        sr_ = sr;
        lpDrum_.set(false, 800.0, sr); hpHorn_.set(true, 800.0, sr);
        reflLp_ = 1.0 - std::exp(-2.0 * kPi * 5000.0 / sr);  // väggen tar bort lite diskant
        reset();
    }
    void reset()
    {
        lpDrum_.reset(); hpHorn_.reset();
        std::fill(horn_.begin(), horn_.end(), 0.0);
        std::fill(drum_.begin(), drum_.end(), 0.0);
        reflZ_[0] = reflZ_[1] = 0.0;
        w_ = 0;
    }
    void setHornHz(double hz) { target_ = clampd(hz, 0.1, 25.0); }
    void setRoom(double amount) { room_ = clampd(amount, 0.0, 1.0); }   // 0 = bara mikrofonerna (för tester)

    // Hastigheten och vinklarna går alltid, även när effekten inte hörs,
    // så att den låter naturligt när man slår på den.
    void advance()
    {
        const double hornT = target_, drumT = target_ * 0.87;
        const double hornTau = hornT > hornHz_ ? 0.7 : 1.0;   // sekunder
        const double drumTau = drumT > drumHz_ ? 3.0 : 4.0;
        hornHz_ += (hornT - hornHz_) * (1.0 - std::exp(-1.0 / (hornTau * sr_)));
        drumHz_ += (drumT - drumHz_) * (1.0 - std::exp(-1.0 / (drumTau * sr_)));
        hornAng_ += 2.0 * kPi * hornHz_ / sr_;
        drumAng_ += 2.0 * kPi * drumHz_ / sr_;
        if (hornAng_ > 2.0 * kPi) hornAng_ -= 2.0 * kPi;
        if (drumAng_ > 2.0 * kPi) drumAng_ -= 2.0 * kPi;
    }

    void process(double in, double& outL, double& outR)
    {
        horn_[w_] = hpHorn_.process(in);
        drum_[w_] = lpDrum_.process(in);
        // Mikrofonerna på motsatta sidor (vänster = +90 grader, höger = -90 grader)
        const double micL = 0.5 * kPi, micR = -0.5 * kPi;
        double l = rotor(horn_, hornAng_ - micL, 0.0005, 0.85) + rotor(drum_, drumAng_ - micL, 0.0002, 0.5);
        double r = rotor(horn_, hornAng_ - micR, 0.0005, 0.85) + rotor(drum_, drumAng_ - micR, 0.0002, 0.5);
        // Rummet: när rotorn pekar bort från en mikrofon hörs ljudet via väggen
        // bakom (längre väg, olika långt till vänster och höger vägg).
        double rl = wall(hornAng_ - micL, drumAng_ - micL, 0.0073);
        double rr = wall(hornAng_ - micR, drumAng_ - micR, 0.0111);
        reflZ_[0] += (rl - reflZ_[0]) * reflLp_;
        reflZ_[1] += (rr - reflZ_[1]) * reflLp_;
        outL = (l + 0.55 * room_ * reflZ_[0]) * kNorm;
        outR = (r + 0.55 * room_ * reflZ_[1]) * kNorm;
        w_ = (w_ + 1) & (kBufSize - 1);
    }

    double hornHz() const { return hornHz_; }
    double drumHz() const { return drumHz_; }
    double hornAngle() const { return hornAng_; }

private:
    // Nivå: Leslien ger brett, okorrelerat ljud som upplevs svagare än samma
    // effekt i mitten. Därför ligger den ca 1,5 dB över torrt ljud i effekt,
    // så att den upplevs lika stark som PAN-läget.
    static constexpr double kNorm = 1.05;

    // Rotorn sedd från en mikrofon: avståndet ändras (fördröjning = doppler)
    // och nivån är högst när rotorn pekar mot mikrofonen.
    double rotor(const std::vector<double>& buf, double ang, double depthSec, double amDepth) const
    {
        const double c = std::cos(ang);
        const double delay = (0.0012 + depthSec * (1.0 - c)) * sr_;
        const double gain = 1.0 + amDepth * c;
        return read(buf, delay) * gain;
    }
    // Reflexen: starkast när rotorn pekar bort från mikrofonen (mot väggen)
    double wall(double hornAng, double drumAng, double extraSec) const
    {
        const double ch = -std::cos(hornAng), cd = -std::cos(drumAng);
        const double h = read(horn_, (0.0012 + extraSec + 0.0005 * (1.0 - ch)) * sr_) * (0.5 + 0.5 * ch);
        const double d = read(drum_, (0.0012 + extraSec + 0.0002 * (1.0 - cd)) * sr_) * (0.7 + 0.3 * cd);
        return h + d;
    }
    // Läs med bråkdels-fördröjning (Hermite-interpolation)
    double read(const std::vector<double>& buf, double delay) const
    {
        const int di = (int)delay;
        const double f = delay - di;
        const int m = kBufSize - 1;
        const double y0 = buf[(w_ - di + 1) & m], y1 = buf[(w_ - di) & m];
        const double y2 = buf[(w_ - di - 1) & m], y3 = buf[(w_ - di - 2) & m];
        const double c1 = 0.5 * (y2 - y0);
        const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    double sr_ = 44100.0;
    LR4 lpDrum_, hpHorn_;
    std::vector<double> horn_, drum_;
    double reflLp_ = 0.5, reflZ_[2] = { 0.0, 0.0 }, room_ = 1.0;
    int w_ = 0;
    double target_ = 2.0;
    double hornHz_ = 2.0, drumHz_ = 1.74, hornAng_ = 0.0, drumAng_ = 1.0;
};

// ---------------------------------------------------------------------------
// Motorn
// ---------------------------------------------------------------------------
class Engine
{
public:
    Engine() { prepare(44100.0); }

    void prepare(double sampleRate)
    {
        sr_ = (sampleRate >= 8000.0 && sampleRate <= 768000.0) ? sampleRate : 44100.0;
        smoothCoef_ = 1.0 - std::exp(-1.0 / (0.005 * sr_));   // 5 ms (pan, bredd, nivå)
        fadeCoef_ = 1.0 - std::exp(-1.0 / (0.020 * sr_));     // 20 ms (LOW MONO av/på)
        filterHz_ = -1.0;
        leslie_.prepare(sr_);
        reset();
    }

    void reset()
    {
        for (int i = 0; i < 2; ++i) { hpL_[i].reset(); hpR_[i].reset(); }
        lpM_.reset();
        leslie_.reset();
        snap_ = true;
        peakL_ = peakR_ = 0.0;
    }

    // Parametrar (normerade 0..1), sätts en gång per block
    void setParams(const float* n)
    {
        pan_ = panFromNorm(n[kParamPan]);
        width_ = widthFromNorm(n[kParamWidth]);
        mode_ = modeFromNorm(n[kParamMode]);
        law_ = lawFromNorm(n[kParamLaw]);
        lowOn_ = lowMonoOn(n[kParamLowMono]);
        if (lowOn_) lowHz_ = lowMonoHz(n[kParamLowMono]);
        depth_ = depthFromNorm(n[kParamDepth]);
        rate_ = rateFromNorm(n[kParamRate]);
        shape_ = shapeFromNorm(n[kParamShape]);
        outGain_ = dbToGain(outputNormToDb(n[kParamOutput]));
        monitor_ = monitorFromNorm(n[kParamMonitor]);
        fxMode_ = fxModeFromNorm(n[kParamFxMode]);
        lesSpeed_ = lesSpeedFromNorm(n[kParamLesSpeed]);
    }

    // Tempo och position från värden. När Live spelar följer AUTO-PAN takten.
    void setTransport(bool tempoValid, double tempo, bool playing, bool ppqValid, double ppq)
    {
        if (tempoValid && tempo >= 20.0 && tempo <= 999.0) tempo_ = tempo;
        if (playing && ppqValid && std::isfinite(ppq)) {
            const double ph = ppq / kRateBeats[rate_];
            phase_ = ph - std::floor(ph);
            synced_ = true;
        } else {
            synced_ = false;
        }
    }

    // inL/inR får vara samma minne som outL/outR (Live gör ibland så).
    void process(const float* inL, const float* inR, float* outL, float* outR, int n, bool accumulate)
    {
        if (!outL || n <= 0) return;
        if (!inL && inR) inL = inR;       // bara en ingång: använd den till båda
        if (!inR) inR = inL;

        if (lowHz_ != filterHz_) {
            for (int i = 0; i < 2; ++i) { hpL_[i].set(true, lowHz_, sr_); hpR_[i].set(true, lowHz_, sr_); }
            lpM_.set(false, lowHz_, sr_);
            filterHz_ = lowHz_;
        }

        const double phaseInc = tempo_ / 60.0 / sr_ / kRateBeats[rate_];
        leslie_.setHornHz(tempo_ / 60.0 / kLesSpeedBeats[lesSpeed_]);
        double pkL = 0.0, pkR = 0.0;
        double pos = pan_;

        for (int i = 0; i < n; ++i) {
            const double l = inL ? inL[i] : 0.0;
            const double r = inR ? inR[i] : 0.0;

            // Position: PAN + AUTO-PAN. TREMOLO: nivån svajar i takt.
            pos = pan_;
            double ttrem = 1.0;
            if (depth_ > 0.0) {
                const double lfo = lfoValue(shape_, phase_);
                if (fxMode_ == kFxPan) pos = clampd(pan_ + depth_ * lfo, -1.0, 1.0);
                else if (fxMode_ == kFxTremolo) {
                    // Kurva med exponent 1,5: djupare dalar än rak kurva, så att tremolot
                    // hörs tydligt (DEPTH 50 % sänker 9 dB; rak kurva hade gett 6 dB).
                    const double t = 1.0 - depth_ * (0.5 - 0.5 * lfo);
                    ttrem = t * std::sqrt(t);
                }
            }
            const double tles = fxMode_ == kFxLeslie ? depth_ : 0.0;
            phase_ += phaseInc;
            if (phase_ >= 1.0) phase_ -= std::floor(phase_);

            // Målvärden
            double tgl, tgr;
            panGains(pos, law_, tgl, tgr);
            const double narrow = mode_ == kModeStereo ? 1.0 - std::fabs(pos) : 1.0;
            const double tside = width_ * narrow;
            const double tlow = lowOn_ ? 1.0 : 0.0;

            if (snap_) {
                gl_ = tgl; gr_ = tgr; side_ = tside; out_ = outGain_; low_ = tlow;
                trem_ = ttrem; les_ = tles;
                snap_ = false;
            } else {
                gl_ += (tgl - gl_) * smoothCoef_;
                gr_ += (tgr - gr_) * smoothCoef_;
                side_ += (tside - side_) * smoothCoef_;
                out_ += (outGain_ - out_) * smoothCoef_;
                low_ += (tlow - low_) * fadeCoef_;
                if (std::fabs(low_ - tlow) < 1e-6) low_ = tlow;
                trem_ += (ttrem - trem_) * smoothCoef_;
                les_ += (tles - les_) * fadeCoef_;
                if (std::fabs(les_ - tles) < 1e-6) les_ = tles;
            }

            // LOW MONO: basen (mono) delas av och läggs i mitten efter panoreringen.
            // Filtren körs alltid, så att av/på inte klickar.
            const double hl = hpL_[1].process(hpL_[0].process(l));
            const double hr = hpR_[1].process(hpR_[0].process(r));
            const double lowM = lpM_.process((l + r) * 0.5);
            double xl = l, xr = r, bass = 0.0;
            if (low_ > 0.0) {
                xl = l + (hl - l) * low_;
                xr = r + (hr - r) * low_;
                bass = lowM * low_;
            }

            // WIDTH och MODE (mid/side)
            double yl = xl, yr = xr;
            if (side_ != 1.0) {
                const double m = (xl + xr) * 0.5, s = (xl - xr) * 0.5 * side_;
                yl = m + s;
                yr = m - s;
            }

            // LESLIE (DEPTH = andel effekt). Rotorerna snurrar alltid.
            // Körs alltid (fördröjningen hålls fylld), så att av/på inte klickar.
            leslie_.advance();
            {
                double ll, lr;
                leslie_.process((yl + yr) * 0.5, ll, lr);
                if (les_ > 0.0) {
                    yl += (ll - yl) * les_;
                    yr += (lr - yr) * les_;
                }
            }

            // Panorering + bas i mitten + TREMOLO + OUTPUT
            const double g = trem_ * out_;
            double ol = (yl * gl_ + bass) * g;
            double orr = (yr * gr_ + bass) * g;

            // MONITOR: lyssna på hela, mono, bara mitten eller bara sidorna
            if (monitor_ == kMonMono || monitor_ == kMonMid) { const double m = (ol + orr) * 0.5; ol = orr = m; }
            else if (monitor_ == kMonSides) { const double s = (ol - orr) * 0.5; ol = orr = s; }

            if (accumulate) { outL[i] += (float)ol; if (outR) outR[i] += (float)orr; }
            else            { outL[i] = (float)ol;  if (outR) outR[i] = (float)orr; }

            const double al = std::fabs(ol), ar = std::fabs(orr);
            if (al > pkL) pkL = al;
            if (ar > pkR) pkR = ar;
        }
        peakL_ = pkL;
        peakR_ = pkR;
        displayPos_ = pos;
    }

    // För fönstret
    double displayPos() const { return displayPos_; }
    double peakL() const { return peakL_; }
    double peakR() const { return peakR_; }
    bool synced() const { return synced_; }
    double phase() const { return phase_; }
    double tempo() const { return tempo_; }
    double sampleRate() const { return sr_; }
    double leslieHornHz() const { return leslie_.hornHz(); }
    double leslieDrumHz() const { return leslie_.drumHz(); }

private:
    double sr_ = 44100.0;
    double smoothCoef_ = 0.0, fadeCoef_ = 0.0;

    // parametrar
    double pan_ = 0.0, width_ = 1.0, depth_ = 0.0, outGain_ = 1.0, lowHz_ = 120.0;
    int mode_ = kModeStereo, law_ = kLaw3, rate_ = 5, shape_ = kShapeSine;
    int monitor_ = kMonStereo, fxMode_ = kFxPan, lesSpeed_ = 1;
    bool lowOn_ = false;

    // utjämnade värden
    double gl_ = 1.0, gr_ = 1.0, side_ = 1.0, out_ = 1.0, low_ = 0.0, trem_ = 1.0, les_ = 0.0;
    bool snap_ = true;

    // AUTO-PAN
    double tempo_ = 120.0, phase_ = 0.0;
    bool synced_ = false;

    // LOW MONO
    Biquad hpL_[2], hpR_[2];
    LR4 lpM_;
    double filterHz_ = -1.0;

    Leslie leslie_;

    // visning
    double displayPos_ = 0.0, peakL_ = 0.0, peakR_ = 0.0;
};

// ---------------------------------------------------------------------------
// Fabrikspresets (syns som program i Lives presetlista och i fönstret)
// ---------------------------------------------------------------------------
const int kNumFactoryPresets = 13;

inline const char* factoryPresetName(int i)
{
    static const char* const names[kNumFactoryPresets] = {
        "Center", "Hard Left", "Hard Right", "Wide Stereo", "Mono",
        "Auto-Pan 1/4", "Auto-Pan 1/8 Tri", "Ping-Pong 1/16", "Slow Sweep 2 Bars", "Bass Mono 120",
        "Tremolo 1/8", "Leslie Slow", "Leslie Fast"
    };
    return (i >= 0 && i < kNumFactoryPresets) ? names[i] : "";
}

inline void factoryPresetValues(int i, float* v)
{
    for (int k = 0; k < kNumParams; ++k) v[k] = defaultParam(k);
    switch (i) {
    case 1: v[kParamPan] = 0.0f; break;
    case 2: v[kParamPan] = 1.0f; break;
    case 3: v[kParamWidth] = widthNorm(1.5); break;
    case 4: v[kParamWidth] = 0.0f; break;
    case 5: v[kParamDepth] = 1.0f; v[kParamRate] = choiceNorm(5, kNumRates); break;
    case 6: v[kParamDepth] = 0.8f; v[kParamRate] = choiceNorm(7, kNumRates); v[kParamShape] = choiceNorm(kShapeTriangle, kNumShapes); break;
    case 7: v[kParamDepth] = 1.0f; v[kParamRate] = choiceNorm(9, kNumRates); v[kParamShape] = choiceNorm(kShapeSquare, kNumShapes);
            v[kParamMode] = 0.0f; break;
    case 8: v[kParamDepth] = 0.7f; v[kParamRate] = choiceNorm(1, kNumRates); break;
    case 9: v[kParamLowMono] = lowMonoNorm(120.0); v[kParamWidth] = widthNorm(1.2); break;
    case 10: v[kParamFxMode] = choiceNorm(kFxTremolo, kNumFxModes); v[kParamDepth] = 0.85f; v[kParamRate] = choiceNorm(7, kNumRates); break;
    case 11: v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes); v[kParamDepth] = 1.0f; v[kParamLesSpeed] = choiceNorm(0, kNumLesSpeeds); break;
    case 12: v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes); v[kParamDepth] = 1.0f; v[kParamLesSpeed] = choiceNorm(5, kNumLesSpeeds); break;
    default: break;
    }
}

// Rensar ett presetnamn: bara tecken som syns i Live, högst 31 tecken.
inline void cleanName(const char* in, char* out, size_t max)
{
    size_t j = 0;
    for (size_t i = 0; in && in[i] && j + 1 < max && j < 31; ++i) {
        const unsigned char c = (unsigned char)in[i];
        out[j++] = (c >= 32 && c < 127) ? (char)c : '_';
    }
    out[j] = 0;
    if (j == 0 && max > 1) std::snprintf(out, max, "Preset");
}

// ---------------------------------------------------------------------------
// Egna presets som textfil (Dokument\TruePan Presets\*.tpp)
// ---------------------------------------------------------------------------
static const char* const kPresetHeader = "TruePan preset 2";
static const char* const kPresetHeaderV11 = "TruePan preset 1";   // v1.1: LesSpd var SLOW/FAST

inline std::string cleanPresetName(const std::string& in)
{
    std::string out;
    for (char c : in) {
        const unsigned char u = (unsigned char)c;
        if (u >= 32 && u < 127 && c != '=') out += c;
        else if (u >= 128) out += '_';
        if (out.size() >= 31) break;
    }
    return out.empty() ? std::string("Untitled") : out;
}

inline std::string presetToText(const std::string& name, const float* v)
{
    std::string t = kPresetHeader;
    t += "\r\nName=" + cleanPresetName(name) + "\r\n";
    char buf[64];
    for (int i = 0; i < kNumParams; ++i) {
        std::snprintf(buf, sizeof buf, "%s=%.6f\r\n", kParamNames[i], (double)v[i]);
        t += buf;
    }
    return t;
}

// Föreslår ett namn som inte redan används: "Wide", "Wide 2", "Wide 3" ...
template <typename ExistsFn>
inline std::string uniquePresetName(const std::string& base, ExistsFn exists)
{
    const std::string clean = cleanPresetName(base);
    if (!exists(clean)) return clean;
    std::string stem = clean;
    int n = 2;
    const size_t sp = clean.find_last_of(' ');
    if (sp != std::string::npos && sp + 1 < clean.size()) {
        bool digits = true;
        for (size_t i = sp + 1; i < clean.size(); ++i) digits = digits && clean[i] >= '0' && clean[i] <= '9';
        if (digits && clean.size() - sp - 1 <= 3) { stem = clean.substr(0, sp); n = std::atoi(clean.c_str() + sp + 1) + 1; }
    }
    for (; n < 1000; ++n) {
        const std::string candidate = stem + " " + std::to_string(n);
        if (!exists(candidate)) return candidate;
    }
    return clean;
}

// Läser en preset. Börjar från standardvärden; giltiga rader skriver över.
// Returnerar false om texten inte är en TruePan-preset.
inline bool presetFromText(const std::string& text, float* v, std::string* name)
{
    const bool v11 = text.compare(0, std::strlen(kPresetHeaderV11), kPresetHeaderV11) == 0;
    if (!v11 && text.compare(0, std::strlen(kPresetHeader), kPresetHeader) != 0) return false;
    for (int i = 0; i < kNumParams; ++i) v[i] = defaultParam(i);
    if (name) *name = "Untitled";
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find_first_of("\r\n", pos);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        if (key == "Name") { if (name) *name = cleanPresetName(val); continue; }
        for (int i = 0; i < kNumParams; ++i) {
            if (key != kParamNames[i]) continue;
            char* endp = nullptr;
            const double d = std::strtod(val.c_str(), &endp);
            if (endp != val.c_str() && d >= 0.0 && d <= 1.0) {               // ogiltigt ignoreras
                v[i] = (float)d;
                if (v11 && i == kParamLesSpeed) v[i] = lesSpeedNormFromV11((float)d);
            }
        }
    }
    return true;
}

} // namespace truepan
