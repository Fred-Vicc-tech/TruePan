// TruePanEditor.cpp - plugin-fönstret (Windows, GDI+), gemensamt för VST2-
// och VST3-versionen. Pratar bara med kärnan (TruePanCore.h).
#include "TruePanCore.h"
#include "Manual.h"

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>
#include <windows.h>
// gdiplus.h behöver min/max, som NOMINMAX (i CMakeLists) stänger av.
#include <algorithm>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#include <commdlg.h>
#include <shlobj.h>
#include <string>
#include <vector>

using namespace truepan;

// ===========================================================================
// Plugin-fönstret (Windows, GDI+), samma stil som TrueSub.
// Rad 1: PAN, MODE/WIDTH, AUTO-PAN (PAN/TREM/LESLIE), OUTPUT (lika breda).
// Rad 2: STEREO FIELD (position och nivåmätare), PRESET (SAVE/LOAD).
// Nederst: PAN LAW-grafen (verkliga kurvor för vald lag).
// ===========================================================================
static HINSTANCE g_instance = nullptr;
static ULONG_PTR g_gdiplusToken = 0;
static int g_gdiplusUsers = 0;
static bool g_classRegistered = false;
static const wchar_t* kWindowClass = L"TruePanEditorWindow";

namespace {

using namespace Gdiplus;

// Färger som i TrueSub: mörk metall, cyan detaljer, orange som andra färg.
const Color kBgTop(255, 46, 49, 54);
const Color kBgBottom(255, 22, 24, 27);
const Color kPanel(255, 14, 15, 17);
const Color kPanelEdge(255, 70, 74, 80);
const Color kCyan(255, 0, 220, 255);
const Color kOrange(255, 255, 160, 40);
const Color kText(255, 225, 230, 235);
const Color kTextDim(255, 140, 146, 152);
const Color kGrid(255, 45, 49, 54);

// Layout
const float kPanelW = 206.0f;
inline float colX(int i) { return 20.0f + i * 218.0f; }  // fyra lika breda kolumner
// Rad 1 är 30 px högre än i v1.9 (mer luft mellan knappar, rattar och rutor);
// allt under flyttas ner lika mycket (kDy).
const float kDy = 30.0f;
const float kRow1Y = 83.0f, kRow1H = 260.0f;
const float kRow2Y = 323.0f + kDy, kRow2H = 130.0f;
// v2.1: layout enligt användarens bild (GUI_02): egen rad för vågform/modell i
// AUTO-PAN, WIDTH/DEPTH och rattarna längre ner, högre graf, HEAD-knapparna i mitten.
const float kGraphY = 494.0f, kGraphH = 186.0f;
const float kKnobRowY = 272.0f;      // rattar i MODE och AUTO-PAN
const float kKnobValueY = 313.0f;      // procent under WIDTH/DEPTH
const float kLabelY = 199.0f;        // WIDTH / DEPTH
const float kShapeRowY = 161.0f;     // vågform/modell-väljaren under PAN/TREM/LESLIE
const float kBigKnobY = 192.0f;      // PAN och OUTPUT
const float kBigValueY = 252.0f;     // värdet under PAN och OUTPUT
const float kSelY = 296.0f;          // LAW- och MONITOR-väljarna
const float kSideKnobDX = 157.0f;
const float kFieldX = 20.0f, kFieldW = 642.0f;

struct KnobDef
{
    int param;
    float cx, cy, r;
};

struct RectDef
{
    float x, y, w, h;
    bool hit(int px, int py) const { return px >= x && px <= x + w && py >= y && py <= y + h; }
};

struct Editor
{
    Core* plugin = nullptr;
    HWND hwnd = nullptr;

    static const int kNumKnobs = 8;
    KnobDef knobs[kNumKnobs] = {
        { kParamPan,     colX(0) + 103.0f, kBigKnobY, 44.0f },
        { kParamWidth,   colX(1) + 62.0f, kKnobRowY, 36.0f },
        { kParamLowMono, colX(1) + kSideKnobDX, kKnobRowY, 20.0f },
        { kParamDepth,   colX(2) + 62.0f, kKnobRowY, 36.0f },
        { kParamRate,    colX(2) + kSideKnobDX, kKnobRowY, 20.0f },
        { kParamOutput,  colX(3) + 103.0f, kBigKnobY, 44.0f },
        { kParamLesSpeed, colX(2) + kSideKnobDX, kKnobRowY, 20.0f }, // samma plats som RATE (LESLIE)
        { kParamDistance, 604.0f, 424.0f, 20.0f },                   // DISTANCE i STEREO FIELD
    };
    int dragKnob = -1;
    int dragStartY = 0;
    float dragStartValue = 0.0f;

    // Väljare med pilar: samma storlek som PRESET (ruta mellan pilarna)
    RectDef panDown    = { colX(0) + 14.0f,  kBigValueY, 30.0f, 30.0f };   // PAN-steg
    RectDef panUp      = { colX(0) + 162.0f, kBigValueY, 30.0f, 30.0f };
    RectDef lawDown    = { colX(0) + 14.0f,  kSelY, 30.0f, 30.0f };
    RectDef lawUp      = { colX(0) + 162.0f, kSelY, 30.0f, 30.0f };
    RectDef monPrev    = { colX(3) + 14.0f,  kSelY, 30.0f, 30.0f };
    RectDef monNext    = { colX(3) + 162.0f, kSelY, 30.0f, 30.0f };
    RectDef btnBalance = { colX(1) + 14.0f,  119.0f, 84.0f, 34.0f };
    RectDef btnStereo  = { colX(1) + 106.0f, 119.0f, 84.0f, 34.0f };
    // AUTO-PAN: tre lägen, vågformsruta (klick = nästa form), SLOW/FAST för LESLIE
    RectDef btnFx[kNumFxModes] = { { colX(2) + 14.0f, 119.0f, 56.0f, 34.0f },
                                   { colX(2) + 75.0f, 119.0f, 56.0f, 34.0f },
                                   { colX(2) + 136.0f, 119.0f, 56.0f, 34.0f } };
    // Vågform (PAN/TREM) eller modell (LESLIE): < ruta >, samma typ som PRESET
    RectDef shapePrev  = { colX(2) + 14.0f,  kShapeRowY, 30.0f, 30.0f };
    RectDef shapeNext  = { colX(2) + 162.0f, kShapeRowY, 30.0f, 30.0f };
    RectDef shapeBox   = { colX(2) + 48.0f,  kShapeRowY, 110.0f, 30.0f };
    RectDef presetPrev = { colX(3) + 14.0f,  357.0f + kDy, 30.0f, 30.0f };
    RectDef presetNext = { colX(3) + 162.0f, 357.0f + kDy, 30.0f, 30.0f };
    RectDef btnSave    = { colX(3) + 14.0f,  401.0f + kDy, 84.0f, 34.0f };
    // HEAD i grafens rubrikrad: knapp och huvudstorlek < 17.5 cm >
    RectDef btnHead    = { 278.0f, 507.0f, 60.0f, 26.0f };
    RectDef headDown   = { 346.0f, 507.0f, 26.0f, 26.0f };
    RectDef headUp     = { 442.0f, 507.0f, 26.0f, 26.0f };
    RectDef btnLoad    = { colX(3) + 106.0f, 401.0f + kDy, 84.0f, 34.0f };

    float meterL = 0.0f, meterR = 0.0f;   // nivåmätare med mjukt fall
    DWORD overUntil = 0;

    // Baksidan (klick på loggan): credits och manual
    bool showBack = false;
    float logoRight = 200.0f;          // loggans högra kant (mäts när den ritas)
    float scroll = 0.0f;               // manualens rullning (px)
    float contentH = 0.0f;             // manualens totala höjd (px)
    bool dragThumb = false;
    int dragThumbY = 0;
    float dragScroll0 = 0.0f;
    RectDef btnBack = { 760.0f, 114.0f, 100.0f, 34.0f };
    HWND prevFocus = nullptr;          // fönstret som hade fokus innan musen kom in
    bool tracking = false;
    bool overLogo(int x, int y) const { return x >= 15 && x <= logoRight && y >= 4 && y <= 52; }
    bool dragField = false;            // PAN dras i STEREO FIELD
    int lastKeyDir = 0;                // piltangenterna (se panKey)
    DWORD lastKeyTime = 0;
    bool overField(int x, int y) const;

    bool leslieMode() const { return fxModeFromNorm(plugin->params[kParamFxMode].load()) == kFxLeslie; }

