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
    kParamLesModel,
    kParamHead,
    kParamHeadSize,
    kParamDistance,
    kNumParams
};

// Högst 7 tecken (Live 9 visar inte fler).
static const char* const kParamNames[kNumParams] = {
    "Pan", "Width", "Mode", "Law", "LoMono", "APDepth", "APRate", "APShape", "Output",
    "Monitor", "APMode", "LesSpd", "LesMod", "Head", "HeadSz", "Dist"
};
static const char* const kParamLabels[kNumParams] = {
    "", "%", "", "dB", "Hz", "%", "", "", "dB", "", "", "", "", "", "cm", "m"
};

// --- Val (knappar/väljare) ------------------------------------------------
enum { kModeBalance = 0, kModeStereo = 1 };
enum { kLaw0 = 0, kLaw3, kLaw45, kLaw6, kNumLaws };
enum { kShapeSine = 0, kShapeTriangle, kShapeSquare, kShapeParadiddle, kShapeParaVariant,
       kShapeDoubleStroke, kShapeParaDiddleDiddle, kShapeInvParadiddle, kShapeSixStroke, kShapeFlam, kNumShapes };
const int kNumShapesV16 = 3;   // t.o.m. v1.7: SINE, TRIANGLE, SQUARE
const int kNumShapesV18 = 4;   // v1.8: + PARADIDDLE (som nu heter PARAVARIANT)
const int kNumShapesV19 = 5;   // v1.9: + PARADIDDLE med separata slag
const int kNumRates = 12;
enum { kMonStereo = 0, kMonMono, kMonMid, kMonSides, kNumMonitors };
enum { kFxPan = 0, kFxTremolo, kFxLeslie, kNumFxModes };
// LESLIE SPEED: STOP, CHORALE och TREMOLO har originalets fart för vald modell
// (oberoende av tempot). Därefter tempolägen: ett varv för hornet per notlängd,
// från långsamt (1/2) till snabbt (1/32), som RATE.
enum { kLesStop = 0, kLesChorale, kLesTremolo, kLesSyncFirst };
const int kNumLesSync = 8;
const int kNumLesSpeeds = kLesSyncFirst + kNumLesSync;   // 11
static const char* const kLesSpeedNames[kNumLesSpeeds] = {
    "Stop", "Chorale", "Tremolo", "1/2", "1/4", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32"
};
static const char* const kLesSpeedLongNames[kNumLesSpeeds] = {
    "STOP", "CHORALE", "TREMOLO", "1/2", "1/4", "1/6", "1/8", "1/12", "1/16", "1/24", "1/32"
};
static const double kLesSyncBeats[kNumLesSync] = { 2.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

// LESLIE-modeller. Farter i varv per sekund, ramptider i sekunder (tid att
// nå nya farten). 122/147: uppmätta värden (horn 48/400 varv/min, trumma
// 40/342 varv/min; horn ca 0,9 s upp / 1,0 s ner, trumma ca 5,5 s / 4,8 s).
// 145: samma motorer och farter, mindre kabinett. VIBRATONE (Fender/Leslie 16):
// en enda roterande trumma, ca 40/340 varv/min.
enum { kModel122 = 0, kModel145, kModelVibratone, kNumLesModels };
struct LesModelSpec
{
    double hornSlow, hornFast, drumSlow, drumFast;
    double hornUp, hornDown, drumUp, drumDown;
};
static const LesModelSpec kLesModels[kNumLesModels] = {
    { 48.0 / 60.0, 400.0 / 60.0, 40.0 / 60.0, 342.0 / 60.0, 0.9, 1.0, 5.5, 4.8 },   // 122 / 147
    { 48.0 / 60.0, 400.0 / 60.0, 40.0 / 60.0, 342.0 / 60.0, 0.9, 1.0, 5.0, 4.5 },   // 145
    { 40.0 / 60.0, 340.0 / 60.0, 40.0 / 60.0, 340.0 / 60.0, 1.6, 2.0, 1.6, 2.0 },   // Vibratone: en lätt rotor
};
static const char* const kLesModelNames[kNumLesModels] = { "122", "145", "Vibra" };
static const char* const kLesModelLongNames[kNumLesModels] = { "MOD 122", "MOD 145", "VIBRATONE" };

// Äldre versioner: v1.1 SLOW/FAST, v1.2-1.3 åtta tempolägen
inline float lesSpeedNormFromV11(float old)
{
    return (float)(old >= 0.5f ? kLesTremolo : kLesChorale) / (float)(kNumLesSpeeds - 1);
}
inline float lesSpeedNormFromV12(float old)
{
    int i = (int)std::floor(old * (kNumLesSync - 1) + 0.5f);
    i = i < 0 ? 0 : (i >= kNumLesSync ? kNumLesSync - 1 : i);
    return (float)(kLesSyncFirst + i) / (float)(kNumLesSpeeds - 1);
}

static const char* const kLawNames[kNumLaws] = { "0.0", "-3.0", "-4.5", "-6.0" };
static const char* const kShapeNames[kNumShapes] = { "Sine", "Tri", "Square", "Paradid", "DidTie",
                                                     "DblStrk", "PDDidl", "InvPara", "SixStrk", "Flam" };
static const char* const kShapeLongNames[kNumShapes] = { "SINE", "TRIANGLE", "SQUARE", "PARADIDDLE", "DIDDLETIE",
                                                         "DOUBLE STROKE", "PARADIDDLE-DIDDLE", "INV. PARADIDDLE", "SIX STROKE ROLL", "FLAM" };

// Rudiment: varje slag är en puls på en RATE-period (första halvan ute på
// sidan, andra halvan i mitten), precis som PARADIDDLE. +1 = höger, -1 = vänster;
// 0,6 = obetonat slag (närmare mitten / svagare i TREM), 1 = betonat.
struct Rudiment { int len; double v[8]; };
static const Rudiment kRudDoubleStroke = { 4, { 1, 1, -1, -1 } };                       // R R L L
static const Rudiment kRudParaDiddleDiddle = { 6, { 1, -1, 1, 1, -1, -1 } };            // R L R R L L
static const Rudiment kRudInvParadiddle = { 8, { 1, -1, -1, 1, -1, 1, 1, -1 } };        // R L L R L R R L
static const Rudiment kRudSixStroke = { 6, { 1, -0.6, -0.6, 0.6, 0.6, -1 } };          // >R l l r r >L
// FLAM: förslag (svagt slag på motsatt sida) strax före huvudslaget: lR rL.
// Förslaget tar de första 12 % av perioden (30 ms vid 1/8 och 120 bpm).
const double kFlamGrace = 0.12, kFlamGraceLevel = 0.6;
// Mönstret R L R R L R L L
static const double kParadiddle[8] = { 1.0, -1.0, 1.0, 1.0, -1.0, 1.0, -1.0, -1.0 };
// PARADIDDLE: varje slag är en ren SQUARE-puls på en RATE-period: första halvan
// ute på sidan, andra halvan tillbaka i mitten. R-R hörs alltså som två slag.
// Hela mönstret tar 8 RATE-perioder (RATE 1/16 = sextondelsslag).
// DIDDLETIE (hette PARAVARIANT före v2.6): varje slag är en halv RATE-period och ljudet stannar på sidan
// (R-R blir ett längre slag). Hela mönstret tar 4 RATE-perioder.
const double kLfoCycle = 24.0;  // AUTO-PAN-fasen räknas över 24 perioder (delbart med 2, 4, 6 och 8 slag)

// TREM: händernas karaktär. Vänster hand lite mörkare, höger lite ljusare
// (diskanten över ca 1,2 kHz -/+ 3 dB vid DEPTH 100 %), och obetonade slag
// -4 dB, så att mönstret hörs fast ljudet står i mitten.
const double kHandTiltDb = 3.0, kHandTiltHz = 1200.0, kUnaccentDb = -4.0;
// Betonade slag (första slaget i varje grupp), ett värde per slag
static const bool kAccParadiddle[8] = { true, false, false, false, true, false, false, false };   // >R l r r >L r l l
static const bool kAccDoubleStroke[4] = { true, false, true, false };                              // >R r >L l
static const bool kAccParaDiddleDiddle[6] = { true, false, false, false, false, false };          // >R l r r l l
static const bool kAccInvParadiddle[8] = { true, false, false, false, true, false, false, false }; // >R l l r >L r r l

// Är PARADIDDLE-slaget "på" just nu (första halvan av perioden)?
inline bool paradiddleOn(double phase) { return phase - std::floor(phase) < 0.5; }

// PAN-steg (piltangenterna och skalan i STEREO FIELD), i Ableton-enheter 50L..50R
const int kNumPanSteps = 9;
static const int kPanSteps[kNumPanSteps] = { -50, -36, -22, -10, 0, 10, 22, 36, 50 };
inline float panStepNorm(int units) { return (float)((units / 50.0 + 1.0) * 0.5); }
// Nästa steg åt vänster (dir < 0) eller höger (dir > 0) från ett godtyckligt läge
inline float panNextStep(float norm, int dir)
{
    const double u = (norm * 2.0 - 1.0) * 50.0;
    if (dir > 0) { for (int i = 0; i < kNumPanSteps; ++i) if (kPanSteps[i] > u + 0.25) return panStepNorm(kPanSteps[i]); return panStepNorm(50); }
    for (int i = kNumPanSteps - 1; i >= 0; --i) if (kPanSteps[i] < u - 0.25) return panStepNorm(kPanSteps[i]);
    return panStepNorm(-50);
}

// Äldre projekt/presets hade 3 eller 4 former: räkna om rattläget
inline float shapeNormFromOld(float old, int oldCount)
{
    int i = (int)std::floor(old * (oldCount - 1) + 0.5f);
    i = i < 0 ? 0 : (i >= oldCount ? oldCount - 1 : i);
    if (oldCount == kNumShapesV18 && i == 3) i = kShapeParaVariant;   // v1.8:s PARADIDDLE = PARAVARIANT
    // v1.9 (5 former): samma ordning, bara fler former efter
    return (float)i / (float)(kNumShapes - 1);
}
inline float shapeNormFromV16(float old) { return shapeNormFromOld(old, kNumShapesV16); }
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
inline int lesModelFromNorm(float n) { return choiceFromNorm(n, kNumLesModels); }

// HEAD: huvudstorlek som effektiv huvuddiameter 12-30 cm. Standard 17,5 cm =
// forskningens effektiva huvudradie 8,75 cm (Woodworth; Brown & Duda), som ger
// den uppmätta största tidsskillnaden ca 0,66 ms. Större = överdriven effekt.
const double kHeadMinCm = 12.0, kHeadMaxCm = 30.0, kHeadStdCm = 17.5;
inline bool headOn(float n) { return n >= 0.5f; }
inline double headSizeCm(float n) { return kHeadMinCm + clampd(n, 0.0f, 1.0f) * (kHeadMaxCm - kHeadMinCm); }
inline float headSizeNorm(double cm) { return (float)clampd((cm - kHeadMinCm) / (kHeadMaxCm - kHeadMinCm), 0.0, 1.0); }

// DISTANCE: 0,25 - 100 m. 1 m = referens (oförändrat ljud) = klockan 9 på ratten
// (1/6 av vridningen). Före: 0,25 - 1 m (närmare), efter: 1 - 100 m, båda logaritmiska.
const double kDistMin = 0.25, kDistMax = 100.0, kDistRef = 1.0;
const float kDistRefNorm = 1.0f / 6.0f;   // klockan 9 (ratten går från ca 7.30 till 4.30)
inline double distanceM(float n)
{
    const double x = clampd(n, 0.0f, 1.0f), z = kDistRefNorm;
    if (x < z) return kDistRef * std::pow(kDistMin / kDistRef, (z - x) / z);
    return kDistRef * std::pow(kDistMax / kDistRef, (x - z) / (1.0 - z));
}
inline float distanceNorm(double m)
{
    m = clampd(m, kDistMin, kDistMax);
    const double z = kDistRefNorm;
    if (m < kDistRef) return (float)(z - z * std::log(m / kDistRef) / std::log(kDistMin / kDistRef));
    return (float)(z + (1.0 - z) * std::log(m / kDistRef) / std::log(kDistMax / kDistRef));
}
// Nivåhöjningen (dB) när ljudet är närmare än 1 m (avståndslagen); 0 för 1 m och längre bort
inline double distanceNearBoostDb(float n)
{
    const double m = distanceM(n);
    return m < kDistRef ? 20.0 * std::log10(kDistRef / m) : 0.0;
}
// v2.2: 1 m i mitten av ratten
inline float distanceNormFromV22(float old)
{
    const double x = clampd(old, 0.0f, 1.0f);
    const double m = x < 0.5 ? std::pow(kDistMin, (0.5 - x) * 2.0) : std::pow(kDistMax, (x - 0.5) * 2.0);
    return distanceNorm(m);
}
// v1.7 - v2.1.1: ratten var en enda log-skala 0,25 - 100 m (1 m = 0,23)
inline float distanceNormFromV17(float old)
{
    return distanceNorm(kDistMin * std::pow(kDistMax / kDistMin, clampd(old, 0.0f, 1.0f)));
}

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
    case kParamLesSpeed: return choiceNorm(kLesChorale, kNumLesSpeeds);
    case kParamLesModel: return 0.0f;                           // 122
    case kParamHead:    return 0.0f;                            // av (äldre projekt låter som förut)
    case kParamHeadSize: return headSizeNorm(kHeadStdCm);
    case kParamDistance: return distanceNorm(kDistRef);
    default:            return 0.0f;
    }
}

