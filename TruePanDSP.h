// TruePanDSP.h - ljudmotorn i TruePan (panorering), utan Windows-beroenden
// så att den kan testas automatiskt på vilken dator som helst.
//
// Signalväg per sampel:
//   in L/R -> [LOW MONO: delning i bas (mono, i mitten) och resten]
//          -> WIDTH (mid/side) -> MODE (BALANCE eller STEREO)
//          -> panorering med vald PAN LAW (PAN + AUTO-PAN)
//          -> basen läggs tillbaka i mitten -> OUTPUT
//
// Grundregel: med standardvärden (PAN i mitten, WIDTH 100 %, LOW MONO av,
// AUTO-PAN 0 %, OUTPUT 0 dB) går ljudet igenom oförändrat.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstring>

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
    kNumParams
};

// Högst 7 tecken (Live 9 visar inte fler).
static const char* const kParamNames[kNumParams] = {
    "Pan", "Width", "Mode", "Law", "LoMono", "APDepth", "APRate", "APShape", "Output"
};
static const char* const kParamLabels[kNumParams] = {
    "", "%", "", "dB", "Hz", "%", "", "", "dB"
};

// --- Val (knappar/väljare) ------------------------------------------------
enum { kModeBalance = 0, kModeStereo = 1 };
enum { kLaw0 = 0, kLaw3, kLaw45, kLaw6, kNumLaws };
enum { kShapeSine = 0, kShapeTriangle, kShapeSquare, kNumShapes };
const int kNumRates = 12;

static const char* const kLawNames[kNumLaws] = { "0.0", "-3.0", "-4.5", "-6.0" };
static const char* const kShapeNames[kNumShapes] = { "Sine", "Tri", "Square" };
static const char* const kShapeLongNames[kNumShapes] = { "SINE", "TRIANGLE", "SQUARE" };
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
        reset();
    }

    void reset()
    {
        for (int i = 0; i < 2; ++i) { hpL_[i].reset(); hpR_[i].reset(); }
        lpM_.reset();
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
    }

    void setMonoCheck(bool on) { monoCheck_ = on; }

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
        double pkL = 0.0, pkR = 0.0;
        double pos = pan_;

        for (int i = 0; i < n; ++i) {
            const double l = inL ? inL[i] : 0.0;
            const double r = inR ? inR[i] : 0.0;

            // Position: PAN + AUTO-PAN
            pos = pan_;
            if (depth_ > 0.0) pos = clampd(pan_ + depth_ * lfoValue(shape_, phase_), -1.0, 1.0);
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
                snap_ = false;
            } else {
                gl_ += (tgl - gl_) * smoothCoef_;
                gr_ += (tgr - gr_) * smoothCoef_;
                side_ += (tside - side_) * smoothCoef_;
                out_ += (outGain_ - out_) * smoothCoef_;
                low_ += (tlow - low_) * fadeCoef_;
                if (std::fabs(low_ - tlow) < 1e-6) low_ = tlow;
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

            // Panorering + bas i mitten + OUTPUT
            double ol = (yl * gl_ + bass) * out_;
            double orr = (yr * gr_ + bass) * out_;
            if (monoCheck_) { const double m = (ol + orr) * 0.5; ol = orr = m; }

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

private:
    double sr_ = 44100.0;
    double smoothCoef_ = 0.0, fadeCoef_ = 0.0;

    // parametrar
    double pan_ = 0.0, width_ = 1.0, depth_ = 0.0, outGain_ = 1.0, lowHz_ = 120.0;
    int mode_ = kModeStereo, law_ = kLaw3, rate_ = 5, shape_ = kShapeSine;
    bool lowOn_ = false, monoCheck_ = false;

    // utjämnade värden
    double gl_ = 1.0, gr_ = 1.0, side_ = 1.0, out_ = 1.0, low_ = 0.0;
    bool snap_ = true;

    // AUTO-PAN
    double tempo_ = 120.0, phase_ = 0.0;
    bool synced_ = false;

    // LOW MONO
    Biquad hpL_[2], hpR_[2];
    LR4 lpM_;
    double filterHz_ = -1.0;

    // visning
    double displayPos_ = 0.0, peakL_ = 0.0, peakR_ = 0.0;
};

// ---------------------------------------------------------------------------
// Fabrikspresets (syns som program i Lives presetlista och i fönstret)
// ---------------------------------------------------------------------------
const int kNumFactoryPresets = 10;

inline const char* factoryPresetName(int i)
{
    static const char* const names[kNumFactoryPresets] = {
        "Center", "Hard Left", "Hard Right", "Wide Stereo", "Mono",
        "Auto-Pan 1/4", "Auto-Pan 1/8 Tri", "Ping-Pong 1/16", "Slow Sweep 2 Bars", "Bass Mono 120"
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

} // namespace truepan