    int knobAt(int x, int y) const
    {
        for (int i = 0; i < kNumKnobs; ++i) {
            if (knobs[i].param == kParamRate && leslieMode()) continue;      // RATE är dold i LESLIE-läget
            if (knobs[i].param == kParamLesSpeed && !leslieMode()) continue; // SPEED syns bara i LESLIE-läget
            const float dx = x - knobs[i].cx, dy = y - knobs[i].cy, rr = knobs[i].r + 12.0f;
            if (dx * dx + dy * dy <= rr * rr) return i;
        }
        return -1;
    }
};

void roundedRect(GraphicsPath& path, float x, float y, float w, float h, float r)
{
    const float d = r * 2;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
}

void drawPanel(Graphics& g, float x, float y, float w, float h)
{
    GraphicsPath path;
    roundedRect(path, x, y, w, h, 12);
    SolidBrush fill(kPanel);
    g.FillPath(&fill, &path);
    Pen edge(kPanelEdge, 1.0f);
    g.DrawPath(&edge, &path);
}

void drawText(Graphics& g, const wchar_t* text, const FontFamily& ff, float size, int style,
              const Color& color, float x, float y, float w, StringAlignment align)
{
    Font font(&ff, size, style, UnitPixel);
    SolidBrush brush(color);
    StringFormat fmt;
    fmt.SetAlignment(align);
    RectF r(x, y, w, size * 1.6f);
    g.DrawString(text, -1, &font, r, &fmt, &brush);
}

void drawButton(Graphics& g, const FontFamily& ff, const RectDef& r, const wchar_t* label, bool active, const Color& activeColor,
                float fontSize = 0.0f)
{
    GraphicsPath path;
    roundedRect(path, r.x, r.y, r.w, r.h, 6);
    SolidBrush fill(active ? Color(255, 10, 60, 80) : Color(255, 30, 33, 37));
    g.FillPath(&fill, &path);
    Pen edge(active ? activeColor : kPanelEdge, active ? 2.0f : 1.0f);
    g.DrawPath(&edge, &path);
    const float size = fontSize > 0.0f ? fontSize : (r.h >= 34.0f ? 15.0f : 14.0f);
    // Pilarna "<" och ">" sitter lågt i typsnittet: lyft dem 1,5 px.
    const bool arrow = label[0] != 0 && label[1] == 0 && (label[0] == L'<' || label[0] == L'>');
    const float dy = arrow ? -1.5f : 0.0f;
    drawText(g, label, ff, size, FontStyleBold, active ? activeColor : kTextDim, r.x, r.y + (r.h - size * 1.35f) / 2.0f + dy, r.w, StringAlignmentCenter);
}

// Mörk ruta som i PRESET/OCTAVE i TrueSub
void drawBox(Graphics& g, float x, float y, float w, float h)
{
    GraphicsPath box;
    roundedRect(box, x, y, w, h, 6);
    SolidBrush boxFill(Color(255, 8, 9, 10));
    g.FillPath(&boxFill, &box);
    Pen boxEdge(kPanelEdge, 1.0f);
    g.DrawPath(&boxEdge, &box);
}

// Ratt med kuggkant, samma som i TrueSub. Cyan båge för värdet (0..1);
// bipolar = bågen växer från toppen (PAN: vänster/höger).
void drawKnob(Graphics& g, const KnobDef& k, float v, bool dim = false, bool bipolar = false, const Color* leftColor = nullptr,
              float zero = 0.5f)
{
    const float cx = k.cx, cy = k.cy, r = k.r;
    const float start = 135.0f, sweep = 270.0f;
    const bool isSmall = r < 30.0f;
    const float arcW = isSmall ? 4.0f : 6.0f;
    const float ar = r + (isSmall ? 6.0f : 9.0f);
    Pen track(Color(255, 40, 43, 47), arcW);
    g.DrawArc(&track, cx - ar, cy - ar, ar * 2, ar * 2, start, sweep);
    Pen arc(dim ? Color(255, 70, 90, 98) : kCyan, arcW);
    if (bipolar) {
        // Bågen växer från nollpunkten (zero, t.ex. mitten för PAN, klockan 9 för DISTANCE)
        const float d = v - zero;
        Pen arcLeft(leftColor && !dim ? *leftColor : (dim ? Color(255, 70, 90, 98) : kCyan), arcW);
        if (std::fabs(d) > 0.001f) g.DrawArc(d < 0.0f ? &arcLeft : &arc, cx - ar, cy - ar, ar * 2, ar * 2, start + sweep * zero, sweep * d);
    } else if (v > 0.001f) g.DrawArc(&arc, cx - ar, cy - ar, ar * 2, ar * 2, start, sweep * v);

    const float kPiF = 3.14159265f;
    const float angle = (start + sweep * v) * kPiF / 180.0f;

    for (int i = 0; i < 4; ++i) {
        const float rr = r + 2.5f - i * 1.0f;
        SolidBrush sh(Color((BYTE)(40 + i * 25), 0, 0, 0));
        g.FillEllipse(&sh, cx - rr + 1.5f, cy - rr + 3.0f, rr * 2, rr * 2);
    }

    const float kRefR = 36.0f;
    const float kDepth = (0.99f - 0.90f) * kRefR;
    const float tipRef = kRefR * (0.99f - 2.5f / 82.0f);
    const float kPitch = 2.0f * kPiF * tipRef / 20.0f;
    const float kGap = tipRef - kDepth - (0.66f * kRefR + kRefR * 4.4f / 82.0f);
    const float tip = r * (0.99f - 2.5f / 82.0f);
    const float root = tip - kDepth * (isSmall ? 0.72f : 1.0f);
    int n = isSmall ? 14 : (int)std::floor(2.0f * kPiF * tip / kPitch + 0.5f);
    if (n < 8) n = 8;
    const float ringOut = root - (isSmall ? kGap - 1.4f : kGap);
    const float ringIn = ringOut - 3.0f;

    const Color dark(255, 14, 14, 16);
    const Color rim  = dim ? Color(255, 72, 80, 88)  : Color(255, 115, 127, 139);
    const Color ring = Color(255, 26, 34, 43);
    const Color cap  = dim ? Color(255, 68, 76, 84)  : Color(255, 109, 122, 134);
    const Color ptr  = dim ? Color(255, 140, 146, 152) : Color(255, 250, 250, 250);

    SolidBrush darkB(dark);
    g.FillEllipse(&darkB, cx - r, cy - r, r * 2, r * 2);

    const int steps = n * 24;
    PointF pts[64 * 24];
    const int m = steps <= 64 * 24 ? steps : 64 * 24;
    const float rot = angle - kPiF / n;
    for (int i = 0; i < m; ++i) {
        const float a = 2.0f * kPiF * i / m;
        float u = (a - rot) * n / (2.0f * kPiF);
        u -= std::floor(u);
        const float x = std::fabs(u - 0.5f) * 2.0f;
        float t = (x - 0.25f) / 0.5f;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        t = t * t * (3.0f - 2.0f * t);
        const float rr = tip - (tip - root) * t;
        pts[i] = PointF(cx + rr * std::cos(a), cy + rr * std::sin(a));
    }
    SolidBrush rimB(rim);
    g.FillPolygon(&rimB, pts, m);

    SolidBrush ringB(ring), capB(cap);
    g.FillEllipse(&ringB, cx - ringOut, cy - ringOut, ringOut * 2, ringOut * 2);
    g.FillEllipse(&capB, cx - ringIn, cy - ringIn, ringIn * 2, ringIn * 2);

    float w = r * 2.2f / 82.0f * 1.6f;
    if (w < 1.8f) w = 1.8f;
    Pen pointer(ptr, w);
    const float r0 = ringOut + r * 0.4f / 82.0f, r1 = tip - r * 3.6f / 82.0f;
    g.DrawLine(&pointer, cx + std::cos(angle) * r0, cy + std::sin(angle) * r0,
               cx + std::cos(angle) * r1, cy + std::sin(angle) * r1);
}

void drawSmallKnob(Graphics& g, const FontFamily& ff, const KnobDef& k, float v, const wchar_t* label, const wchar_t* value,
                   bool dim = false, const Color* valueColor = nullptr, bool bipolar = false, const Color* leftColor = nullptr,
                   float zero = 0.5f)
{
    const Color& tc = dim ? kTextDim : kText;
    drawText(g, label, ff, 12, FontStyleBold, tc, k.cx - 52.0f, k.cy - 46.0f, 104.0f, StringAlignmentCenter);
    drawKnob(g, k, v, dim, bipolar, leftColor, zero);
    drawText(g, value, ff, 13, valueColor ? FontStyleBold : FontStyleRegular, valueColor && !dim ? *valueColor : tc,
             k.cx - 52.0f, k.cy + 27.0f, 104.0f, StringAlignmentCenter);
}

void drawBigValue(Graphics& g, const FontFamily& ff, const KnobDef& k, const wchar_t* text, const Color& c)
{
    drawText(g, text, ff, 13, FontStyleBold, c, k.cx + 4.0f - 50.0f, kKnobValueY, 100.0f, StringAlignmentCenter);
}

// Liten bild av AUTO-PAN-vågformen (två perioder)
void drawShapeIcon(Graphics& g, int shape, float x, float y, float w, float h, const Color& color)
{
    const int n = 241;   // tillräckligt för 8 skarpa slag och flammens förslag
    PointF pts[n];
    for (int i = 0; i < n; ++i) {
        // två perioder; PARADIDDLE/PARAVARIANT visar hela mönstret (8 slag)
        double span = 2.0;
        switch (shape) {
        case kShapeParadiddle: case kShapeInvParadiddle: span = 8.0; break;
        case kShapeParaVariant: case kShapeDoubleStroke: span = 4.0; break;
        case kShapeParaDiddleDiddle: case kShapeSixStroke: span = 6.0; break;
        case kShapeFlam: span = 2.0; break;
        default: span = 2.0; break;
        }
        double ph = (double)i / (n - 1) * span;
        if (i == n - 1) ph = span - 1e-6;
        const float v = (float)lfoValue(shape, ph);
        pts[i] = PointF(x + w * (float)i / (n - 1), y + h * 0.5f - v * h * 0.45f);
    }
    Pen pen(color, 1.8f);
    g.DrawLines(&pen, pts, n);
}

void wideFromNarrow(const char* in, wchar_t* out, int max)
{
    int i = 0;
    for (; i < max - 1 && in[i]; ++i) out[i] = (wchar_t)(unsigned char)in[i];
    out[i] = 0;
}

// Grafens koordinater (PAN LAW)
const float kPlotX0 = 86.0f, kPlotX1 = 850.0f, kPlotY0 = 546.0f, kPlotY1 = 634.0f;
static float kPlotDbMax = 6.0f;           // +12 dB i HEAD-läget (närmaste örat får mer diskant)
const float kPlotDbMin = -24.0f;

float plotX(double pos) { return kPlotX0 + (float)((pos + 1.0) * 0.5) * (kPlotX1 - kPlotX0); }
float plotY(double db)
{
    if (db > kPlotDbMax) db = kPlotDbMax;
    if (db < kPlotDbMin) db = kPlotDbMin;
    return kPlotY0 + (float)((kPlotDbMax - db) / (kPlotDbMax - kPlotDbMin)) * (kPlotY1 - kPlotY0);
}

enum CurveKind { kCurveLeft, kCurveRight, kCurveTotal };

// HEAD-läget: örats nivå vid frekvensen hz för alla positioner
void drawHeadCurve(Graphics& g, bool rightEar, double hz, double a, const Color& color, float width, bool dashed)
{
    const int n = 101;
    PointF pts[n];
    for (int i = 0; i < n; ++i) {
        const double pos = -1.0 + 2.0 * i / (n - 1);
        pts[i] = PointF(plotX(pos), plotY(headEarGainDb(pos * 0.5 * kPi, rightEar, hz, a)));
    }
    Pen pen(color, width);
    if (dashed) pen.SetDashStyle(DashStyleDash);
    g.DrawLines(&pen, pts, n);
}

void drawLawCurve(Graphics& g, int law, CurveKind kind, const Color& color, float width, bool dashed)
{
    const int n = 101;
    PointF pts[n];
    for (int i = 0; i < n; ++i) {
        const double pos = -1.0 + 2.0 * i / (n - 1);
        double gl, gr;
        panGains(pos, law, gl, gr);
        double db;
        if (kind == kCurveLeft) db = gainToDb(gl);
        else if (kind == kCurveRight) db = gainToDb(gr);
        else db = 10.0 * std::log10((gl * gl + gr * gr) * 0.5 + 1e-24); // monoljudets totala nivå
        pts[i] = PointF(plotX(pos), plotY(db));
    }
    Pen pen(color, width);
    if (dashed) pen.SetDashStyle(DashStyleDash);
    g.DrawLines(&pen, pts, n);
}

// Nivå 0..1 -> stapelns längd (-30 .. +6 dBFS)
const double kMeterMinDb = -30.0, kMeterMaxDb = 6.0;
float meterFrac(float peak)
{
    const double db = gainToDb(peak);
    const double t = (db - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb);
    return (float)clampd(t, 0.0, 1.0);
}

// ===========================================================================
// Baksidan: vem som byggt TruePan och en rullbar manual
// ===========================================================================
const float kManX = 40.0f, kManW = 798.0f, kManTop = 224.0f, kManBottom = 664.0f;
const float kThumbX = 852.0f, kThumbW = 6.0f;

// STEREO FIELD-banan (PAN går att dra här)
const float kFieldX0 = 70.0f, kFieldX1 = 540.0f, kFieldY = 366.0f + kDy;
float fieldPanNorm(int x)
{
    float n = (x - kFieldX0) / (kFieldX1 - kFieldX0);
    n = n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
    for (int i = 0; i < kNumPanSteps; ++i) {                            // snäpper till stegen (och C)
        const float s = panStepNorm(kPanSteps[i]);
        if (std::fabs(n - s) * (kFieldX1 - kFieldX0) < 4.0f) return s;
    }
    return n;
}
bool Editor::overField(int x, int y) const
{
    return !showBack && x >= kFieldX0 - 10 && x <= kFieldX1 + 10 && y >= kFieldY - 14 && y <= kFieldY + 12;
}

float manualMaxScroll(const Editor* ed) { return std::max(0.0f, ed->contentH - (kManBottom - kManTop)); }

void manualFont(int kind, float& size, int& style, Color& color)
{
    if (kind == kManHead) { size = 14.0f; style = FontStyleBold; color = kCyan; }
    else if (kind == kManNote) { size = 12.0f; style = FontStyleItalic; color = kText; }
    else { size = 13.0f; style = FontStyleRegular; color = kText; }
}

// Ritar (eller bara mäter, om draw = false) manualen; returnerar total höjd
float layoutManual(Graphics& g, const FontFamily& ff, float y0, bool draw)
{
    float y = y0;
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentNear);
    for (int i = 0; i < kManualCount; ++i) {
        const ManualPara& mp = kManual[i];
        float size; int style; Color color;
        manualFont(mp.kind, size, style, color);
        if (mp.kind == kManHead && i > 0) y += 14.0f;
        Font font(&ff, size, style, UnitPixel);
        RectF box;
        g.MeasureString(mp.text, -1, &font, RectF(0.0f, 0.0f, kManW, 10000.0f), &fmt, &box);
        if (draw && y + box.Height > kManTop - 20.0f && y < kManBottom + 20.0f) {
            SolidBrush brush(color);
            g.DrawString(mp.text, -1, &font, RectF(kManX, y, kManW, box.Height + 2.0f), &fmt, &brush);
        }
        y += box.Height + (mp.kind == kManHead ? 3.0f : 7.0f);
    }
    return y - y0;
}