// PAN som text: "C", "18L", "50R" (Ableton-stil)
inline void panText(double pan, char* out, size_t max, bool /*spaced*/)
{
    // Som i Ableton Live: 50L ... C ... 50R
    const int units = (int)std::floor(std::fabs(pan) * 50.0 + 0.5);
    if (units == 0) std::snprintf(out, max, "C");
    else std::snprintf(out, max, "%d%c", units, pan < 0 ? 'L' : 'R');
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
    case kParamLesModel: std::snprintf(out, max, "%s", kLesModelNames[lesModelFromNorm(n)]); break;
    case kParamHead:    std::snprintf(out, max, "%s", headOn(n) ? "On" : "Off"); break;
    case kParamHeadSize: std::snprintf(out, max, "%.1f", headSizeCm(n)); break;
    case kParamDistance: {
        const double m = distanceM(n);
        std::snprintf(out, max, m < 10.0 ? "%.2f" : (m < 99.95 ? "%.1f" : "%.0f"), m);
        break;
    }
    default:            std::snprintf(out, max, "-"); break;
    }
}

// Text -> parametervärde (för VST3, där man kan skriva in ett värde).
// Klarar samma texter som paramDisplayText ger, plus vanliga varianter.
inline bool textEqualNoCase(const char* a, const char* b)
{
    for (; *a && *b; ++a, ++b) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return false;
    }
    return *a == 0 && *b == 0;
}
inline int paramStepCount(int index)   // antal val (0 = steglös)
{
    switch (index) {
    case kParamMode:     return 2;
    case kParamLaw:      return kNumLaws;
    case kParamRate:     return kNumRates;
    case kParamShape:    return kNumShapes;
    case kParamMonitor:  return kNumMonitors;
    case kParamFxMode:   return kNumFxModes;
    case kParamLesSpeed: return kNumLesSpeeds;
    case kParamLesModel: return kNumLesModels;
    case kParamHead:     return 2;
    default:             return 0;
    }
}
inline bool paramFromText(int index, const char* text, float& out)
{
    if (!text || index < 0 || index >= kNumParams) return false;
    while (*text == ' ') ++text;
    const int steps = paramStepCount(index);
    if (steps > 0) {
        char buf[32];
        for (int i = 0; i < steps; ++i) {
            const float n = choiceNorm(i, steps);
            paramDisplayText(index, n, buf, sizeof buf);
            if (textEqualNoCase(buf, text)) { out = n; return true; }
        }
        return false;
    }
    if (index == kParamLowMono && (textEqualNoCase(text, "off") || textEqualNoCase(text, "0"))) { out = 0.0f; return true; }
    if (index == kParamPan && textEqualNoCase(text, "c")) { out = 0.5f; return true; }
    char* end = nullptr;
    double v = std::strtod(text, &end);
    if (end == text || !std::isfinite(v)) return false;
    while (end && *end == ' ') ++end;
    switch (index) {
    case kParamPan: {
        // "25L", "25R", eller -25 / 25 (minus = vänster); 50 = helt åt sidan
        if (end && (*end == 'L' || *end == 'l')) v = -std::fabs(v);
        else if (end && (*end == 'R' || *end == 'r')) v = std::fabs(v);
        out = panNorm(clampd(v / 50.0, -1.0, 1.0));
        return true;
    }
    case kParamWidth:    out = widthNorm(v / 100.0); return true;
    case kParamLowMono:  out = lowMonoNorm(clampd(v, kLowMonoMinHz, kLowMonoMaxHz)); return true;
    case kParamDepth:    out = (float)clampd(v / 100.0, 0.0, 1.0); return true;
    case kParamOutput:   out = outputDbToNorm(v); return true;
    case kParamHeadSize: out = headSizeNorm(v); return true;
    case kParamDistance: out = distanceNorm(clampd(v, kDistMin, kDistMax)); return true;
    default:             return false;
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

// Är slaget betonat? (TREM: obetonade slag spelas svagare.) Former utan egen
// betoning räknas som betonade; SIX STROKE och FLAM har redan betoning i nivån.
inline bool strokeAccented(int shape, double phase)
{
    const bool* acc = nullptr;
    int len = 0;
    switch (shape) {
    case kShapeParadiddle:       acc = kAccParadiddle; len = 8; break;
    case kShapeDoubleStroke:     acc = kAccDoubleStroke; len = 4; break;
    case kShapeParaDiddleDiddle: acc = kAccParaDiddleDiddle; len = 6; break;
    case kShapeInvParadiddle:    acc = kAccInvParadiddle; len = 8; break;
    default: return true;
    }
    double p = phase / len;
    p -= std::floor(p);
    int step = (int)(p * len);
    if (step >= len) step = len - 1;
    return acc[step];
}

// AUTO-PAN-vågform -> -1..+1 (börjar i mitten och går åt höger).
// phase räknas i perioder; PARADIDDLE använder 4 perioder (0..4).
inline double lfoValue(int shape, double phase)
{
    if (shape == kShapeParadiddle) {
        double p = phase / 8.0;
        p -= std::floor(p);
        int step = (int)(p * 8.0);
        if (step > 7) step = 7;
        return paradiddleOn(phase) ? kParadiddle[step] : 0.0;   // andra halvan: mitten
    }
    if (shape >= kShapeDoubleStroke && shape <= kShapeSixStroke) {
        const Rudiment& r = shape == kShapeDoubleStroke ? kRudDoubleStroke
                          : shape == kShapeParaDiddleDiddle ? kRudParaDiddleDiddle
                          : shape == kShapeInvParadiddle ? kRudInvParadiddle : kRudSixStroke;
        double p = phase / r.len;
        p -= std::floor(p);
        int step = (int)(p * r.len);
        if (step >= r.len) step = r.len - 1;
        return paradiddleOn(phase) ? r.v[step] : 0.0;
    }
    if (shape == kShapeFlam) {
        double p = phase / 2.0;
        p -= std::floor(p);
        const double side = p < 0.5 ? 1.0 : -1.0;   // lR, sedan rL
        const double f = phase - std::floor(phase);
        if (f < kFlamGrace) return -side * kFlamGraceLevel;
        return f < 0.5 ? side : 0.0;
    }
    if (shape == kShapeParaVariant) {
        double p = phase / 4.0;
        p -= std::floor(p);
        int step = (int)(p * 8.0);
        if (step > 7) step = 7;
        return kParadiddle[step];
    }
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
// LESLIE: roterande högtalare i ett rum. 122 och 145: ljudet delas vid 800 Hz i
// bastrumma och diskanthorn som snurrar med var sin fart och tröghet.
// VIBRATONE: hela ljudet (gitarrområdet) går genom en enda roterande trumma.
// Två mikrofoner står på var sin sida (vänster och höger): när rotorn pekar
// mot vänster hörs den starkt och nära där, medan höger mest hör reflexen från
// rummets vägg. Ljudet snurrar alltså runt i stereobilden, med dopplereffekt.
//
// Riktverkan (psykoakustik): hornet strålar diskanten som en ficklampa. När det
// pekar bort från en mikrofon försvinner de höga frekvenserna mer än de låga, så
// ljudet blir dovt, inte bara svagare. Brytfrekvensen följer riktningen på en
// logaritmisk skala (som örat uppfattar tonhöjd) och stannar högt en bit åt
// sidan, för att sedan falla mot baksidan. Eftersom diskantförlusten i sig sänker
// den upplevda styrkan (örat är känsligast vid 2-5 kHz) är nivåsvajet minskat
// något, så att den totala upplevda styrkevariationen blir som förut.
// ---------------------------------------------------------------------------
class Leslie
{
public:
    static const int kBufSize = 16384;   // räcker till 768 kHz (högst ca 14 ms fördröjning)

    // Riktverkan per rotortyp: brytfrekvens rakt mot mikrofonen, rakt åt
    // sidan och rakt bort (Hz), samt nivåkompensation för filtret.
    struct Directivity { double front, side, back, makeup; };
    struct Tone { double z1 = 0.0, z2 = 0.0; };   // tvåpoligt lågpass
    static Directivity dirHorn() { return { 16000.0, 7000.0, 2200.0, 1.08 }; }   // tydlig ficklampa
    static Directivity dirDrum() { return { 4000.0, 1500.0, 450.0, 1.03 }; }     // bastrumma: mild
    static Directivity dirVib()  { return { 12000.0, 6000.0, 2800.0, 1.06 }; }   // frigolitrotorn sprider mer

    Leslie() : horn_(kBufSize, 0.0), drum_(kBufSize, 0.0) {}

    void prepare(double sr)
    {
        sr_ = sr;
        lpDrum_.set(false, 800.0, sr); hpHorn_.set(true, 800.0, sr);
        vibHp_.set(true, 90.0, sr); vibLp_.set(false, 5000.0, sr);
        reflLp_ = 1.0 - std::exp(-2.0 * kPi * 5000.0 / sr);  // väggen tar bort lite diskant
        setModel(model_);
        reset();
    }
    void reset()
    {
        lpDrum_.reset(); hpHorn_.reset(); vibHp_.reset(); vibLp_.reset();
        std::fill(horn_.begin(), horn_.end(), 0.0);
        std::fill(drum_.begin(), drum_.end(), 0.0);
        reflZ_[0] = reflZ_[1] = 0.0;
        for (int i = 0; i < 2; ++i) { hornTone_[i] = Tone(); drumTone_[i] = Tone(); }
        w_ = 0;
    }
    void setModel(int m)
    {
        model_ = (m >= 0 && m < kNumLesModels) ? m : kModel122;
        const LesModelSpec& sp = kLesModels[model_];
        // Exponentiell inbromsning/uppvarvning: ramptiden = ca 3 tidskonstanter (95 %)
        hornUpC_ = coef(sp.hornUp); hornDnC_ = coef(sp.hornDown);
        drumUpC_ = coef(sp.drumUp); drumDnC_ = coef(sp.drumDown);
    }
    void setTargets(double hornHz, double drumHz)
    {
        hornT_ = clampd(hornHz, 0.0, 25.0);
        drumT_ = clampd(drumHz, 0.0, 25.0);
    }

    // Farten och vinklarna går alltid, även när effekten inte hörs,
    // så att den låter naturligt när man slår på den.
    void advance()
    {
        hornHz_ += (hornT_ - hornHz_) * (hornT_ > hornHz_ ? hornUpC_ : hornDnC_);
        drumHz_ += (drumT_ - drumHz_) * (drumT_ > drumHz_ ? drumUpC_ : drumDnC_);
        if (hornT_ == 0.0 && hornHz_ < 0.01) hornHz_ = 0.0;   // STOP: står helt still
        if (drumT_ == 0.0 && drumHz_ < 0.01) drumHz_ = 0.0;
        hornAng_ += 2.0 * kPi * hornHz_ / sr_;
        drumAng_ += 2.0 * kPi * drumHz_ / sr_;
        if (hornAng_ > 2.0 * kPi) hornAng_ -= 2.0 * kPi;
        if (drumAng_ > 2.0 * kPi) drumAng_ -= 2.0 * kPi;
    }

    void process(double in, double& outL, double& outR)
    {
        const double micL = 0.5 * kPi, micR = -0.5 * kPi;   // mikrofonerna på motsatta sidor
        if (model_ == kModelVibratone) {
            // En rotor (frigolittrumma framför en 10-tumshögtalare), gitarrområdet
            drum_[w_] = vibLp_.process(vibHp_.process(in));
            horn_[w_] = 0.0;
            const double l = rotor(drum_, drumAng_ - micL, 0.0004, 0.65, dirVib(), drumTone_[0]);
            const double r = rotor(drum_, drumAng_ - micR, 0.0004, 0.65, dirVib(), drumTone_[1]);
            const double rl = wallOne(drum_, drumAng_ - micL, 0.0061);
            const double rr = wallOne(drum_, drumAng_ - micR, 0.0089);
            reflZ_[0] += (rl - reflZ_[0]) * reflLp_;
            reflZ_[1] += (rr - reflZ_[1]) * reflLp_;
            outL = (l + 0.5 * room_ * reflZ_[0]) * kNormVib;
            outR = (r + 0.5 * room_ * reflZ_[1]) * kNormVib;
        } else {
            horn_[w_] = hpHorn_.process(in);
            drum_[w_] = lpDrum_.process(in);
            const bool small = model_ == kModel145;
            const double l = rotor(horn_, hornAng_ - micL, 0.0005, 0.7, dirHorn(), hornTone_[0])
                           + rotor(drum_, drumAng_ - micL, 0.0002, small ? 0.4 : 0.45, dirDrum(), drumTone_[0]);
            const double r = rotor(horn_, hornAng_ - micR, 0.0005, 0.7, dirHorn(), hornTone_[1])
                           + rotor(drum_, drumAng_ - micR, 0.0002, small ? 0.4 : 0.45, dirDrum(), drumTone_[1]);
            // Rummet: när rotorn pekar bort från en mikrofon hörs ljudet via väggen
            // bakom. 145 är mindre och står oftast närmare väggen: kortare, svagare reflex.
            const double rl = wall(hornAng_ - micL, drumAng_ - micL, small ? 0.0049 : 0.0073);
            const double rr = wall(hornAng_ - micR, drumAng_ - micR, small ? 0.0071 : 0.0111);
            reflZ_[0] += (rl - reflZ_[0]) * reflLp_;
            reflZ_[1] += (rr - reflZ_[1]) * reflLp_;
            const double rg = (small ? 0.42 : 0.55) * room_;
            outL = (l + rg * reflZ_[0]) * kNorm;
            outR = (r + rg * reflZ_[1]) * kNorm;
        }
        w_ = (w_ + 1) & (kBufSize - 1);
    }

    void setRoom(double amount) { room_ = clampd(amount, 0.0, 1.0); }   // 0 = bara mikrofonerna (för tester)
    double hornHz() const { return hornHz_; }
    double drumHz() const { return drumHz_; }
    int model() const { return model_; }

private:
    // Nivå: Leslien ger brett, okorrelerat ljud som upplevs svagare än samma
    // effekt i mitten. Därför ligger den ca 1,5 dB över torrt ljud i effekt,
    // så att den upplevs lika stark som PAN-läget.
    static constexpr double kNorm = 1.12;   // inkl. kompensation för att riktverkan tar bort diskant
    static constexpr double kNormVib = 1.12;

    double coef(double rampSec) const { return 1.0 - std::exp(-3.0 / (rampSec * sr_)); }

    // Brytfrekvensen följer riktningen på log-skala: c = 1 (mot), 0 (sidan), -1 (bort)
    static double toneHz(const Directivity& d, double c)
    {
        const double lf = std::log(d.front), ls = std::log(d.side), lb = std::log(d.back);
        const double l = c >= 0.0 ? ls + (lf - ls) * c : ls + (ls - lb) * c;
        return std::exp(l);
    }

    // Rotorn sedd från en mikrofon: avståndet ändras (fördröjning = doppler),
    // nivån är högst och klangen ljusast när rotorn pekar mot mikrofonen.
    double rotor(const std::vector<double>& buf, double ang, double depthSec, double amDepth,
                 const Directivity& dir, Tone& tone)
    {
        const double c = std::cos(ang);
        const double delay = (0.0012 + depthSec * (1.0 - c)) * sr_;
        double x = read(buf, delay) * (1.0 + amDepth * c);
        // Tvåpoligt lågpass (12 dB/oktav), koefficient från brytfrekvensen
        const double wc = 2.0 * kPi * clampd(toneHz(dir, c), 20.0, sr_ * 0.45) / sr_;
        const double a = wc / (1.0 + wc);
        tone.z1 += (x - tone.z1) * a;
        tone.z2 += (tone.z1 - tone.z2) * a;
        return tone.z2 * dir.makeup;
    }
    // Reflexen: starkast när rotorn pekar bort från mikrofonen (mot väggen)
    double wall(double hornAng, double drumAng, double extraSec) const
    {
        const double ch = -std::cos(hornAng), cd = -std::cos(drumAng);
        const double h = read(horn_, (0.0012 + extraSec + 0.0005 * (1.0 - ch)) * sr_) * (0.5 + 0.5 * ch);
        const double d = read(drum_, (0.0012 + extraSec + 0.0002 * (1.0 - cd)) * sr_) * (0.7 + 0.3 * cd);
        return h + d;
    }
    double wallOne(const std::vector<double>& buf, double ang, double extraSec) const
    {
        const double c = -std::cos(ang);
        return read(buf, (0.0012 + extraSec + 0.0004 * (1.0 - c)) * sr_) * (0.5 + 0.5 * c);
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
    Biquad vibHp_, vibLp_;
    std::vector<double> horn_, drum_;
    double reflLp_ = 0.5, reflZ_[2] = { 0.0, 0.0 }, room_ = 1.0;
    int w_ = 0, model_ = kModel122;
    double hornUpC_ = 0.0, hornDnC_ = 0.0, drumUpC_ = 0.0, drumDnC_ = 0.0;
    Tone hornTone_[2], drumTone_[2];   // riktverkan per rotor och mikrofon (0 = vänster, 1 = höger)
    double hornT_ = 48.0 / 60.0, drumT_ = 40.0 / 60.0;
    double hornHz_ = 48.0 / 60.0, drumHz_ = 40.0 / 60.0, hornAng_ = 0.0, drumAng_ = 1.0;
};

// ---------------------------------------------------------------------------
// DISTANCE
//  - Nivå: avståndslagen (sfärisk spridning), 1/r: -6 dB per fördubblat avstånd,
//    relativt 1 m.
//  - Luftens dämpning: ISO 9613-1 (atmosfärisk absorption), vid 20 grader C,
//    50 % relativ fuktighet och normalt lufttryck. Dämpningen i dB är
//    alpha(f) * (r - 1 m) och växer kraftigt med frekvensen (ca 4,7 dB/km vid
//    1 kHz, 105 dB/km vid 8 kHz, 365 dB/km vid 16 kHz).
//    Filtret: 4 bilinjära + 4 enkla förstapolsfilter vars styrka anpassas
//    (minsta kvadrat i dB) till ISO-kurvan för varje avstånd och samplingsfrekvens.
//  - Närfält (med HEAD): avståndslagen per öra. Nära huvudet är det närmaste
//    örat märkbart närmare källan än det bortre, så nivåskillnaden mellan öronen
//    växer även i basen (under ca 1 m).
// ---------------------------------------------------------------------------
inline double isoAirAbsorptionDbPerM(double f, double tempC = 20.0, double rh = 50.0, double pKPa = 101.325)
{
    const double pr = 101.325, T0 = 293.15, T01 = 273.16, T = tempC + 273.15;
    const double psat = pr * std::pow(10.0, -6.8346 * std::pow(T01 / T, 1.261) + 4.6151);
    const double h = rh * psat / pKPa;                                   // vattenånga, mol-%
    const double frO = pKPa / pr * (24.0 + 4.04e4 * h * (0.02 + h) / (0.391 + h));
    const double frN = pKPa / pr * std::pow(T / T0, -0.5) * (9.0 + 280.0 * h * std::exp(-4.170 * (std::pow(T / T0, -1.0 / 3.0) - 1.0)));
    const double f2 = f * f;
    return 8.686 * f2 * (1.84e-11 * (pr / pKPa) * std::sqrt(T / T0)
           + std::pow(T / T0, -2.5) * (0.01275 * std::exp(-2239.1 / T) / (frO + f2 / frO)
                                       + 0.1068 * std::exp(-3352.0 / T) / (frN + f2 / frN)));
}

// Filtrets dämpning (dB) vid frekvensen f för styrkorna K1 (bilinjära) och K2 (enkla)
const int kAirN1 = 4, kAirN2 = 4;
inline double airFilterDb(double k1, double k2, double f, double sr)
{
    const double t = std::tan(kPi * f / sr), sn = std::sin(kPi * f / sr);
    return 10.0 * kAirN1 * std::log10(1.0 + k1 * t * t) + 10.0 * kAirN2 * std::log10(1.0 + k2 * 4.0 * sn * sn);
}

// Tabell (per samplingsfrekvens) med filterstyrkor för olika avstånd
class AirTable
{
public:
    static const int kPoints = 48;

    void build(double sr)
    {
        sr_ = sr;
        // Frekvenser 250 Hz .. 16 kHz (högst 0,45 * fs)
        nf_ = 0;
        const double fmax = std::min(16000.0, 0.45 * sr);
        for (double f = 250.0; f <= fmax && nf_ < kMaxF; f *= 1.08) {
            const double t = std::tan(kPi * f / sr), sn = std::sin(kPi * f / sr);
            fT_[nf_] = t * t; fS_[nf_] = 4.0 * sn * sn; fA_[nf_] = isoAirAbsorptionDbPerM(f);
            ++nf_;
        }
        double k1 = -1.0, k2 = -1.0;
        for (int i = 0; i < kPoints; ++i) {
            const double x = xAt(i);
            fit(x, k1, k2);
            k1_[i] = k1; k2_[i] = k2;
        }
    }
    // x = avstånd - 1 m. Ger filterstyrkorna (0 = inget filter).
    void lookup(double x, double& k1, double& k2) const
    {
        if (x <= 0.0) { k1 = k2 = 0.0; return; }
        const double x0 = xAt(0);
        if (x <= x0) { k1 = k1_[0] * x / x0; k2 = k2_[0] * x / x0; return; }
        const double pos = std::log(x / x0) / std::log(xAt(kPoints - 1) / x0) * (kPoints - 1);
        int i = (int)pos;
        if (i >= kPoints - 1) { k1 = k1_[kPoints - 1]; k2 = k2_[kPoints - 1]; return; }
        const double fr = pos - i;
        k1 = std::exp(std::log(k1_[i]) * (1.0 - fr) + std::log(k1_[i + 1]) * fr);
        k2 = std::exp(std::log(k2_[i]) * (1.0 - fr) + std::log(k2_[i + 1]) * fr);
    }
    static double xAt(int i) { return 0.25 * std::pow((kDistMax - kDistRef) / 0.25, (double)i / (kPoints - 1)); }

private:
    static const int kMaxF = 64;

    double err(double x, double k1, double k2) const
    {
        double e = 0.0;
        for (int i = 0; i < nf_; ++i) {
            const double a = fA_[i] * x;
            if (a > 30.0) continue;                       // över 30 dB: hörs inte ändå
            const double m = 10.0 * kAirN1 * std::log10(1.0 + k1 * fT_[i]) + 10.0 * kAirN2 * std::log10(1.0 + k2 * fS_[i]);
            e += (m - a) * (m - a);
        }
        return e;
    }
    // Minsta kvadrat i dB (mönstersökning i log-skala), varmstart från förra punkten
    void fit(double x, double& k1, double& k2) const
    {
        if (k1 <= 0.0 || k2 <= 0.0) {
            double num = 0.0, den = 0.0;
            for (int i = 0; i < nf_; ++i) {
                const double b = 4.343 * (kAirN1 * fT_[i] + kAirN2 * fS_[i]);
                num += fA_[i] * x * b; den += b * b;
            }
            k1 = k2 = den > 0.0 ? std::max(num / den, 1e-12) : 1e-12;
        } else {
            k1 *= x / prevX_; k2 *= x / prevX_;
        }
        double e = err(x, k1, k2), step = 2.0;
        for (int it = 0; it < 400 && step > 1.0005; ++it) {
            bool better = false;
            const double c[4][2] = { { step, 1.0 }, { 1.0 / step, 1.0 }, { 1.0, step }, { 1.0, 1.0 / step } };
            for (int k = 0; k < 4; ++k) {
                const double e2 = err(x, k1 * c[k][0], k2 * c[k][1]);
                if (e2 < e) { e = e2; k1 *= c[k][0]; k2 *= c[k][1]; better = true; }
            }
            if (!better) step = std::sqrt(step);
        }
        prevX_ = x;
    }

    double sr_ = 44100.0;
    double fT_[kMaxF], fS_[kMaxF], fA_[kMaxF];
    int nf_ = 0;
    double k1_[kPoints], k2_[kPoints];
    mutable double prevX_ = 1.0;
};

// Luftdämpningsfiltret för en kanal
struct AirFilter
{
    struct Bil { double x1 = 0.0, y1 = 0.0; };
    Bil bil[kAirN1];
    double leak[kAirN2] = { 0.0, 0.0, 0.0, 0.0 };
    double b0 = 1.0, a1 = 0.0, c = 1.0;
    bool active = false;

    void set(double k1, double k2)
    {
        active = k1 > 1e-12 || k2 > 1e-12;
        const double k = std::sqrt(std::max(k1, 0.0));      // bilinjär förstapol: k = 2 fs / wc
        b0 = 1.0 / (1.0 + k);
        a1 = (1.0 - k) / (1.0 + k);
        c = k2 > 1e-12 ? (-1.0 + std::sqrt(1.0 + 4.0 * k2)) / (2.0 * k2) : 1.0;
    }
    void reset() { for (int i = 0; i < kAirN1; ++i) bil[i] = Bil(); for (int i = 0; i < kAirN2; ++i) leak[i] = 0.0; }
    double process(double x)
    {
        if (!active) {
            // följ signalen så att filtret kan slås på utan klick
            for (int i = 0; i < kAirN1; ++i) { bil[i].x1 = x; bil[i].y1 = x; }
            for (int i = 0; i < kAirN2; ++i) leak[i] = x;
            return x;
        }
        for (int i = 0; i < kAirN1; ++i) {
            const double y = b0 * (x + bil[i].x1) - a1 * bil[i].y1;
            bil[i].x1 = x; bil[i].y1 = y; x = y;
        }
        for (int i = 0; i < kAirN2; ++i) { leak[i] += (x - leak[i]) * c; x = leak[i]; }
        return x;
    }
};

// ---------------------------------------------------------------------------
// HEAD: sfärisk huvudmodell (psykoakustik).
//  - Tidsskillnad (ITD): Brown & Duda (1998) / Woodworth: per öra
//      T(theta) = -(a/c) cos(theta)        för 0 <= theta < 90 grader
//      T(theta) = (a/c)(theta - pi/2)      för 90 <= theta <= 180 grader
//    där theta är vinkeln mellan ljudkällan och örats riktning. Skillnaden
//    mellan öronen blir Woodworths ITD = (a/c)(az + sin az).
//  - Huvudskugga (ILD): Brown & Duda, första ordningens filter per öra
//      H(w, theta) = (1 + j alpha(theta) w/(2 w0)) / (1 + j w/(2 w0)),  w0 = c/a
//      alpha(theta) = (1 + amin/2) + (1 - amin/2) cos(theta/thetamin * 180 grader)
//    med amin = 0,1 och thetamin = 150 grader. Basen påverkas nästan inte (vågorna
//    böjer sig runt huvudet), diskanten dämpas mycket på bortre örat och höjs upp
//    till +6 dB på närmaste örat.
// Allt räknas relativt läget rakt fram, så PAN i mitten är helt oförändrat ljud.
// ---------------------------------------------------------------------------
const double kSoundSpeed = 343.0;     // m/s vid ca 20 grader C

inline double headAlpha(double theta)  // theta i radianer, 0..pi
{
    const double amin = 0.1, thetaMin = 150.0 * kPi / 180.0;
    return (1.0 + amin / 2.0) + (1.0 - amin / 2.0) * std::cos(theta / thetaMin * kPi);
}
inline double headEarDelay(double theta, double a)  // sekunder (kan vara negativ)
{
    theta = std::fabs(theta);
    if (theta < 0.5 * kPi) return -(a / kSoundSpeed) * std::cos(theta);
    return (a / kSoundSpeed) * (theta - 0.5 * kPi);
}
// Vinkeln mellan källan och vänster/höger öra. az = azimut (-pi/2 = vänster, +pi/2 = höger).
inline double earAngleLeft(double az)  { return std::fabs(az + 0.5 * kPi); }
inline double earAngleRight(double az) { return std::fabs(az - 0.5 * kPi); }
// Tidsskillnad mellan öronen (s), positiv = höger öra först
inline double headItd(double az, double a)
{
    return headEarDelay(earAngleLeft(az), a) - headEarDelay(earAngleRight(az), a);
}
// Örats nivå (dB) vid frekvensen hz, relativt läget rakt fram (PAN i mitten)
inline double headEarGainDb(double az, bool rightEar, double hz, double a)
{
    const double w = 2.0 * kPi * hz, w0 = kSoundSpeed / a;
    const double th = rightEar ? earAngleRight(az) : earAngleLeft(az);
    auto mag = [&](double alpha) {
        const double x = w / (2.0 * w0);
        return std::sqrt((1.0 + alpha * alpha * x * x) / (1.0 + x * x));
    };
    return 20.0 * std::log10(mag(headAlpha(th)) / mag(headAlpha(0.5 * kPi)));
}

class HeadModel
{
public:
    static const int kBufSize = 4096;    // räcker till 768 kHz och 30 cm huvud (ca 1,1 ms)

    HeadModel() : bufL_(kBufSize, 0.0), bufR_(kBufSize, 0.0) {}

    void prepare(double sr, double a)
    {
        sr_ = sr;
        setRadius(a);
        reset();
    }
    void setRadius(double a)
    {
        a_ = clampd(a, 0.05, 0.16);
        w0_ = kSoundSpeed / a_;
        alpha0_ = headAlpha(0.5 * kPi);   // läget rakt fram (normering)
        // Omvänt filter för läget rakt fram: (2w0 + s)/(2w0 + alpha0 s), bilinjär
        const double fs = sr_;
        const double d0 = w0_ + alpha0_ * fs;
        inv_.b0 = (w0_ + fs) / d0;
        inv_.b1 = (w0_ - fs) / d0;
        inv_.a1 = (w0_ - alpha0_ * fs) / d0;
    }
    void reset()
    {
        std::fill(bufL_.begin(), bufL_.end(), 0.0);
        std::fill(bufR_.begin(), bufR_.end(), 0.0);
        earL_ = Ear(); earR_ = Ear();
        w_ = 0;
    }
    double radius() const { return a_; }
    void setDistance(double r) { r_ = r > a_ * 1.5 ? r : a_ * 1.5; }

    // Närfält: avståndslagen per öra, relativt läget rakt fram (lika långt till båda)
    static double nearFieldGain(double az, bool rightEar, double r, double a)
    {
        const double s = std::sin(az) * (rightEar ? 1.0 : -1.0);
        const double d = std::sqrt(r * r + a * a - 2.0 * r * a * s);
        return std::sqrt(r * r + a * a) / d;
    }

    // pos = -1 (vänster) .. +1 (höger) -> azimut -90 .. +90 grader
    void process(double inL, double inR, double pos, double& outL, double& outR)
    {
        bufL_[w_] = inL;
        bufR_[w_] = inR;
        const double az = clampd(pos, -1.0, 1.0) * 0.5 * kPi;
        const double thL = earAngleLeft(az), thR = earAngleRight(az);
        // Fördröjning per öra, relativt det öra som hör först (rakt fram: 0 och 0)
        const double dL = headEarDelay(thL, a_), dR = headEarDelay(thR, a_);
        const double first = dL < dR ? dL : dR;
        outL = shade(earL_, read(bufL_, (dL - first) * sr_), headAlpha(thL)) * nearFieldGain(az, false, r_, a_);
        outR = shade(earR_, read(bufR_, (dR - first) * sr_), headAlpha(thR)) * nearFieldGain(az, true, r_, a_);
        w_ = (w_ + 1) & (kBufSize - 1);
    }

private:
    struct Section { double b0 = 1.0, b1 = 0.0, a1 = 0.0; };
    struct Ear { double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0; };

    // Huvudskugga för vinkeln (alpha) följt av normeringen mot läget rakt fram
    double shade(Ear& e, double x, double alpha)
    {
        const double fs = sr_, d = w0_ + fs;
        const double b0 = (w0_ + alpha * fs) / d, b1 = (w0_ - alpha * fs) / d, a1 = (w0_ - fs) / d;
        const double y = b0 * x + b1 * e.x1 - a1 * e.y1;
        e.x1 = x; e.y1 = y;
        const double z = inv_.b0 * y + inv_.b1 * e.x2 - inv_.a1 * e.y2;
        e.x2 = y; e.y2 = z;
        return z;
    }
    double read(const std::vector<double>& buf, double delay) const
    {
        if (delay <= 0.0) return buf[w_];
        const int di = (int)delay;
        const double f = delay - di;
        const int m = kBufSize - 1;
        const double y0 = buf[(w_ - di + 1) & m], y1 = buf[(w_ - di) & m];
        const double y2 = buf[(w_ - di - 1) & m], y3 = buf[(w_ - di - 2) & m];
        if (di == 0) return y1 + (y2 - y1) * f;   // inget framtida sampel: linjärt sista biten
        const double c1 = 0.5 * (y2 - y0);
        const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    double sr_ = 44100.0, a_ = 0.0875, w0_ = 343.0 / 0.0875, alpha0_ = 1.0, r_ = 1.0;
    Section inv_;
    std::vector<double> bufL_, bufR_;
    Ear earL_, earR_;
    int w_ = 0;
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
        handCoef_ = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * kHandTiltHz / sr_);
        fadeCoef_ = 1.0 - std::exp(-1.0 / (0.020 * sr_));     // 20 ms (LOW MONO av/på)
        filterHz_ = -1.0;
        leslie_.prepare(sr_);
        head_.prepare(sr_, headRadius_);
        air_.build(sr_);
        airSetFor_ = -1.0;
        reset();
    }

    void reset()
    {
        for (int i = 0; i < 2; ++i) { hpL_[i].reset(); hpR_[i].reset(); }
        lpM_.reset();
        leslie_.reset();
        head_.reset();
        airL_.reset(); airR_.reset();
        handLpL_ = handLpR_ = 0.0;
        handG_ = handTarget_ = 1.0;
        distSm_ = distance_;
        airSetFor_ = -1.0;
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
        const int model = lesModelFromNorm(n[kParamLesModel]);
        if (model != leslie_.model()) leslie_.setModel(model);
        headOn_ = headOn(n[kParamHead]);
        headRadius_ = headSizeCm(n[kParamHeadSize]) * 0.005;   // cm -> radie i meter
        if (headRadius_ != head_.radius()) head_.setRadius(headRadius_);
        distance_ = distanceM(n[kParamDistance]);
    }

    // Tempo och position från värden. När Live spelar följer AUTO-PAN takten.
    // Med loop i Live (cycleOn) kan positionen hoppa tillbaka till loopens start
    // mitt i ett block. TruePan räknar fram två förslag (med och utan hopp) och
    // litar på loopinformationen först när Live faktiskt har hoppat tillbaka vid
    // loopens slut. Säger Live att loopen är på utan att hoppa (t.ex. i Session-
    // vyn) följer TruePan i stället den löpande positionen. Inget av fallen hackar.
    void setTransport(bool tempoValid, double tempo, bool playing, bool ppqValid, double ppq,
                      bool cycleOn = false, double cycleStart = 0.0, double cycleEnd = 0.0)
    {
        if (tempoValid && tempo >= 20.0 && tempo <= 999.0) tempo_ = tempo;
        if (playing && ppqValid && std::isfinite(ppq)) {
            const bool cycleOk = cycleOn && std::isfinite(cycleStart) && std::isfinite(cycleEnd) && cycleEnd - cycleStart > 1e-6;
            if (synced_ && crossedLoop_) {
                // Förra blocket passerade loopens slut: vad gjorde Live?
                const double tol = 1e-3 + 4.0 * tempo_ / 60.0 / sr_;
                if (std::fabs(ppq - wrappedPos_) < tol) loopVerified_ = true;           // hoppade tillbaka
                else if (std::fabs(ppq - straightPos_) < tol) loopVerified_ = false;    // spelade vidare
            }
            if (!cycleOk || cycleStart != loopStart_ || cycleEnd != loopEnd_) loopVerified_ = false;   // ny loop: verifiera igen
            loopOn_ = cycleOk && ppq < cycleEnd + 1e-9;
            loopStart_ = cycleStart; loopEnd_ = cycleEnd;
            beatPos_ = straightPos_ = wrappedPos_ = ppq;
            crossedLoop_ = false;
            phase_ = phaseFromBeats(beatPos_);
            synced_ = true;
        } else {
            synced_ = false;
            loopOn_ = false;
            crossedLoop_ = false;
        }
    }
    double phaseFromBeats(double beats) const
    {
        const double ph = beats / kRateBeats[rate_];
        return ph - kLfoCycle * std::floor(ph / kLfoCycle);   // följer takten
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

        // DISTANCE: avståndet glider mjukt (ca 30 ms, i log-skala); filtret följer per block
        if (snap_) distSm_ = distance_;
        else distSm_ = std::exp(std::log(distSm_) + (std::log(distance_) - std::log(distSm_)) * (1.0 - std::exp(-n / (0.03 * sr_))));
        if (std::fabs(distSm_ / distance_ - 1.0) < 1e-6) distSm_ = distance_;
        if (distSm_ != airSetFor_) {
            double k1, k2;
            air_.lookup(distSm_ - kDistRef, k1, k2);
            airL_.set(k1, k2); airR_.set(k1, k2);
            airSetFor_ = distSm_;
        }
        head_.setDistance(distSm_);
        // Avståndslagen 1/r: nivån följer ratten lika snabbt som OUTPUT (5 ms), så att
        // OUTPUT-kompensationen i fönstret håller volymen helt still
        const double tdist = kDistRef / distance_;

        const double phaseInc = tempo_ / 60.0 / sr_ / kRateBeats[rate_];
        const double beatInc = tempo_ / 60.0 / sr_;
        {
            // LESLIE: originalfart (STOP/CHORALE/TREMOLO) eller tempoläge
            const LesModelSpec& sp = kLesModels[leslie_.model()];
            double h = 0.0, d = 0.0;
            if (lesSpeed_ == kLesChorale) { h = sp.hornSlow; d = sp.drumSlow; }
            else if (lesSpeed_ == kLesTremolo) { h = sp.hornFast; d = sp.drumFast; }
            else if (lesSpeed_ >= kLesSyncFirst) {
                h = tempo_ / 60.0 / kLesSyncBeats[lesSpeed_ - kLesSyncFirst];
                d = h * sp.drumFast / sp.hornFast;   // trumman i samma förhållande som originalet
            }
            leslie_.setTargets(h, d);
        }
        double pkL = 0.0, pkR = 0.0;
        double pos = pan_;
        const bool tremStrokes = fxMode_ == kFxTremolo && depth_ > 0.0
                              && (shape_ == kShapeParadiddle || shape_ >= kShapeDoubleStroke);
        const double handDark = dbToGain(-kHandTiltDb * depth_), handBright = dbToGain(kHandTiltDb * depth_);
        const double unaccent = dbToGain(kUnaccentDb * depth_);
        if (!tremStrokes) handTarget_ = 1.0;

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
                    // PARADIDDLE och rudiment i TREM: varje slag = på (betonat fullt, obetonat
                    // svagare), mellan slagen = av
                    const bool strokes = shape_ == kShapeParadiddle || shape_ >= kShapeDoubleStroke;
                    const double tl = strokes ? 2.0 * std::fabs(lfo) - 1.0 : lfo;
                    // Kurva med exponent 1,5: djupare dalar än rak kurva, så att tremolot
                    // hörs tydligt (DEPTH 50 % sänker 9 dB; rak kurva hade gett 6 dB).
                    const double t = 1.0 - depth_ * (0.5 - 0.5 * tl);
                    ttrem = t * std::sqrt(t);
                    if (tremStrokes) {
                        // Händernas karaktär: klangen byts när ett nytt slag börjar
                        // (mellan slagen behålls den, så bytet hörs inte som ett klick)
                        if (lfo > 0.0) handTarget_ = handBright;
                        else if (lfo < 0.0) handTarget_ = handDark;
                        if (!strokeAccented(shape_, phase_)) ttrem *= unaccent;
                    }
                }
            }
            const double tles = fxMode_ == kFxLeslie ? depth_ : 0.0;
            if (synced_) {
                // I takt med Live: räkna fram taktpositionen per sampel (loopen tas med)
                straightPos_ += beatInc;
                wrappedPos_ += beatInc;
                if (loopOn_ && wrappedPos_ >= loopEnd_) { wrappedPos_ = loopStart_ + (wrappedPos_ - loopEnd_); crossedLoop_ = true; }
                beatPos_ = loopVerified_ ? wrappedPos_ : straightPos_;
                phase_ = phaseFromBeats(beatPos_);
            } else {
                phase_ += phaseInc;
                if (phase_ >= kLfoCycle) phase_ -= kLfoCycle * std::floor(phase_ / kLfoCycle);
            }

            // Målvärden
            double tgl, tgr;
            panGains(pos, law_, tgl, tgr);
            const double narrow = mode_ == kModeStereo ? 1.0 - std::fabs(pos) : 1.0;
            const double tside = width_ * narrow;
            const double tlow = lowOn_ ? 1.0 : 0.0;

            if (snap_) {
                gl_ = tgl; gr_ = tgr; side_ = tside; out_ = outGain_; low_ = tlow;
                trem_ = ttrem; les_ = tles;
                handG_ = handTarget_;
                headPos_ = pos; headMix_ = headOn_ ? 1.0 : 0.0;
                distGain_ = tdist;
                snap_ = false;
            } else {
                gl_ += (tgl - gl_) * smoothCoef_;
                gr_ += (tgr - gr_) * smoothCoef_;
                side_ += (tside - side_) * smoothCoef_;
                out_ += (outGain_ - out_) * smoothCoef_;
                low_ += (tlow - low_) * fadeCoef_;
                if (std::fabs(low_ - tlow) < 1e-6) low_ = tlow;
                trem_ += (ttrem - trem_) * smoothCoef_;
                handG_ += (handTarget_ - handG_) * smoothCoef_;
                les_ += (tles - les_) * fadeCoef_;
                headPos_ += (pos - headPos_) * smoothCoef_;
                const double thead = headOn_ ? 1.0 : 0.0;
                headMix_ += (thead - headMix_) * fadeCoef_;
                distGain_ += (tdist - distGain_) * smoothCoef_;
                if (std::fabs(headMix_ - thead) < 1e-6) headMix_ = thead;
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

            // LESLIE (DEPTH = andel effekt). Rotorerna snurrar alltid.
            // Körs alltid (fördröjningen hålls fylld), så att av/på inte klickar.
            // Före WIDTH, så att WIDTH även styr hur brett Leslien låter.
            leslie_.advance();
            {
                double ll, lr;
                leslie_.process((xl + xr) * 0.5, ll, lr);
                if (les_ > 0.0) {
                    xl += (ll - xl) * les_;
                    xr += (lr - xr) * les_;
                }
            }

            // WIDTH och MODE (mid/side)
            double yl = xl, yr = xr;
            if (side_ != 1.0) {
                const double m = (xl + xr) * 0.5, s = (xl - xr) * 0.5 * side_;
                yl = m + s;
                yr = m - s;
            }

            // Panorering: PAN LAW eller HEAD (huvudmodellen ersätter nivåpanoreringen).
            // Huvudmodellen körs alltid (fördröjningen hålls fylld), så att av/på inte klickar.
            double pl = yl * gl_, pr = yr * gr_;
            {
                double hl, hr;
                head_.process(yl, yr, headPos_, hl, hr);
                if (headMix_ > 0.0) {
                    pl += (hl - pl) * headMix_;
                    pr += (hr - pr) * headMix_;
                }
            }

            // + bas i mitten, DISTANCE (luftdämpning och 1/r), TREMOLO, OUTPUT
            const double g = trem_ * out_ * distGain_;
            double ol = airL_.process(pl + bass) * g;
            double orr = airR_.process(pr + bass) * g;

            // TREM-rudiment: diskanten mörkare (vänster hand) eller ljusare (höger hand).
            // Filtret körs alltid, så att av/på inte klickar.
            handLpL_ += (ol - handLpL_) * handCoef_;
            handLpR_ += (orr - handLpR_) * handCoef_;
            if (handG_ != 1.0) {
                ol = handLpL_ + (ol - handLpL_) * handG_;
                orr = handLpR_ + (orr - handLpR_) * handG_;
            }

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
    double phase() const { return phase_ - std::floor(phase_); }   // inom perioden
    double cyclePhase() const { return phase_; }                  // 0..4 perioder
    double tempo() const { return tempo_; }
    double sampleRate() const { return sr_; }
    double headRadius() const { return head_.radius(); }
    double distanceNow() const { return distSm_; }
    double leslieHornHz() const { return leslie_.hornHz(); }
    double leslieDrumHz() const { return leslie_.drumHz(); }

private:
    double sr_ = 44100.0;
    double smoothCoef_ = 0.0, fadeCoef_ = 0.0;

    // parametrar
    double pan_ = 0.0, width_ = 1.0, depth_ = 0.0, outGain_ = 1.0, lowHz_ = 120.0;
    int mode_ = kModeStereo, law_ = kLaw3, rate_ = 5, shape_ = kShapeSine;
    int monitor_ = kMonStereo, fxMode_ = kFxPan, lesSpeed_ = kLesChorale;
    bool lowOn_ = false;

    // utjämnade värden
    double gl_ = 1.0, gr_ = 1.0, side_ = 1.0, out_ = 1.0, low_ = 0.0, trem_ = 1.0, les_ = 0.0;
    bool snap_ = true;
    double handCoef_ = 0.0, handLpL_ = 0.0, handLpR_ = 0.0, handG_ = 1.0, handTarget_ = 1.0;

    // AUTO-PAN
    double tempo_ = 120.0, phase_ = 0.0, beatPos_ = 0.0, loopStart_ = 0.0, loopEnd_ = 0.0;
    double straightPos_ = 0.0, wrappedPos_ = 0.0;
    bool loopOn_ = false, loopVerified_ = false, crossedLoop_ = false;
    bool synced_ = false;

    // LOW MONO
    Biquad hpL_[2], hpR_[2];
    LR4 lpM_;
    double filterHz_ = -1.0;

    Leslie leslie_;

    // HEAD
    HeadModel head_;
    bool headOn_ = false;

    // DISTANCE
    AirTable air_;
    AirFilter airL_, airR_;
    double distance_ = kDistRef, distSm_ = kDistRef, distGain_ = 1.0, airSetFor_ = -1.0;
    double headRadius_ = kHeadStdCm * 0.005, headPos_ = 0.0, headMix_ = 0.0;

    // visning
    double displayPos_ = 0.0, peakL_ = 0.0, peakR_ = 0.0;
};

// ---------------------------------------------------------------------------
// Fabrikspresets (syns som program i Lives presetlista och i fönstret)
// ---------------------------------------------------------------------------
const int kNumFactoryPresets = 19;

inline const char* factoryPresetName(int i)
{
    static const char* const names[kNumFactoryPresets] = {
        "Center", "Hard Left", "Hard Right", "Wide Stereo", "Mono",
        "Auto-Pan 1/4", "Auto-Pan 1/8 Tri", "Ping-Pong 1/16", "Slow Sweep 2 Bars", "Bass Mono 120",
        "Tremolo 1/8", "Leslie 122 Chorale", "Leslie 122 Tremolo", "Leslie 145 Tremolo", "Vibratone Fast", "Paradiddle 1/16", "Diddletie 1/8", "Double Stroke 1/16", "Flam 1/8"
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
    case 15: v[kParamDepth] = 1.0f; v[kParamRate] = choiceNorm(9, kNumRates); v[kParamShape] = choiceNorm(kShapeParadiddle, kNumShapes); break;
    case 16: v[kParamDepth] = 1.0f; v[kParamRate] = choiceNorm(7, kNumRates); v[kParamShape] = choiceNorm(kShapeParaVariant, kNumShapes); break;
    case 17: v[kParamDepth] = 1.0f; v[kParamRate] = choiceNorm(9, kNumRates); v[kParamShape] = choiceNorm(kShapeDoubleStroke, kNumShapes); break;
    case 18: v[kParamDepth] = 1.0f; v[kParamRate] = choiceNorm(7, kNumRates); v[kParamShape] = choiceNorm(kShapeFlam, kNumShapes); break;
    case 8: v[kParamDepth] = 0.7f; v[kParamRate] = choiceNorm(1, kNumRates); break;
    case 9: v[kParamLowMono] = lowMonoNorm(120.0); v[kParamWidth] = widthNorm(1.2); break;
    case 10: v[kParamFxMode] = choiceNorm(kFxTremolo, kNumFxModes); v[kParamDepth] = 0.85f; v[kParamRate] = choiceNorm(7, kNumRates); break;
    case 11: v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes); v[kParamDepth] = 1.0f; v[kParamLesSpeed] = choiceNorm(kLesChorale, kNumLesSpeeds); break;
    case 12: v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes); v[kParamDepth] = 1.0f; v[kParamLesSpeed] = choiceNorm(kLesTremolo, kNumLesSpeeds); break;
    case 13: v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes); v[kParamDepth] = 1.0f; v[kParamLesSpeed] = choiceNorm(kLesTremolo, kNumLesSpeeds);
             v[kParamLesModel] = choiceNorm(kModel145, kNumLesModels); break;
    case 14: v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes); v[kParamDepth] = 1.0f; v[kParamLesSpeed] = choiceNorm(kLesTremolo, kNumLesSpeeds);
             v[kParamLesModel] = choiceNorm(kModelVibratone, kNumLesModels); break;
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
static const char* const kPresetHeader = "TruePan preset 8";
static const char* const kPresetHeaderV22 = "TruePan preset 7";   // v2.2: DISTANCE med 1 m i mitten
static const char* const kPresetHeaderV20 = "TruePan preset 6";   // v2.0 - v2.1.1: gammal DISTANCE-skala
static const char* const kPresetHeaderV19 = "TruePan preset 5";   // v1.9: 5 vågformer
static const char* const kPresetHeaderV18 = "TruePan preset 4";   // v1.8: 4 vågformer
static const char* const kPresetHeaderV14 = "TruePan preset 3";   // v1.4-1.7: 3 vågformer
static const char* const kPresetHeaderV11 = "TruePan preset 1";   // v1.1: LesSpd var SLOW/FAST
static const char* const kPresetHeaderV12 = "TruePan preset 2";   // v1.2-1.3: LesSpd hade 8 tempolägen

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
    const bool v12 = text.compare(0, std::strlen(kPresetHeaderV12), kPresetHeaderV12) == 0;
    const bool v14 = text.compare(0, std::strlen(kPresetHeaderV14), kPresetHeaderV14) == 0;
    const bool v18 = text.compare(0, std::strlen(kPresetHeaderV18), kPresetHeaderV18) == 0;
    const bool v19 = text.compare(0, std::strlen(kPresetHeaderV19), kPresetHeaderV19) == 0;
    const bool v20 = text.compare(0, std::strlen(kPresetHeaderV20), kPresetHeaderV20) == 0;
    const bool v22 = text.compare(0, std::strlen(kPresetHeaderV22), kPresetHeaderV22) == 0;
    if (!v11 && !v12 && !v14 && !v18 && !v19 && !v20 && !v22 && text.compare(0, std::strlen(kPresetHeader), kPresetHeader) != 0) return false;
    const bool oldDistance = v14 || v18 || v19 || v20;   // presets med Dist-rad i gammal skala
    const int oldShapeCount = (v11 || v12 || v14) ? kNumShapesV16 : (v18 ? kNumShapesV18 : (v19 ? kNumShapesV19 : 0));
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
                if (v12 && i == kParamLesSpeed) v[i] = lesSpeedNormFromV12((float)d);
                if (oldShapeCount && i == kParamShape) v[i] = shapeNormFromOld((float)d, oldShapeCount);
                if (oldDistance && i == kParamDistance) v[i] = distanceNormFromV17((float)d);
                if (v22 && i == kParamDistance) v[i] = distanceNormFromV22((float)d);
            }
        }
    }
    return true;
}

} // namespace truepan