void paintBack(Editor* ed, Graphics& g, const FontFamily& ff)
{
    // Credits
    drawPanel(g, 20.0f, 83.0f, 860.0f, 92.0f);
    drawText(g, L"TruePan v2.6.1", ff, 22, FontStyleBold, kText, 38.0f, 92.0f, 400.0f, StringAlignmentNear);
    drawText(g, L"Psychoacoustic Stereo Panner  \u00B7  VST2 / VST3  \u00B7  Windows 7 and later", ff, 13, FontStyleItalic, kText,
             38.0f, 123.0f, 600.0f, StringAlignmentNear);
    {
        // Fredrik Frigren | Copyright (c) 2F Productions 2026 | info@2f.se
        // Copyright-tecknet ritas själv (ring + C) så att det blir tydligt i alla storlekar.
        Font f(&ff, 15, FontStyleBold, UnitPixel);
        StringFormat typo(StringFormat::GenericTypographic());
        typo.SetFormatFlags(typo.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
        SolidBrush cy(kCyan);
        const wchar_t* partA = L"Fredrik Frigren  |  Copyright ";
        const wchar_t* partB = L" 2F Productions 2026  |  info@2f.se";
        RectF boxA;
        g.MeasureString(partA, -1, &f, PointF(0.0f, 0.0f), &typo, &boxA);
        float x = 40.0f;
        const float y = 148.0f;
        g.DrawString(partA, -1, &f, PointF(x, y), &typo, &cy);
        x += boxA.Width;
        const float d = 15.0f, cyc = y + 9.5f;           // ringens diameter och mitt (textens mitt)
        Pen ring(kCyan, 1.6f);
        g.DrawEllipse(&ring, x + 1.0f, cyc - d * 0.5f, d, d);
        drawText(g, L"C", ff, 10, FontStyleBold, kCyan, x + 1.0f, cyc - 7.5f, d, StringAlignmentCenter);
        x += d + 2.0f;
        g.DrawString(partB, -1, &f, PointF(x, y), &typo, &cy);
    }
    drawButton(g, ff, ed->btnBack, L"BACK", false, kCyan);

    // Manual
    drawPanel(g, 20.0f, 185.0f, 860.0f, 495.0f);
    drawText(g, L"MANUAL", ff, 14, FontStyleBold, kTextDim, 38.0f, 192.0f, 200.0f, StringAlignmentNear);
    drawText(g, L"scroll with the mouse wheel  \u00B7  click the logo or BACK to return", ff, 11, FontStyleRegular, kTextDim,
             380.0f, 195.0f, 478.0f, StringAlignmentFar);
    if (ed->contentH <= 0.0f) ed->contentH = layoutManual(g, ff, 0.0f, false);
    const float maxScroll = manualMaxScroll(ed);
    if (ed->scroll > maxScroll) ed->scroll = maxScroll;
    if (ed->scroll < 0.0f) ed->scroll = 0.0f;
    g.SetClip(RectF(kManX - 4.0f, kManTop, kManW + 8.0f, kManBottom - kManTop));
    layoutManual(g, ff, kManTop - ed->scroll, true);
    g.ResetClip();

    // Rullningslist
    const float viewH = kManBottom - kManTop;
    SolidBrush track(Color(255, 30, 33, 37));
    g.FillRectangle(&track, kThumbX, kManTop, kThumbW, viewH);
    if (maxScroll > 0.0f) {
        const float th = std::max(30.0f, viewH * viewH / ed->contentH);
        const float ty = kManTop + (viewH - th) * (ed->scroll / maxScroll);
        SolidBrush thumb(ed->dragThumb ? kCyan : Color(255, 90, 96, 104));
        g.FillRectangle(&thumb, kThumbX, ty, kThumbW, th);
    }
}

void paint(Editor* ed, HDC hdc, int width, int height)
{
    Core* p = ed->plugin;
    if (width <= 0 || height <= 0) return;

    Bitmap buffer(width, height, PixelFormat32bppPARGB);
    Graphics g(&buffer);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    FontFamily segoe(L"Segoe UI");   // finns i Windows 7
    FontFamily arial(L"Arial");      // reserv
    const FontFamily& ff = segoe.IsAvailable() ? segoe : arial;

    // Bakgrund: mörk borstad metall
    LinearGradientBrush bg(Point(0, 0), Point(0, height), kBgTop, kBgBottom);
    g.FillRectangle(&bg, 0, 0, width, height);
    Pen hair(Color(10, 255, 255, 255), 1.0f);
    for (int y = 0; y < height; y += 3) g.DrawLine(&hair, 0, y, width, y);

    float pv[kNumParams];
    for (int i = 0; i < kNumParams; ++i) pv[i] = p->params[i].load();
    const double pan = panFromNorm(pv[kParamPan]);
    const int mode = modeFromNorm(pv[kParamMode]);
    const int law = lawFromNorm(pv[kParamLaw]);
    const int shape = shapeFromNorm(pv[kParamShape]);
    const int fxMode = fxModeFromNorm(pv[kParamFxMode]);
    const bool headMode = headOn(pv[kParamHead]);
    const double headA = headSizeCm(pv[kParamHeadSize]) * 0.005;
    const int monitor = monitorFromNorm(pv[kParamMonitor]);
    const double depth = depthFromNorm(pv[kParamDepth]);
    const bool autoOn = depth > 0.0005;
    const double pos = p->displayPos.load();
    const DWORD now = GetTickCount();

    // Nivåmätare: läs högsta värdet sedan förra bilden, fall mjukt
    const float pkL = p->peakL.exchange(0.0f), pkR = p->peakR.exchange(0.0f);
    ed->meterL = pkL > ed->meterL ? pkL : ed->meterL * 0.82f;
    ed->meterR = pkR > ed->meterR ? pkR : ed->meterR * 0.82f;
    if (pkL > 1.0f || pkR > 1.0f) ed->overUntil = now + 600;
    const bool signal = ed->meterL > 0.001f || ed->meterR > 0.001f;

    // --- Header ---
    {
        Font logoFont(&ff, 38, FontStyleBoldItalic, UnitPixel);
        const StringFormat* typo = StringFormat::GenericTypographic();
        RectF trueBox;
        g.MeasureString(L"True", -1, &logoFont, PointF(0.0f, 0.0f), typo, &trueBox);
        SolidBrush white(kText), blue(kCyan);
        g.DrawString(L"True", -1, &logoFont, PointF(21.0f, 7.0f), typo, &white);
        g.DrawString(L"Pan", -1, &logoFont, PointF(21.0f + trueBox.Width, 7.0f), typo, &blue);
        RectF panBox;
        g.MeasureString(L"Pan", -1, &logoFont, PointF(0.0f, 0.0f), typo, &panBox);
        ed->logoRight = 21.0f + trueBox.Width + panBox.Width + 6.0f;   // loggan är klickbar (baksidan)
    }
    drawText(g, L"Psychoacoustic Stereo Panner", ff, 15, FontStyleItalic, kText, 21, 53, 320, StringAlignmentNear);
    drawText(g, L"TruePan  v2.6.1", ff, 15, FontStyleRegular, kText, 555, 32, 300, StringAlignmentFar);
    SolidBrush dot(signal ? kCyan : Color(255, 50, 54, 58));
    g.FillEllipse(&dot, 862.0f, 35.0f, 14.0f, 14.0f);

    if (ed->showBack) {
        paintBack(ed, g, ff);
        Graphics screen(hdc);
        screen.DrawImage(&buffer, 0, 0, width, height);
        return;
    }

    wchar_t txt[64];
    char nb[32];

    // === RAD 1 ===
    // --- PAN ---
    float x0 = colX(0);
    drawPanel(g, x0, kRow1Y, kPanelW, kRow1H);
    drawText(g, L"PAN", ff, 16, FontStyleBold, kText, x0 + 16, kRow1Y + 10, 180, StringAlignmentNear);
    drawKnob(g, ed->knobs[0], pv[kParamPan], false, true);
    panText(pan, nb, sizeof nb, true);
    wideFromNarrow(nb, txt, 64);
    // PAN-värdet i en pil-box: pilarna hoppar mellan stegen 50L ... C ... 50R
    drawButton(g, ff, ed->panDown, L"<", false, kCyan);
    drawButton(g, ff, ed->panUp, L">", false, kCyan);
    {
        const float bx = ed->panDown.x + ed->panDown.w + 4.0f, bw = ed->panUp.x - bx - 4.0f;
        drawBox(g, bx, ed->panDown.y, bw, ed->panDown.h);
        drawText(g, txt, ff, 20, FontStyleBold, kCyan, bx, ed->panDown.y + 1.0f, bw, StringAlignmentCenter);
    }
    drawButton(g, ff, ed->lawDown, L"<", false, kCyan);
    drawButton(g, ff, ed->lawUp, L">", false, kCyan);
    {
        const float bx = ed->lawDown.x + ed->lawDown.w + 4.0f, bw = ed->lawUp.x - bx - 4.0f;
        drawBox(g, bx, ed->lawDown.y, bw, ed->lawDown.h);
        wchar_t lw[16];
        wideFromNarrow(kLawNames[law], lw, 16);
        if (headMode) swprintf(txt, 64, L"HEAD MODEL");      // huvudmodellen ersätter PAN LAW
        else swprintf(txt, 64, L"LAW %ls dB", lw);
        drawText(g, txt, ff, 13, FontStyleBold, headMode ? kTextDim : kCyan, bx, ed->lawDown.y + 6.0f, bw, StringAlignmentCenter);
    }

    // --- MODE + WIDTH + LOW MONO ---
    x0 = colX(1);
    drawPanel(g, x0, kRow1Y, kPanelW, kRow1H);
    drawText(g, L"MODE", ff, 16, FontStyleBold, kText, x0 + 16, kRow1Y + 10, 180, StringAlignmentNear);
    drawButton(g, ff, ed->btnBalance, L"BALANCE", mode == kModeBalance, kOrange);
    drawButton(g, ff, ed->btnStereo, L"STEREO", mode == kModeStereo, kCyan);
    drawText(g, L"WIDTH", ff, 16, FontStyleBold, kText, x0 + 16, kLabelY, 180, StringAlignmentNear);
    drawKnob(g, ed->knobs[1], pv[kParamWidth]);
    swprintf(txt, 64, L"%.0f %%", widthFromNorm(pv[kParamWidth]) * 100.0);
    drawBigValue(g, ff, ed->knobs[1], txt, kText);
    {
        const bool on = lowMonoOn(pv[kParamLowMono]);
        if (on) swprintf(txt, 64, L"%.0f Hz", lowMonoHz(pv[kParamLowMono]));
        else wcscpy(txt, L"Off");
        drawSmallKnob(g, ff, ed->knobs[2], pv[kParamLowMono], L"LOW MONO", txt, !on);
    }

    // --- AUTO-PAN: PAN / TREMOLO / LESLIE ---
    x0 = colX(2);
    drawPanel(g, x0, kRow1Y, kPanelW, kRow1H);
    drawText(g, L"AUTO-PAN", ff, 16, FontStyleBold, kText, x0 + 16, kRow1Y + 10, 180, StringAlignmentNear);
    {
        const wchar_t* fxLabels[kNumFxModes] = { L"PAN", L"TREM", L"LESLIE" };
        for (int i = 0; i < kNumFxModes; ++i)
            drawButton(g, ff, ed->btnFx[i], fxLabels[i], fxMode == i, i == kFxPan ? kCyan : kOrange, 13.0f);
    }
    drawText(g, L"DEPTH", ff, 16, FontStyleBold, kText, x0 + 16, kLabelY, 100, StringAlignmentNear);
    {
        // Väljarrad under PAN/TREM/LESLIE: < vågform >  eller (LESLIE) < modell >
        drawButton(g, ff, ed->shapePrev, L"<", false, kCyan);
        drawButton(g, ff, ed->shapeNext, L">", false, kCyan);
        const RectDef& sb = ed->shapeBox;
        drawBox(g, sb.x, sb.y, sb.w, sb.h);
        const Color& sc = autoOn ? kOrange : kTextDim;
        if (fxMode == kFxLeslie) {
            wchar_t mw[24];
            wideFromNarrow(kLesModelLongNames[lesModelFromNorm(pv[kParamLesModel])], mw, 24);
            drawText(g, mw, ff, 13, FontStyleBold, sc, sb.x, sb.y + 6.0f, sb.w, StringAlignmentCenter);
        } else {
            drawShapeIcon(g, shape, sb.x + 6.0f, sb.y + 8.0f, 22.0f, 14.0f, sc);
            wchar_t sw[24];
            wideFromNarrow(kShapeLongNames[shape], sw, 24);
            // Långa namn (PARADIDDLE-DIDDLE) krymps så att de får plats
            const float tx = sb.x + 30.0f, tw = sb.w - 34.0f;
            float fsz = 12.0f;
            for (; fsz > 7.5f; fsz -= 0.5f) {
                Font f(&ff, fsz, FontStyleBold, UnitPixel);
                RectF box;
                g.MeasureString(sw, -1, &f, PointF(0.0f, 0.0f), &box);
                if (box.Width <= tw + 2.0f) break;
            }
            drawText(g, sw, ff, fsz, FontStyleBold, sc, tx, sb.y + 7.0f + (12.0f - fsz) * 0.6f, tw, StringAlignmentCenter);
        }
    }
    drawKnob(g, ed->knobs[3], pv[kParamDepth]);
    swprintf(txt, 64, L"%.0f %%", depth * 100.0);
    drawBigValue(g, ff, ed->knobs[3], txt, kText);
    if (fxMode == kFxLeslie) {
        // SPEED: STOP / CHORALE / TREMOLO (originalfart) och tempolägen 1/2 ... 1/32, samma stil som RATE
        const int sp = lesSpeedFromNorm(pv[kParamLesSpeed]);
        wchar_t sw[16];
        wideFromNarrow(kLesSpeedLongNames[sp], sw, 16);
        drawSmallKnob(g, ff, ed->knobs[6], pv[kParamLesSpeed], L"SPEED", sw, !autoOn);
    } else {
        wchar_t rw[24];
        wideFromNarrow(kRateNames[rateFromNorm(pv[kParamRate])], rw, 24);
        drawSmallKnob(g, ff, ed->knobs[4], pv[kParamRate], L"RATE", rw, !autoOn);
        if (autoOn && p->synced.load())
            drawText(g, L"SYNC", ff, 10, FontStyleBold, kCyan, ed->knobs[4].cx - 52.0f, ed->knobs[4].cy + 44.0f, 104.0f, StringAlignmentCenter);
    }

    // --- OUTPUT ---
    x0 = colX(3);
    drawPanel(g, x0, kRow1Y, kPanelW, kRow1H);
    drawText(g, L"OUTPUT", ff, 16, FontStyleBold, kText, x0 + 16, kRow1Y + 10, 180, StringAlignmentNear);
    drawKnob(g, ed->knobs[5], pv[kParamOutput]);
    swprintf(txt, 64, L"%.1f dB", outputNormToDb(pv[kParamOutput]));
    drawText(g, txt, ff, 22, FontStyleBold, kText, ed->knobs[5].cx - 70.0f, kBigValueY, 140.0f, StringAlignmentCenter);
    {
        // Over 0 dB uppe till höger i rubrikraden
        const bool over = now < ed->overUntil;
        SolidBrush odot(over ? Color(255, 255, 70, 60) : Color(255, 50, 54, 58));
        g.FillEllipse(&odot, x0 + 122.0f, kRow1Y + 16.0f, 10.0f, 10.0f);
        drawText(g, L"Over 0 dB", ff, 12, over ? FontStyleBold : FontStyleRegular,
                 over ? Color(255, 255, 90, 80) : kTextDim, x0 + 136, kRow1Y + 12.0f, 70, StringAlignmentNear);
    }
    {
        // MONITOR: STEREO / MONO / MID / SIDES
        drawButton(g, ff, ed->monPrev, L"<", false, kCyan);
        drawButton(g, ff, ed->monNext, L">", false, kCyan);
        const float bx = ed->monPrev.x + ed->monPrev.w + 4.0f, bw = ed->monNext.x - bx - 4.0f;
        drawBox(g, bx, ed->monPrev.y, bw, ed->monPrev.h);
        wchar_t mw[16];
        wideFromNarrow(kMonitorLongNames[monitor], mw, 16);
        drawText(g, mw, ff, 13, FontStyleBold, monitor == kMonStereo ? kCyan : kOrange, bx, ed->monPrev.y + 6.0f, bw, StringAlignmentCenter);
    }

    // === RAD 2 ===
    // --- STEREO FIELD: positionen just nu och nivåmätare ---
    drawPanel(g, kFieldX, kRow2Y, kFieldW, kRow2H);
    drawText(g, L"STEREO FIELD", ff, 14, FontStyleBold, kTextDim, kFieldX + 16, kRow2Y + 6, 200, StringAlignmentNear);
    {
        const float fx0 = kFieldX0, fx1 = kFieldX1, fy = kFieldY;   // DISTANCE-ratten till höger
        auto fieldX = [&](double ps) { return fx0 + (float)((ps + 1.0) * 0.5) * (fx1 - fx0); };
        // AUTO-PAN-område (orange band)
        if (autoOn) {
            const float a = fieldX(clampd(pan - depth, -1.0, 1.0)), b = fieldX(clampd(pan + depth, -1.0, 1.0));
            SolidBrush band(Color(70, 255, 160, 40));
            g.FillRectangle(&band, a, fy - 5.0f, b - a, 10.0f);
        }
        Pen trackPen(Color(255, 60, 64, 70), 2.0f);
        g.DrawLine(&trackPen, fx0, fy, fx1, fy);
        // PAN-stegen: 50L 36L 22L 10L C 10R 22R 36R 50R
        Pen tickPen(Color(255, 80, 85, 92), 1.0f);
        for (int i = 0; i < kNumPanSteps; ++i) {
            const int u = kPanSteps[i];
            const float x = fieldX(u / 50.0);
            const float h = (u == 0 || u == 50 || u == -50) ? 8.0f : 6.0f;
            g.DrawLine(&tickPen, x, fy - h, x, fy + h);
            wchar_t tn[8];
            if (u == 0) wcscpy(tn, L"C");
            else swprintf(tn, 8, L"%d%c", u < 0 ? -u : u, u < 0 ? L'L' : L'R');
            drawText(g, tn, ff, 10, FontStyleRegular, kTextDim, x - 22.0f, fy + 9.0f, 44.0f, StringAlignmentCenter);
        }
        // PAN-rattens läge (ring) och verklig position (fylld punkt)
        Pen ringPen(kTextDim, 1.5f);
        g.DrawEllipse(&ringPen, fieldX(pan) - 8.0f, fy - 8.0f, 16.0f, 16.0f);
        // Punkten blir mindre ju längre bort ljudet är
        SolidBrush posB(autoOn ? kOrange : kCyan);
        const float pr = 3.5f + 4.5f * (1.0f - pv[kParamDistance]);
        g.FillEllipse(&posB, fieldX(pos) - pr, fy - pr, pr * 2.0f, pr * 2.0f);

        // Nivåmätare L och R
        const float my[2] = { 405.0f + kDy, 425.0f + kDy };
        const float lv[2] = { ed->meterL, ed->meterR };
        const wchar_t* mn[2] = { L"L", L"R" };
        for (int i = 0; i < 2; ++i) {
            drawText(g, mn[i], ff, 12, FontStyleBold, kTextDim, 38.0f, my[i] - 4.0f, 20.0f, StringAlignmentNear);
            SolidBrush back(Color(255, 30, 33, 37));
            g.FillRectangle(&back, fx0, my[i], fx1 - fx0, 9.0f);
            const float w = meterFrac(lv[i]) * (fx1 - fx0);
            const float zero = meterFrac(1.0f) * (fx1 - fx0);   // 0 dBFS
            SolidBrush barB(kCyan), hotB(Color(255, 255, 70, 60));
            g.FillRectangle(&barB, fx0, my[i], w < zero ? w : zero, 9.0f);
            if (w > zero) g.FillRectangle(&hotB, fx0 + zero, my[i], w - zero, 9.0f);
            // Skalstreck i staplarna
            const int marks[7] = { -30, -24, -18, -12, -6, -3, 0 };
            for (int k = 0; k < 7; ++k) {
                const float mx = fx0 + meterFrac((float)dbToGain(marks[k])) * (fx1 - fx0);
                Pen mp(marks[k] == 0 ? kTextDim : Color(255, 70, 74, 80), 1.0f);
                g.DrawLine(&mp, mx, my[i] - 2.0f, mx, my[i] + 11.0f);
            }
        }
        // Skala under mätarna: -30, -24, -18, -12, -6, -3, 0 dB (mätaren börjar vid -30 dB)
        {
            const int marks[7] = { -30, -24, -18, -12, -6, -3, 0 };
            for (int k = 0; k < 7; ++k) {
                const float mx = fx0 + meterFrac((float)dbToGain(marks[k])) * (fx1 - fx0);
                wchar_t ml[16];
                if (marks[k] == 0) wcscpy(ml, L"0 dB");
                else swprintf(ml, 16, L"%d", marks[k]);
                drawText(g, ml, ff, 10, FontStyleRegular, kTextDim, mx - 20.0f, 436.0f + kDy, 40.0f, StringAlignmentCenter);
            }
        }
    }

    {
        // DISTANCE
        const double dm = distanceM(pv[kParamDistance]);
        if (dm < 10.0) swprintf(txt, 64, L"%.2f m", dm);
        else if (dm < 99.95) swprintf(txt, 64, L"%.1f m", dm);
        else swprintf(txt, 64, L"%.0f m", dm);
        // 1 m = klockan 9; bågen (blå) växer därifrån åt båda hållen
        drawSmallKnob(g, ff, ed->knobs[7], pv[kParamDistance], L"DISTANCE", txt, false, nullptr, true, nullptr, kDistRefNorm);
    }

    // --- PRESET ---
    x0 = colX(3);
    drawPanel(g, x0, kRow2Y, kPanelW, kRow2H);
    drawText(g, L"PRESET", ff, 14, FontStyleBold, kTextDim, x0 + 16, kRow2Y + 6, 180, StringAlignmentNear);
    {
        const float bx = ed->presetPrev.x + ed->presetPrev.w + 4.0f, bw = ed->presetNext.x - bx - 4.0f;
        drawBox(g, bx, ed->presetPrev.y, bw, 30.0f);
        wchar_t nameW[40];
        wideFromNarrow(p->presetName, nameW, 40);
        drawText(g, nameW, ff, 12, FontStyleBold, kCyan, bx, ed->presetPrev.y + 7.0f, bw, StringAlignmentCenter);
    }
    drawButton(g, ff, ed->presetPrev, L"<", false, kCyan);
    drawButton(g, ff, ed->presetNext, L">", false, kCyan);
    drawButton(g, ff, ed->btnSave, L"SAVE", false, kCyan);
    drawButton(g, ff, ed->btnLoad, L"LOAD", false, kCyan);

    // === PAN LAW-graf / HEAD-graf ===
    drawPanel(g, 20, kGraphY, 860, kGraphH);
    drawText(g, headMode ? L"HEAD" : L"PAN LAW", ff, 16, FontStyleBold, kText, 38, kGraphY + 9, 140, StringAlignmentNear);
    {
        // HEAD-knapp, huvudstorlek och tidsskillnaden just nu
        drawButton(g, ff, ed->btnHead, L"HEAD", headMode, kOrange, 13.0f);
        drawButton(g, ff, ed->headDown, L"<", false, kCyan);
        drawButton(g, ff, ed->headUp, L">", false, kCyan);
        const float bx = ed->headDown.x + ed->headDown.w + 4.0f, bw = ed->headUp.x - bx - 4.0f;
        drawBox(g, bx, ed->headDown.y, bw, ed->headDown.h);
        swprintf(txt, 64, L"%.1f cm", headSizeCm(pv[kParamHeadSize]));
        drawText(g, txt, ff, 12, FontStyleBold, headMode ? kCyan : kTextDim, bx, ed->headDown.y + 5.0f, bw, StringAlignmentCenter);
        if (headMode) {
            const double itd = std::fabs(headItd(pos * 0.5 * kPi, headA)) * 1000.0;
            swprintf(txt, 64, L"ITD %.2f ms", itd);
            drawText(g, txt, ff, 12, FontStyleBold, kOrange, 482, kGraphY + 17, 110, StringAlignmentNear);
        }
    }
    kPlotDbMax = headMode ? 12.0f : 6.0f;

    Pen grid(kGrid, 1.0f);
    const int dbsLaw[4] = { -24, -12, 0, 6 };
    const int dbsHead[4] = { -24, -12, 0, 12 };
    const int* dbs = headMode ? dbsHead : dbsLaw;
    for (int i = 0; i < 4; ++i) {
        const float y = plotY(dbs[i]);
        g.DrawLine(&grid, kPlotX0, y, kPlotX1, y);
        wchar_t lab[16];
        swprintf(lab, 16, dbs[i] > 0 ? L"+%d dB" : L"%d dB", dbs[i]);
        drawText(g, lab, ff, 11, FontStyleRegular, kTextDim, 26, y - 8, 56, StringAlignmentFar);
    }
    const double gx[5] = { -1.0, -0.5, 0.0, 0.5, 1.0 };
    const wchar_t* gxn[5] = { L"50L", L"25L", L"C", L"25R", L"50R" };
    for (int i = 0; i < 5; ++i) {
        const float x = plotX(gx[i]);
        g.DrawLine(&grid, x, kPlotY0, x, kPlotY1);
        drawText(g, gxn[i], ff, 11, FontStyleRegular, kTextDim, x - 30, kPlotY1 + 2, 60, StringAlignmentCenter);
    }

    Pen ln(kCyan, 3.0f), lr(kOrange, 3.0f), lt(kText, 1.5f);
    lt.SetDashStyle(DashStyleDash);
    if (headMode) {
        // Varje öras nivå: heldragen = 4 kHz (huvudskuggan tydlig), streckad = 500 Hz (nästan ingen)
        drawHeadCurve(g, false, 500.0, headA, kCyan, 1.5f, true);
        drawHeadCurve(g, true, 500.0, headA, kOrange, 1.5f, true);
        drawHeadCurve(g, false, 4000.0, headA, kCyan, 3.0f, false);
        drawHeadCurve(g, true, 4000.0, headA, kOrange, 3.0f, false);
        SolidBrush ml(kCyan), mr(kOrange);
        const double az = pos * 0.5 * kPi;
        g.FillEllipse(&ml, plotX(pos) - 5.0f, plotY(headEarGainDb(az, false, 4000.0, headA)) - 5.0f, 10.0f, 10.0f);
        g.FillEllipse(&mr, plotX(pos) - 5.0f, plotY(headEarGainDb(az, true, 4000.0, headA)) - 5.0f, 10.0f, 10.0f);
        g.DrawLine(&ln, 661.0f, kGraphY + 18.0f, 685.0f, kGraphY + 18.0f);
        drawText(g, L"LEFT EAR", ff, 12, FontStyleRegular, kText, 689, kGraphY + 10, 70, StringAlignmentNear);
        g.DrawLine(&lr, 762.0f, kGraphY + 18.0f, 786.0f, kGraphY + 18.0f);
        drawText(g, L"RIGHT EAR", ff, 12, FontStyleRegular, kText, 790, kGraphY + 10, 80, StringAlignmentNear);
        drawText(g, L"Spherical head model: Woodworth ITD, Brown & Duda head shadow  \u00B7  solid = 4 kHz  \u00B7  dashed = 500 Hz  \u00B7  center is unchanged",
                 ff, 11, FontStyleRegular, kTextDim, 20, kGraphY + kGraphH - 22.0f, 860, StringAlignmentCenter);
    } else {
        drawLawCurve(g, law, kCurveTotal, kText, 1.5f, true);
        drawLawCurve(g, law, kCurveLeft, kCyan, 3.0f, false);
        drawLawCurve(g, law, kCurveRight, kOrange, 3.0f, false);
        double gl, gr;
        panGains(pos, law, gl, gr);
        SolidBrush ml(kCyan), mr(kOrange);
        g.FillEllipse(&ml, plotX(pos) - 5.0f, plotY(gainToDb(gl)) - 5.0f, 10.0f, 10.0f);
        g.FillEllipse(&mr, plotX(pos) - 5.0f, plotY(gainToDb(gr)) - 5.0f, 10.0f, 10.0f);
        g.DrawLine(&ln, 607.0f, kGraphY + 18.0f, 631.0f, kGraphY + 18.0f);
        drawText(g, L"LEFT", ff, 12, FontStyleRegular, kText, 635, kGraphY + 10, 60, StringAlignmentNear);
        g.DrawLine(&lr, 679.0f, kGraphY + 18.0f, 703.0f, kGraphY + 18.0f);
        drawText(g, L"RIGHT", ff, 12, FontStyleRegular, kText, 707, kGraphY + 10, 60, StringAlignmentNear);
        g.DrawLine(&lt, 761.0f, kGraphY + 18.0f, 785.0f, kGraphY + 18.0f);
        drawText(g, L"LOUDNESS", ff, 12, FontStyleRegular, kText, 789, kGraphY + 10, 90, StringAlignmentNear);
        drawText(g, L"Center is always 0 dB  \u00B7  LOW MONO bass stays in the center  \u00B7  dashed = total loudness of a mono source",
                 ff, 11, FontStyleRegular, kTextDim, 20, kGraphY + kGraphH - 22.0f, 860, StringAlignmentCenter);
    }

    Graphics screen(hdc);
    screen.DrawImage(&buffer, 0, 0, width, height);
}

void setParamUI(Editor* ed, int param, float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    if (param == kParamDistance) {
        // Närmare än 1 m höjer nivån (avståndslagen). OUTPUT sänks lika mycket, så att
        // man ser höjningen men den totala volymen står still.
        Core* p = ed->plugin;
        const double delta = distanceNearBoostDb(v) - distanceNearBoostDb(p->params[kParamDistance].load());
        if (std::fabs(delta) > 1e-9) {
            const double outDb = outputNormToDb(p->params[kParamOutput].load()) - delta;
            p->setParamFromEditor(kParamOutput, outputDbToNorm(outDb));
        }
    }
    ed->plugin->setParamFromEditor(param, v);
    InvalidateRect(ed->hwnd, nullptr, FALSE);
}

void setParamOnce(Editor* ed, int param, float v)
{
    ed->plugin->beginEdit(param);
    if (param == kParamDistance) ed->plugin->beginEdit(kParamOutput);   // OUTPUT följer med
    setParamUI(ed, param, v);
    if (param == kParamDistance) ed->plugin->endEdit(kParamOutput);
    ed->plugin->endEdit(param);
}

// Piltangenterna vänster/höger: PAN till nästa steg. Värden kan skicka samma
// tangent både som fönstermeddelande och via sitt eget anrop, så dubbletter
// inom 15 ms räknas bara en gång.
bool panKey(Editor* ed, int dir)
{
    if (!ed || ed->showBack) return false;
    const DWORD now = GetTickCount();
    if (dir == ed->lastKeyDir && now - ed->lastKeyTime < 15) return true;
    ed->lastKeyDir = dir;
    ed->lastKeyTime = now;
    setParamOnce(ed, kParamPan, panNextStep(ed->plugin->params[kParamPan].load(), dir));
    return true;
}

void stepChoice(Editor* ed, int param, int count, int step, bool wrap)
{
    int i = choiceFromNorm(ed->plugin->params[param].load(), count) + step;
    if (wrap) { if (i < 0) i = count - 1; if (i >= count) i = 0; }
    else if (i < 0 || i >= count) return;
    setParamOnce(ed, param, choiceNorm(i, count));
}

void presetsChanged(Editor* ed)
{
    Core* p = ed->plugin;
    p->hostUpdateDisplay();
    InvalidateRect(ed->hwnd, nullptr, FALSE);
}

// --- Presetmapp: Dokument\TruePan Presets (skapas automatiskt) -------------
std::wstring presetFolder()
{
    wchar_t docs[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_PERSONAL | CSIDL_FLAG_CREATE, nullptr, 0, docs) != S_OK) return std::wstring();
    std::wstring dir = std::wstring(docs) + L"\\TruePan Presets";
    CreateDirectoryW(dir.c_str(), nullptr); // gör inget om mappen redan finns
    return dir;
}

bool fileExists(const std::wstring& path)
{
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring widen(const std::string& s)
{
    std::wstring w;
    for (char c : s) w += (wchar_t)(unsigned char)c;
    return w;
}

// Filnamnet utan mapp och filändelse, som säkert presetnamn.
std::string presetNameFromPath(const wchar_t* path)
{
    const wchar_t* base = path;
    for (const wchar_t* c = path; *c; ++c) if (*c == L'\\' || *c == L'/') base = c + 1;
    std::string out;
    for (const wchar_t* c = base; *c; ++c) {
        if (*c == L'.' && wcschr(c + 1, L'.') == nullptr) break; // sista punkten = filändelse
        out += (*c >= 32 && *c < 127) ? (char)*c : '_';
    }
    return cleanPresetName(out);
}

// Egna presets i presetmappen, sorterade efter namn.
std::vector<std::wstring> listUserPresets(const std::wstring& dir)
{
    std::vector<std::wstring> files;
    if (dir.empty()) return files;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*.tpp").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return files;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) files.push_back(dir + L"\\" + fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(files.begin(), files.end(), [](const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    return files;
}

bool readPresetFile(const std::wstring& path, float* v)
{
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && text.size() < 65536) text.append(buf, n);
    std::fclose(f);
    std::string name;
    return presetFromText(text, v, &name);
}

// Laddar en egen preset. Namnet tas från filnamnet (samma som vid SAVE).
bool loadUserPreset(Editor* ed, const std::wstring& path, bool showErrors)
{
    Core* p = ed->plugin;
    float v[kNumParams];
    if (!readPresetFile(path, v)) {
        if (showErrors) MessageBoxW(ed->hwnd, L"This file is not a TruePan preset.", L"TruePan", MB_OK | MB_ICONWARNING);
        return false;
    }
    p->applyValues(v, true);
    copyString(p->presetName, presetNameFromPath(path.c_str()).c_str(), sizeof p->presetName);
    p->program = -1;
    presetsChanged(ed);
    return true;
}

// Pilarna i PRESET: först fabrikspresetsen, sedan egna presets i mappen.
void stepPreset(Editor* ed, int step)
{
    Core* p = ed->plugin;
    const std::vector<std::wstring> user = listUserPresets(presetFolder());
    const int total = kNumFactoryPresets + (int)user.size();

    int pos = -1;
    if (p->program >= 0) pos = p->program;
    else {
        for (size_t i = 0; i < user.size(); ++i)
            if (presetNameFromPath(user[i].c_str()) == p->presetName) { pos = kNumFactoryPresets + (int)i; break; }
    }
    if (pos < 0) pos = step > 0 ? -1 : total;
    for (int tries = 0; tries < total; ++tries) {
        pos += step;
        if (pos < 0) pos = total - 1;
        if (pos >= total) pos = 0;
        if (pos < kNumFactoryPresets) { p->loadFactoryPreset(pos, true); presetsChanged(ed); return; }
        if (loadUserPreset(ed, user[pos - kNumFactoryPresets], false)) return; // hoppa över trasiga filer
    }
}

void savePresetFile(Editor* ed)
{
    Core* p = ed->plugin;
    const std::wstring dir = presetFolder();
    const std::string suggestion = uniquePresetName(p->presetName, [&](const std::string& n) {
        return !dir.empty() && fileExists(dir + L"\\" + widen(n) + L".tpp");
    });
    wchar_t file[MAX_PATH];
    wideFromNarrow(suggestion.c_str(), file, MAX_PATH);
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = ed->hwnd;
    ofn.lpstrFilter = L"TruePan preset (*.tpp)\0*.tpp\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = dir.empty() ? nullptr : dir.c_str();
    ofn.lpstrDefExt = L"tpp";
    ofn.lpstrTitle = L"Save TruePan preset";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) return;

    float v[kNumParams];
    for (int i = 0; i < kNumParams; ++i) v[i] = p->params[i].load();
    const std::string name = presetNameFromPath(file);
    const std::string text = presetToText(name, v);
    FILE* f = _wfopen(file, L"wb");
    if (!f || std::fwrite(text.data(), 1, text.size(), f) != text.size()) {
        if (f) std::fclose(f);
        MessageBoxW(ed->hwnd, L"The preset could not be saved.", L"TruePan", MB_OK | MB_ICONWARNING);
        return;
    }
    std::fclose(f);
    copyString(p->presetName, name.c_str(), sizeof p->presetName);
    p->program = -1;
    presetsChanged(ed);
}

void loadPresetFile(Editor* ed)
{
    const std::wstring dir = presetFolder();
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = ed->hwnd;
    ofn.lpstrFilter = L"TruePan preset (*.tpp)\0*.tpp\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = dir.empty() ? nullptr : dir.c_str();
    ofn.lpstrTitle = L"Load TruePan preset";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return;
    loadUserPreset(ed, file, true);
}

bool handleButtons(Editor* ed, int x, int y)
{
    Core* p = ed->plugin;
    if (ed->btnBalance.hit(x, y)) { setParamOnce(ed, kParamMode, 0.0f); return true; }
    if (ed->btnStereo.hit(x, y))  { setParamOnce(ed, kParamMode, 1.0f); return true; }
    if (ed->panDown.hit(x, y))    { setParamOnce(ed, kParamPan, panNextStep(ed->plugin->params[kParamPan].load(), -1)); return true; }
    if (ed->panUp.hit(x, y))      { setParamOnce(ed, kParamPan, panNextStep(ed->plugin->params[kParamPan].load(), +1)); return true; }
    if (ed->lawDown.hit(x, y))    { stepChoice(ed, kParamLaw, kNumLaws, -1, false); return true; }
    if (ed->lawUp.hit(x, y))      { stepChoice(ed, kParamLaw, kNumLaws, +1, false); return true; }
    if (ed->monPrev.hit(x, y))    { stepChoice(ed, kParamMonitor, kNumMonitors, -1, true); return true; }
    if (ed->monNext.hit(x, y))    { stepChoice(ed, kParamMonitor, kNumMonitors, +1, true); return true; }
    for (int i = 0; i < kNumFxModes; ++i)
        if (ed->btnFx[i].hit(x, y)) { setParamOnce(ed, kParamFxMode, choiceNorm(i, kNumFxModes)); return true; }
    if (ed->shapePrev.hit(x, y) || ed->shapeNext.hit(x, y) || ed->shapeBox.hit(x, y)) {
        const int step = ed->shapePrev.hit(x, y) ? -1 : +1;          // rutan själv = nästa
        if (ed->leslieMode()) stepChoice(ed, kParamLesModel, kNumLesModels, step, true);
        else stepChoice(ed, kParamShape, kNumShapes, step, true);
        return true;
    }
    if (ed->btnHead.hit(x, y)) {
        setParamOnce(ed, kParamHead, headOn(ed->plugin->params[kParamHead].load()) ? 0.0f : 1.0f);
        return true;
    }
    if (ed->headDown.hit(x, y) || ed->headUp.hit(x, y)) {
        // 0,5 cm per klick
        const float step = 0.5f / (float)(kHeadMaxCm - kHeadMinCm);
        const float v = ed->plugin->params[kParamHeadSize].load() + (ed->headUp.hit(x, y) ? step : -step);
        setParamOnce(ed, kParamHeadSize, v);
        return true;
    }
    if (ed->presetPrev.hit(x, y)) { stepPreset(ed, -1); return true; }
    if (ed->presetNext.hit(x, y)) { stepPreset(ed, +1); return true; }
    if (ed->btnSave.hit(x, y))    { savePresetFile(ed); return true; }
    if (ed->btnLoad.hit(x, y))    { loadPresetFile(ed); return true; }
    (void)p;
    return false;
}

float wheelStep(int param)
{
    switch (param) {
    case kParamPan: return 0.01f;                                                // 1 steg (1L/1R)
    case kParamOutput: return 0.5f / (float)(kOutputMaxDb - kOutputMinDb);       // 0,5 dB
    case kParamRate: return 1.0f / (kNumRates - 1);                              // en notlängd
    case kParamLesSpeed: return 1.0f / (kNumLesSpeeds - 1);                      // ett steg
    default: return 0.01f;
    }
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Editor* ed = reinterpret_cast<Editor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        ed = static_cast<Editor*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)ed);
        ed->hwnd = hwnd;
        SetTimer(hwnd, 1, 33, nullptr); // ~30 bilder/s
        return 0;
    }
    case WM_TIMER:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (ed) {
            RECT rc; GetClientRect(hwnd, &rc);
            paint(ed, hdc, rc.right - rc.left, rc.bottom - rc.top);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SETCURSOR:
        if (ed && LOWORD(lParam) == HTCLIENT) {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            if (ed->overLogo(pt.x, pt.y) || (ed->showBack && ed->btnBack.hit(pt.x, pt.y))) {
                SetCursor(LoadCursor(nullptr, IDC_HAND));
                return TRUE;
            }
            if (!ed->showBack && (ed->overField(pt.x, pt.y) || ed->dragField)) {
                SetCursor(LoadCursor(nullptr, IDC_SIZEWE));   // dra i sidled
                return TRUE;
            }
        }
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        if (!ed) return 0;
        const int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
        // Loggan vänder pluginet (fram- och baksida)
        if (ed->overLogo(x, y) || (ed->showBack && ed->btnBack.hit(x, y))) {
            ed->showBack = !ed->showBack;
            ed->scroll = 0.0f;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (ed->showBack) {
            // Rullningslisten: dra i den, eller klicka ovanför/under för en sida
            if (x >= kThumbX - 6 && x <= kThumbX + kThumbW + 6 && y >= kManTop && y <= kManBottom && manualMaxScroll(ed) > 0.0f) {
                const float viewH = kManBottom - kManTop, maxScroll = manualMaxScroll(ed);
                const float th = std::max(30.0f, viewH * viewH / ed->contentH);
                const float ty = kManTop + (viewH - th) * (ed->scroll / maxScroll);
                if (y < ty) ed->scroll -= viewH * 0.9f;
                else if (y > ty + th) ed->scroll += viewH * 0.9f;
                else { ed->dragThumb = true; ed->dragThumbY = y; ed->dragScroll0 = ed->scroll; SetCapture(hwnd); }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        if (handleButtons(ed, x, y)) return 0;
        // STEREO FIELD: klicka/dra i sidled flyttar PAN, dubbelklick = C
        if (ed->overField(x, y)) {
            if (msg == WM_LBUTTONDBLCLK) { setParamOnce(ed, kParamPan, 0.5f); return 0; }
            ed->dragField = true;
            ed->plugin->beginEdit(kParamPan);
            setParamUI(ed, kParamPan, fieldPanNorm(x));
            SetCapture(hwnd);
            return 0;
        }
        const int k = ed->knobAt(x, y);
        if (k < 0) return 0;
        const int param = ed->knobs[k].param;
        if (msg == WM_LBUTTONDBLCLK) {
            setParamOnce(ed, param, defaultParam(param)); // dubbelklick = standardvärde
            return 0;
        }
        ed->dragKnob = k;
        ed->dragStartY = y;
        ed->dragStartValue = ed->plugin->params[param].load();
        ed->plugin->beginEdit(param);
        if (param == kParamDistance) ed->plugin->beginEdit(kParamOutput);   // OUTPUT följer med
        SetCapture(hwnd);
        return 0;
    }
    case WM_MOUSELEAVE:
        // Musen lämnade fönstret: ge tillbaka tangentbordsfokus till Live
        if (ed) {
            ed->tracking = false;
            if (GetFocus() == hwnd && ed->prevFocus && IsWindow(ed->prevFocus)) SetFocus(ed->prevFocus);
            ed->prevFocus = nullptr;
        }
        return 0;
    case WM_MOUSEMOVE:
        // Windows 7 skickar mushjulet till fönstret som har fokus: ta fokus medan musen
        // är över TruePan, så att hjulet fungerar (manualen och rattarna)
        if (ed && !ed->tracking) {
            HWND f = GetFocus();
            if (f != hwnd) { ed->prevFocus = f; SetFocus(hwnd); }
            TRACKMOUSEEVENT tme = {};
            tme.cbSize = sizeof tme;
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            if (TrackMouseEvent(&tme)) ed->tracking = true;
        }
        if (ed && ed->dragThumb) {
            const float viewH = kManBottom - kManTop, maxScroll = manualMaxScroll(ed);
            const float th = std::max(30.0f, viewH * viewH / std::max(ed->contentH, 1.0f));
            if (viewH - th > 1.0f) ed->scroll = ed->dragScroll0 + ((short)HIWORD(lParam) - ed->dragThumbY) * maxScroll / (viewH - th);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (ed && ed->dragField) {
            setParamUI(ed, kParamPan, fieldPanNorm((short)LOWORD(lParam)));
            return 0;
        }
        if (ed && ed->dragKnob >= 0) {
            const int dy = ed->dragStartY - (short)HIWORD(lParam);
            const float pixelsForFullRange = (wParam & MK_SHIFT) ? 1000.0f : 250.0f; // Shift = finjustering
            setParamUI(ed, ed->knobs[ed->dragKnob].param, ed->dragStartValue + dy / pixelsForFullRange);
        }
        return 0;
    case WM_LBUTTONUP:
        if (ed && ed->dragField) {
            ed->dragField = false;
            ed->plugin->endEdit(kParamPan);
            ReleaseCapture();
            return 0;
        }
        if (ed && ed->dragThumb) {
            ed->dragThumb = false;
            ReleaseCapture();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (ed && ed->dragKnob >= 0) {
            ed->plugin->endEdit(ed->knobs[ed->dragKnob].param);
            if (ed->knobs[ed->dragKnob].param == kParamDistance) ed->plugin->endEdit(kParamOutput);
            ed->dragKnob = -1;
            ReleaseCapture();
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (ed) {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            ScreenToClient(hwnd, &pt);
            if (ed->showBack) {
                ed->scroll -= (float)GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 48.0f;   // 48 px per hack
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            const int k = ed->knobAt(pt.x, pt.y);
            if (k >= 0) {
                const int param = ed->knobs[k].param;
                const int clicks = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
                setParamOnce(ed, param, ed->plugin->params[param].load() + clicks * wheelStep(param));
            }
        }
        return 0;
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS | DLGC_WANTCHARS;
    case WM_KEYDOWN:
        if (ed && (wParam == VK_LEFT || wParam == VK_RIGHT) && panKey(ed, wParam == VK_LEFT ? -1 : 1)) return 0;
        break;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

namespace truepan {

void editorOpen(Core* p, void* parent)
{
    if (p->editor || !parent) return;

    if (!g_instance) {
        // Modulens handtag (DLL:en), oavsett om den laddats som VST2 eller VST3
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&windowProc), &g_instance);
    }
    if (g_gdiplusUsers++ == 0) {
        GdiplusStartupInput input;
        GdiplusStartup(&g_gdiplusToken, &input, nullptr);
    }
    if (!g_classRegistered) {
        WNDCLASSW wc = {};
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = windowProc;
        wc.hInstance = g_instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kWindowClass;
        if (RegisterClassW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) g_classRegistered = true;
    }

    Editor* ed = new (std::nothrow) Editor();
    if (!ed) return;
    ed->plugin = p;
    p->editor = ed;
    CreateWindowExW(0, kWindowClass, L"TruePan", WS_CHILD | WS_VISIBLE,
                    0, 0, kEditorWidth, kEditorHeight,
                    static_cast<HWND>(parent), nullptr, g_instance, ed);
}

bool editorKey(Core* p, int dir)
{
    return panKey(static_cast<Editor*>(p->editor), dir);
}

void editorClose(Core* p)
{
    Editor* ed = static_cast<Editor*>(p->editor);
    if (!ed) return;
    if (ed->hwnd) DestroyWindow(ed->hwnd);
    delete ed;
    p->editor = nullptr;
    if (--g_gdiplusUsers == 0) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

// Ritar fönstret (framsida eller baksida) till en PNG-fil utan att öppna något
// fönster. Används av RenderGui (bilder av gränssnittet i bygget).
bool editorRenderPng(Core* core, bool backSide, const wchar_t* path)
{
    GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (GdiplusStartup(&token, &input, nullptr) != Ok) return false;
    bool ok = false;
    {
        Editor ed;
        ed.plugin = core;
        ed.showBack = backSide;
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = kEditorWidth;
        bi.bmiHeader.biHeight = -kEditorHeight;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HDC screen = GetDC(nullptr);
        HDC mem = CreateCompatibleDC(screen);
        HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (mem && dib) {
            HGDIOBJ old = SelectObject(mem, dib);
            paint(&ed, mem, kEditorWidth, kEditorHeight);
            GdiFlush();
            SelectObject(mem, old);
            Bitmap* bmp = Bitmap::FromHBITMAP(dib, nullptr);
            if (bmp) {
                UINT num = 0, size = 0;
                GetImageEncodersSize(&num, &size);
                std::vector<unsigned char> buf(size);
                ImageCodecInfo* codecs = reinterpret_cast<ImageCodecInfo*>(buf.data());
                GetImageEncoders(num, size, codecs);
                for (UINT i = 0; i < num; ++i) {
                    if (wcscmp(codecs[i].MimeType, L"image/png") == 0) {
                        ok = bmp->Save(path, &codecs[i].Clsid, nullptr) == Ok;
                        break;
                    }
                }
                delete bmp;
            }
        }
        if (dib) DeleteObject(dib);
        if (mem) DeleteDC(mem);
        ReleaseDC(nullptr, screen);
    }
    GdiplusShutdown(token);
    return ok;
}

} // namespace truepan


