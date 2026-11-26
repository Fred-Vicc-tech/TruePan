// TruePanTests.cpp - automatiska tester av ljudmotorn (TruePanDSP.h).
// Körs i varje bygge på GitHub. Skriver PASS/FAIL per test.
#include "TruePanDSP.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace truepan;

static int failures = 0;

static void check(bool ok, const char* name)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}

struct Params
{
    float v[kNumParams];
    Params() { for (int i = 0; i < kNumParams; ++i) v[i] = defaultParam(i); }
};

static const double kSr = 48000.0;

// Kör motorn på en signal (n sampel). Returnerar utsignalen.
static void run(Engine& e, const Params& p, const std::vector<float>& inL, const std::vector<float>& inR,
                std::vector<float>& outL, std::vector<float>& outR, int block = 256)
{
    const int n = (int)inL.size();
    outL.assign(n, 0.0f);
    outR.assign(n, 0.0f);
    for (int pos = 0; pos < n; pos += block) {
        const int m = pos + block <= n ? block : n - pos;
        e.setParams(p.v);
        e.process(&inL[pos], &inR[pos], &outL[pos], &outR[pos], m, false);
    }
}

static std::vector<float> sine(double hz, int n, double amp = 0.5)
{
    std::vector<float> s(n);
    for (int i = 0; i < n; ++i) s[i] = (float)(amp * std::sin(2.0 * kPi * hz * i / kSr));
    return s;
}

static double rms(const std::vector<float>& s, int from)
{
    double a = 0.0;
    int c = 0;
    for (size_t i = (size_t)from; i < s.size(); ++i) { a += (double)s[i] * s[i]; ++c; }
    return c ? std::sqrt(a / c) : 0.0;
}

static double db(double x) { return gainToDb(x); }

int main()
{
    // ------------------------------------------------------------------
    // Standardvärden: ljudet går igenom oförändrat
    // ------------------------------------------------------------------
    {
        Engine e; e.prepare(kSr);
        Params p;
        const int n = 20000;
        std::vector<float> l(n), r(n), ol, orr;
        std::srand(1);
        for (int i = 0; i < n; ++i) { l[i] = (float)(std::rand() / (double)RAND_MAX - 0.5); r[i] = (float)(std::rand() / (double)RAND_MAX - 0.5); }
        run(e, p, l, r, ol, orr);
        double maxErr = 0.0;
        for (int i = 0; i < n; ++i) maxErr = std::fmax(maxErr, std::fmax(std::fabs(ol[i] - l[i]), std::fabs(orr[i] - r[i])));
        check(maxErr < 1e-6, "standardlage: ljudet gar igenom oforandrat (stereo in = stereo ut)");

        // Samma minne för in och ut (som Live ibland gör)
        Engine e2; e2.prepare(kSr);
        std::vector<float> bl = l, br = r;
        for (int pos = 0; pos < n; pos += 256) {
            const int m = pos + 256 <= n ? 256 : n - pos;
            e2.setParams(p.v);
            e2.process(&bl[pos], &br[pos], &bl[pos], &br[pos], m, false);
        }
        maxErr = 0.0;
        for (int i = 0; i < n; ++i) maxErr = std::fmax(maxErr, std::fmax(std::fabs(bl[i] - l[i]), std::fabs(br[i] - r[i])));
        check(maxErr < 1e-6, "fungerar nar in- och utbuffert ar samma minne");
    }

    // ------------------------------------------------------------------
    // PAN LAW
    // ------------------------------------------------------------------
    {
        bool centerOk = true;
        for (int law = 0; law < kNumLaws; ++law) {
            double gl, gr;
            panGains(0.0, law, gl, gr);
            centerOk = centerOk && std::fabs(gl - 1.0) < 1e-12 && std::fabs(gr - 1.0) < 1e-12;
        }
        check(centerOk, "alla PAN LAW: mitten = 0 dB i bada kanalerna");

        const double expectDb[kNumLaws] = { 0.0, 3.0103, 4.5154, 6.0206 };
        bool sideOk = true;
        for (int law = 0; law < kNumLaws; ++law) {
            double gl, gr;
            panGains(-1.0, law, gl, gr);
            sideOk = sideOk && std::fabs(db(gl) - expectDb[law]) < 0.01 && gr == 0.0;
            panGains(1.0, law, gl, gr);
            sideOk = sideOk && std::fabs(db(gr) - expectDb[law]) < 0.01 && gl == 0.0;
        }
        check(sideOk, "helt at sidan: bortre kanalen tyst, narmaste +0 / +3 / +4,5 / +6 dB");

        bool powerOk = true;
        for (int i = 0; i <= 200; ++i) {
            double gl, gr;
            panGains(-1.0 + i / 100.0, kLaw3, gl, gr);
            powerOk = powerOk && std::fabs(gl * gl + gr * gr - 2.0) < 1e-9;
        }
        check(powerOk, "-3 dB: monoljud lika starkt i alla positioner (konstant effekt)");

        bool monoOk = true;
        for (int law = 0; law < kNumLaws; ++law) {
            double pl = 1e9, pr = -1e9;
            for (int i = 0; i <= 200; ++i) {
                double gl, gr;
                panGains(-1.0 + i / 100.0, law, gl, gr);
                // vänster ska aldrig öka när man vrider åt höger
                monoOk = monoOk && gl <= pl + 1e-12 && gr >= pr - 1e-12;
                pl = gl; pr = gr;
            }
        }
        check(monoOk, "PAN LAW-kurvorna ar jamna (ingen hacka nar man vrider)");
    }

    // ------------------------------------------------------------------
    // BALANCE och STEREO
    // ------------------------------------------------------------------
    {
        const int n = 24000;
        std::vector<float> l = sine(1000.0, n), silent(n, 0.0f), ol, orr;
        Params p;
        p.v[kParamPan] = 1.0f;          // helt höger
        p.v[kParamMode] = 0.0f;         // BALANCE
        Engine e; e.prepare(kSr);
        run(e, p, l, silent, ol, orr);
        check(rms(ol, 4000) < 1e-6 && rms(orr, 4000) < 1e-6, "BALANCE helt hoger: ljud som bara finns i vanster forsvinner");

        p.v[kParamMode] = 1.0f;         // STEREO
        Engine e2; e2.prepare(kSr);
        run(e2, p, l, silent, ol, orr);
        const double inRms = rms(l, 4000);
        check(rms(ol, 4000) < 1e-6 && db(rms(orr, 4000) / inRms) > -3.5,
              "STEREO helt hoger: vanster kanals ljud flyttas till hoger (inget forsvinner)");

        // Monoljud: BALANCE och STEREO ger exakt samma resultat
        std::vector<float> m = sine(440.0, n), a1, a2, b1, b2;
        bool same = true;
        const float pans[5] = { 0.0f, 0.2f, 0.5f, 0.77f, 1.0f };
        for (int k = 0; k < 5; ++k) {
            Params q; q.v[kParamPan] = pans[k];
            q.v[kParamMode] = 0.0f; Engine x; x.prepare(kSr); run(x, q, m, m, a1, a2);
            q.v[kParamMode] = 1.0f; Engine y; y.prepare(kSr); run(y, q, m, m, b1, b2);
            for (int i = 0; i < n; ++i) same = same && std::fabs(a1[i] - b1[i]) < 1e-5 && std::fabs(a2[i] - b2[i]) < 1e-5;
        }
        check(same, "monoljud: BALANCE och STEREO later exakt likadant");

        // Monoljud med -3 dB: total effekt konstant vid alla positioner
        bool flat = true;
        for (int k = 0; k < 5; ++k) {
            Params q; q.v[kParamPan] = pans[k];
            Engine x; x.prepare(kSr); run(x, q, m, m, a1, a2);
            const double pw = rms(a1, 4000) * rms(a1, 4000) + rms(a2, 4000) * rms(a2, 4000);
            const double ref = 2.0 * rms(m, 4000) * rms(m, 4000);
            flat = flat && std::fabs(10.0 * std::log10(pw / ref)) < 0.05;
        }
        check(flat, "monoljud med LAW -3 dB: lika stark ton var den an star");
    }

    // ------------------------------------------------------------------
    // WIDTH
    // ------------------------------------------------------------------
    {
        const int n = 24000;
        std::vector<float> l = sine(500.0, n), r = sine(700.0, n), ol, orr;
        Params p; p.v[kParamWidth] = 0.0f;
        Engine e; e.prepare(kSr);
        run(e, p, l, r, ol, orr);
        double maxDiff = 0.0;
        for (int i = 4000; i < n; ++i) maxDiff = std::fmax(maxDiff, std::fabs(ol[i] - orr[i]));
        check(maxDiff < 1e-6, "WIDTH 0 %: helt mono (L = R)");

        std::vector<float> s = sine(1000.0, n, 0.25), sn(n);
        for (int i = 0; i < n; ++i) sn[i] = -s[i];
        p.v[kParamWidth] = 1.0f;     // 200 %
        Engine e2; e2.prepare(kSr);
        run(e2, p, s, sn, ol, orr);
        check(std::fabs(db(rms(ol, 4000) / rms(s, 4000)) - 6.0206) < 0.05, "WIDTH 200 %: sidosignalen dubbelt sa stark");
    }

    // ------------------------------------------------------------------
    // LOW MONO
    // ------------------------------------------------------------------
    {
        const int n = 48000;
        Params p; p.v[kParamLowMono] = lowMonoNorm(150.0);
        check(std::fabs(lowMonoHz(p.v[kParamLowMono]) - 150.0) < 0.5, "LOW MONO-ratten visar ratt frekvens");

        std::vector<float> ol, orr;
        // Sidosignal (L = -R) på 40 Hz tas bort
        std::vector<float> s = sine(40.0, n), sn(n);
        for (int i = 0; i < n; ++i) sn[i] = -s[i];
        Engine e; e.prepare(kSr);
        run(e, p, s, sn, ol, orr);
        check(db(rms(ol, 12000) / rms(s, 12000)) < -24.0, "LOW MONO 150 Hz: stereo-bas pa 40 Hz blir mono (minst -24 dB)");

        // Sidosignal på 2 kHz passerar
        s = sine(2000.0, n);
        for (int i = 0; i < n; ++i) sn[i] = -s[i];
        Engine e2; e2.prepare(kSr);
        run(e2, p, s, sn, ol, orr);
        check(std::fabs(db(rms(ol, 12000) / rms(s, 12000))) < 0.3, "LOW MONO 150 Hz: stereo over 2 kHz orord");

        // Monoljud: platt nivå (ingen bas tappas, ingen topp vid delningen)
        bool flat = true;
        const double fr[5] = { 40.0, 100.0, 150.0, 220.0, 1000.0 };
        for (int k = 0; k < 5; ++k) {
            std::vector<float> m = sine(fr[k], n);
            Engine x; x.prepare(kSr);
            run(x, p, m, m, ol, orr);
            flat = flat && std::fabs(db(rms(ol, 12000) / rms(m, 12000))) < 0.15;
        }
        check(flat, "LOW MONO: monoljud har platt niva runt delningen (40-1000 Hz)");

        // Basen stannar i mitten när man panorerar helt höger
        Params q = p; q.v[kParamPan] = 1.0f;
        std::vector<float> bass = sine(40.0, n), hi = sine(3000.0, n);
        Engine b; b.prepare(kSr);
        run(b, q, bass, bass, ol, orr);
        const bool bassCenter = std::fabs(db(rms(ol, 12000) / rms(bass, 12000))) < 0.5 &&
                                std::fabs(db(rms(orr, 12000) / rms(bass, 12000))) < 0.5;
        Engine h; h.prepare(kSr);
        run(h, q, hi, hi, ol, orr);
        const bool hiRight = db(rms(ol, 12000) / rms(hi, 12000)) < -40.0;
        check(bassCenter && hiRight, "LOW MONO + PAN hoger: basen stannar i mitten, diskanten gar till hoger");

        // Av/på klickar inte
        std::vector<float> m = sine(80.0, n);
        Engine c; c.prepare(kSr);
        Params off;
        std::vector<float> cl(n), cr(n);
        for (int pos = 0; pos < n; pos += 256) {
            const int len = pos + 256 <= n ? 256 : n - pos;
            c.setParams(pos < n / 2 ? off.v : p.v);
            c.process(&m[pos], &m[pos], &cl[pos], &cr[pos], len, false);
        }
        double jump = 0.0;
        for (int i = n / 2 - 2000; i < n / 2 + 4000; ++i) jump = std::fmax(jump, std::fabs(cl[i] - cl[i - 1]));
        const double maxSlope = 0.5 * 2.0 * kPi * 80.0 / kSr; // sinusens egen största lutning
        check(jump < maxSlope * 1.6, "LOW MONO pa/av ger inget klick");
    }

    // ------------------------------------------------------------------
    // AUTO-PAN
    // ------------------------------------------------------------------
    {
        const int n = 48000;
        std::vector<float> dc(n, 0.5f), ol, orr;
        Params p; p.v[kParamDepth] = 1.0f;  // sinus, 1/4, 120 bpm -> period 0,5 s
        Engine e; e.prepare(kSr);
        run(e, p, dc, dc, ol, orr);
        int maxAt = 0;
        for (int i = 0; i < 24000; ++i) if (orr[i] > orr[maxAt]) maxAt = i;
        check(std::abs(maxAt - 6000) < 500, "AUTO-PAN 1/4 vid 120 bpm: hogst till hoger efter 1/16 takt (0,125 s)");
        int minAt = 0;
        for (int i = 0; i < 24000; ++i) if (orr[i] < orr[minAt]) minAt = i;
        check(std::abs(minAt - 18000) < 500 && orr[minAt] < 0.01, "AUTO-PAN: helt vanster efter 3/16 takt");

        // Följer Lives takt
        Engine s; s.prepare(kSr);
        s.setParams(p.v);
        s.setTransport(true, 128.0, true, true, 10.25);
        check(std::fabs(s.phase() - 0.25) < 1e-9 && s.synced() && s.tempo() == 128.0, "AUTO-PAN foljer Lives position och tempo");
        s.setTransport(false, 0.0, false, false, 0.0);
        check(!s.synced() && s.tempo() == 128.0, "AUTO-PAN fortsatter i senaste tempot nar Live star still");

        // RATE-ratten
        bool ratesOk = true;
        for (int i = 0; i < kNumRates; ++i) ratesOk = ratesOk && rateFromNorm(choiceNorm(i, kNumRates)) == i;
        check(ratesOk && std::strcmp(kRateNames[rateFromNorm(defaultParam(kParamRate))], "1/4") == 0, "RATE: 12 notlangder, standard 1/4");

        // Fyrkant (ping-pong) hoppar inte på ett sampel
        Params q = p; q.v[kParamShape] = choiceNorm(kShapeSquare, kNumShapes);
        Engine sq; sq.prepare(kSr);
        run(sq, q, dc, dc, ol, orr);
        double jump = 0.0;
        for (int i = 1; i < n; ++i) jump = std::fmax(jump, std::fabs(orr[i] - orr[i - 1]));
        check(jump < 0.01, "AUTO-PAN SQUARE: mjuka hopp (inga klick)");

        // PARAVARIANT (v1.8:s PARADIDDLE): R L R R L R L L, varje slag en halv period, stannar på sidan
        const double want[8] = { 1, -1, 1, 1, -1, 1, -1, -1 };
        {
            bool patOk = true;
            for (int k = 0; k < 8; ++k) patOk = patOk && lfoValue(kShapeParaVariant, k * 0.5 + 0.25) == want[k];
            check(patOk && lfoValue(kShapeParaVariant, 4.25) == 1.0, "PARAVARIANT: R L R R L R L L, ett slag per halv period");
            Params q = p; q.v[kParamShape] = choiceNorm(kShapeParaVariant, kNumShapes);
            const int n2 = 8 * 12000;
            std::vector<float> dc2(n2, 0.5f), a2, b2;
            Engine pd; pd.prepare(kSr);
            run(pd, q, dc2, dc2, a2, b2);
            bool lr = true;
            for (int k = 0; k < 8; ++k) {
                const int i = k * 12000 + 6000;
                lr = lr && (want[k] > 0 ? (b2[i] > 0.6 && a2[i] < 0.01) : (a2[i] > 0.6 && b2[i] < 0.01));
            }
            check(lr, "PARAVARIANT i PAN: hoger-vanster-hoger-hoger-vanster-hoger-vanster-vanster");
        }
        // PARADIDDLE: varje slag = ren SQUARE-puls på en RATE-period (ut på sidan, tillbaka i mitten)
        {
            bool patOk = true;
            for (int k = 0; k < 8; ++k)
                patOk = patOk && lfoValue(kShapeParadiddle, k + 0.25) == want[k] && lfoValue(kShapeParadiddle, k + 0.75) == 0.0;
            check(patOk && lfoValue(kShapeParadiddle, 8.25) == 1.0, "PARADIDDLE: 8 separata slag R L R R L R L L, mitten mellan slagen");
            Params q = p; q.v[kParamShape] = choiceNorm(kShapeParadiddle, kNumShapes);
            const int per = 24000, n2 = 8 * per;   // 1/4 vid 120 bpm
            std::vector<float> dc2(n2, 0.5f), a2, b2;
            Engine pd; pd.prepare(kSr);
            run(pd, q, dc2, dc2, a2, b2);
            bool lr = true;
            for (int k = 0; k < 8; ++k) {
                const int on = k * per + per / 4, off = k * per + 3 * per / 4;
                lr = lr && (want[k] > 0 ? (b2[on] > 0.6 && a2[on] < 0.01) : (a2[on] > 0.6 && b2[on] < 0.01));
                lr = lr && std::fabs(a2[off] - b2[off]) < 0.01;   // tillbaka i mitten
            }
            check(lr, "PARADIDDLE i PAN: R-R och L-L hors som tva separata slag pa samma sida");
            // TREM: på / av för varje slag
            Params tq = q; tq.v[kParamFxMode] = choiceNorm(kFxTremolo, kNumFxModes);
            Engine tr; tr.prepare(kSr);
            run(tr, tq, dc2, dc2, a2, b2);
            bool gate = true;
            for (int k = 0; k < 8; ++k) gate = gate && a2[k * per + per / 4] > 0.49 && a2[k * per + 3 * per / 4] < 0.01;
            check(gate, "PARADIDDLE i TREM: varje slag pa, mellan slagen tyst");
            Engine sy; sy.prepare(kSr);
            sy.setParams(q.v);
            sy.setTransport(true, 120.0, true, true, 13.0);   // 13 mod 8 = 5 -> 6:e slaget (R)
            check(std::fabs(sy.cyclePhase() - 13.0) < 1e-9 && lfoValue(kShapeParadiddle, sy.cyclePhase() + 0.1) == 1.0,
                  "PARADIDDLE foljer Lives takt (8 slag)");
            check(std::fabs(shapeNormFromV16(1.0f) - choiceNorm(kShapeSquare, kNumShapes)) < 1e-6
                  && std::fabs(shapeNormFromV16(0.5f) - choiceNorm(kShapeTriangle, kNumShapes)) < 1e-6
                  && std::fabs(shapeNormFromOld(1.0f, kNumShapesV18) - choiceNorm(kShapeParaVariant, kNumShapes)) < 1e-6,
                  "aldre projekt behaller formen; v1.8:s PARADIDDLE blir PARAVARIANT");
        }

        // Loop i Live: mönstret börjar om på exakt rätt sampel (efter första varvet), och
        // om Live säger att loopen är på men spelar vidare blir det inget hack alls
        {
            Params q = p; q.v[kParamShape] = choiceNorm(kShapeParadiddle, kNumShapes); q.v[kParamRate] = choiceNorm(5, kNumRates); // 1/4
            const double bpm = 120.0, loopS = 0.0, loopE = 4.0 + 0.137;   // loop som inte slutar jämnt på ett block
            const int n2 = 48000 * 8;
            std::vector<float> dc2(n2, 0.5f), a1(n2), b1(n2), a2(n2), b2(n2);
            auto runLoop = [&](int block, bool hostLoops, bool reportCycle, std::vector<float>& L, std::vector<float>& R) {
                Engine x; x.prepare(kSr);
                double ppq = 0.0;
                for (int pos = 0; pos < n2; pos += block) {
                    const int m = pos + block <= n2 ? block : n2 - pos;
                    x.setParams(q.v);
                    x.setTransport(true, bpm, true, true, ppq, reportCycle, loopS, loopE);
                    x.process(&dc2[pos], &dc2[pos], &L[pos], &R[pos], m, false);
                    ppq += m / kSr * bpm / 60.0;
                    if (hostLoops && ppq >= loopE) ppq = loopS + (ppq - loopE);   // som Live: positionen hoppar tillbaka
                }
            };
            // 1) Live loopar: jämför med sampelexakt referens från andra varvet
            runLoop(1, true, true, a1, b1);
            runLoop(512, true, true, a2, b2);
            const int from = (int)(loopE * 60.0 / bpm * kSr) + 1200;   // efter första loopvarvet
            double d = 0.0;
            for (int i = from; i < n2; ++i) d = std::fmax(d, std::fmax(std::fabs(a1[i] - a2[i]), std::fabs(b1[i] - b2[i])));
            std::printf("      (loop i Live, fran andra varvet: storsta skillnad %.6f)\n", d);
            check(d < 0.01, "loop i Live: monstret borjar om pa ratt sampel vid varje nytt varv");
            // 2) Live säger att loopen är på men spelar vidare: exakt som utan loop
            std::vector<float> a3(n2), b3(n2), a4(n2), b4(n2);
            runLoop(512, false, false, a3, b3);
            runLoop(512, false, true, a4, b4);
            double d2 = 0.0;
            for (int i = 0; i < n2; ++i) d2 = std::fmax(d2, std::fabs(b3[i] - b4[i]));
            std::printf("      (loop rapporterad men inte spelad: storsta skillnad %.6f)\n", d2);
            check(d2 < 1e-6, "loop rapporterad men inte spelad (t.ex. Session-vyn): inget hack");
        }

        // Rudiment: Double Stroke, Paradiddle-diddle, Inverted Paradiddle, Six Stroke Roll, Flam
        {
            auto strokes = [](int shape, int count, std::string& out) {
                out.clear();
                for (int k = 0; k < count; ++k) {
                    const double v = lfoValue(shape, k + 0.3), gap = lfoValue(shape, k + 0.75);
                    if (gap != 0.0) { out += '?'; continue; }
                    if (v > 0.9) out += 'R'; else if (v > 0.0) out += 'r'; else if (v < -0.9) out += 'L'; else if (v < 0.0) out += 'l'; else out += '-';
                }
            };
            std::string a, b, c, d;
            strokes(kShapeDoubleStroke, 8, a); strokes(kShapeParaDiddleDiddle, 6, b);
            strokes(kShapeInvParadiddle, 8, c); strokes(kShapeSixStroke, 6, d);
            std::printf("      (DOUBLE STROKE %s, PARADIDDLE-DIDDLE %s, INV. PARADIDDLE %s, SIX STROKE %s)\n", a.c_str(), b.c_str(), c.c_str(), d.c_str());
            check(a == "RRLLRRLL" && b == "RLRRLL" && c == "RLLRLRRL" && d == "RllrrL",
                  "rudimenten har ratt slag (stora = betonade): RRLL, RLRRLL, RLLR LRRL, >R l l r r >L");
            // FLAM: förslag på motsatt sida strax före huvudslaget, lR rL
            const bool flam = lfoValue(kShapeFlam, 0.05) == -kFlamGraceLevel && lfoValue(kShapeFlam, 0.3) == 1.0 && lfoValue(kShapeFlam, 0.75) == 0.0
                              && lfoValue(kShapeFlam, 1.05) == kFlamGraceLevel && lfoValue(kShapeFlam, 1.3) == -1.0;
            check(flam, "FLAM: svagt forslag pa motsatt sida (12 % av perioden) fore huvudslaget, lR rL");
            // I hela motorn: FLAM 1/8 vid 120 bpm -> förslaget ca 30 ms före slaget
            Params q = p; q.v[kParamShape] = choiceNorm(kShapeFlam, kNumShapes); q.v[kParamRate] = choiceNorm(7, kNumRates);
            const int n2 = 24000;
            std::vector<float> dc2(n2, 0.5f), a2, b2;
            Engine fe; fe.prepare(kSr);
            run(fe, q, dc2, dc2, a2, b2);
            // period 12000 sampel: förslag 0-1440 (vänster), slag 1440-6000 (höger)
            check(a2[1000] > b2[1000] && b2[4000] > 0.6 && a2[4000] < 0.01 && std::fabs(a2[9000] - b2[9000]) < 0.01,
                  "FLAM i PAN: forslag till vanster (30 ms), slag till hoger, sedan mitten");
            // TREM: betonade slag fulla, obetonade svagare (SIX STROKE)
            Params tq = p; tq.v[kParamShape] = choiceNorm(kShapeSixStroke, kNumShapes); tq.v[kParamFxMode] = choiceNorm(kFxTremolo, kNumFxModes);
            const int per = 24000;
            std::vector<float> dc3(6 * per, 0.5f), a3, b3;
            Engine te; te.prepare(kSr);
            run(te, tq, dc3, dc3, a3, b3);
            check(a3[per / 4] > 0.49 && a3[per + per / 4] > 0.02 && a3[per + per / 4] < 0.3 && a3[per * 3 / 4] < 0.01,
                  "SIX STROKE i TREM: betonade slag fulla, obetonade svagare, tyst mellan slagen");
            check(std::fabs(shapeNormFromOld(1.0f, kNumShapesV19) - choiceNorm(kShapeParaVariant, kNumShapes)) < 1e-6
                  && std::fabs(shapeNormFromOld(0.75f, kNumShapesV19) - choiceNorm(kShapeParadiddle, kNumShapes)) < 1e-6,
                  "projekt fran v1.9 behaller PARADIDDLE och PARAVARIANT");
        }

        // Vågformerna
        check(std::fabs(lfoValue(kShapeSine, 0.25) - 1.0) < 1e-12 && std::fabs(lfoValue(kShapeTriangle, 0.75) + 1.0) < 1e-12
              && lfoValue(kShapeSquare, 0.1) == 1.0 && lfoValue(kShapeSquare, 0.6) == -1.0, "AUTO-PAN-vagformer SINE/TRIANGLE/SQUARE");
    }

    // ------------------------------------------------------------------
    // Utjämning, OUTPUT, MONO-knappen
    // ------------------------------------------------------------------
    {
        const int n = 24000;
        std::vector<float> dc(n, 0.5f), ol(n), orr(n);
        Params a; a.v[kParamPan] = 0.0f;
        Params b; b.v[kParamPan] = 1.0f;
        Engine e; e.prepare(kSr);
        for (int pos = 0; pos < n; pos += 256) {
            const int len = pos + 256 <= n ? 256 : n - pos;
            e.setParams(pos < n / 2 ? a.v : b.v);
            e.process(&dc[pos], &dc[pos], &ol[pos], &orr[pos], len, false);
        }
        double jump = 0.0;
        for (int i = 1; i < n; ++i) jump = std::fmax(jump, std::fmax(std::fabs(ol[i] - ol[i - 1]), std::fabs(orr[i] - orr[i - 1])));
        check(jump < 0.01, "PAN fran vanster till hoger pa en gang: mjuk overgang, inget klick");

        Params g; g.v[kParamOutput] = outputDbToNorm(6.0);
        std::vector<float> m = sine(1000.0, n, 0.25), gl, gr;
        Engine o; o.prepare(kSr);
        run(o, g, m, m, gl, gr);
        check(std::fabs(db(rms(gl, 4000) / rms(m, 4000)) - 6.0) < 0.02, "OUTPUT +6 dB");

        // MONITOR
        std::vector<float> l = sine(500.0, n), r = sine(800.0, n, 0.3);
        bool monOk = true;
        for (int mon = 1; mon < kNumMonitors; ++mon) {
            Params d; d.v[kParamMonitor] = choiceNorm(mon, kNumMonitors);
            Engine mc; mc.prepare(kSr);
            run(mc, d, l, r, gl, gr);
            for (int i = 0; i < n; ++i) {
                const double want = mon == kMonSides ? (l[i] - r[i]) * 0.5 : (l[i] + r[i]) * 0.5;
                monOk = monOk && gl[i] == gr[i] && std::fabs(gl[i] - want) < 1e-6;
            }
        }
        check(monOk, "MONITOR: MONO och MID = (L+R)/2, SIDES = (L-R)/2, samma i bada hogtalarna");
        {
            Params d; d.v[kParamMonitor] = choiceNorm(kMonSides, kNumMonitors);
            std::vector<float> m = sine(300.0, n);
            Engine mc; mc.prepare(kSr);
            run(mc, d, m, m, gl, gr);
            check(rms(gl, 0) < 1e-7, "MONITOR SIDES: monoljud blir tyst (bara skillnaden hors)");
        }
    }

    // ------------------------------------------------------------------
    // TREMOLO
    // ------------------------------------------------------------------
    {
        const int n = 48000;
        std::vector<float> dc(n, 0.5f), ol, orr;
        Params p; p.v[kParamFxMode] = choiceNorm(kFxTremolo, kNumFxModes); p.v[kParamDepth] = 1.0f;
        Engine e; e.prepare(kSr);
        run(e, p, dc, dc, ol, orr);
        int maxAt = 0, minAt = 0;
        bool same = true;
        for (int i = 0; i < 24000; ++i) {
            if (ol[i] > ol[maxAt]) maxAt = i;
            if (ol[i] < ol[minAt]) minAt = i;
            same = same && ol[i] == orr[i];
        }
        check(same, "TREMOLO: ingen panorering (L = R med PAN i mitten)");
        check(std::abs(maxAt - 6000) < 500 && std::fabs(ol[maxAt] - 0.5) < 0.01 && std::abs(minAt - 18000) < 500 && ol[minAt] < 0.01,
              "TREMOLO 1/4 vid 120 bpm, DEPTH 100 %: full niva och tyst i takt");
        p.v[kParamDepth] = 0.5f;
        Engine e2; e2.prepare(kSr);
        run(e2, p, dc, dc, ol, orr);
        double mn = 1.0;
        for (int i = 2000; i < n; ++i) mn = std::fmin(mn, ol[i]);
        check(std::fabs(mn - 0.5 * std::pow(0.5, 1.5)) < 0.01, "TREMOLO DEPTH 50 %: tydlig sankning, -9 dB i dalarna");
        p.v[kParamShape] = choiceNorm(kShapeSquare, kNumShapes);
        p.v[kParamDepth] = 1.0f;
        Engine e3; e3.prepare(kSr);
        run(e3, p, dc, dc, ol, orr);
        double jump = 0.0;
        for (int i = 1; i < n; ++i) jump = std::fmax(jump, std::fabs(ol[i] - ol[i - 1]));
        check(jump < 0.01, "TREMOLO SQUARE: mjuka hopp (inga klick)");
    }

    // ------------------------------------------------------------------
    // LESLIE
    // ------------------------------------------------------------------
    {
        const int n = 48000 * 2;
        std::vector<float> ol, orr;
        Params p; p.v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes);
        // DEPTH 0: inget händer
        {
            std::vector<float> l = sine(700.0, 9600), r = sine(300.0, 9600);
            Engine e; e.prepare(kSr);
            run(e, p, l, r, ol, orr);
            double d = 0.0;
            for (size_t i = 0; i < l.size(); ++i) d = std::fmax(d, std::fmax(std::fabs(ol[i] - l[i]), std::fabs(orr[i] - r[i])));
            check(d < 1e-6, "LESLIE med DEPTH 0: ljudet orort");
        }
        p.v[kParamDepth] = 1.0f;
        p.v[kParamLesSpeed] = choiceNorm(kLesSyncFirst + 5, kNumLesSpeeds);   // 1/16 vid 120 bpm = 8 varv/s
        std::vector<float> m = sine(2000.0, n);
        Engine e; e.prepare(kSr);
        run(e, p, m, m, ol, orr);
        check(e.leslieHornHz() > 7.0 && e.leslieDrumHz() < e.leslieHornHz() - 0.5,
              "LESLIE SPEED 1/16: hornet varvar upp pa 2 s, trumman langsammare (som originalet)");
        // Ljudet snurrar i rummet: när vänster är starkt är höger svagt (10 ms-fönster)
        {
            const int win = 480;
            std::vector<double> el, er;
            for (int i = n / 2; i + win <= n; i += win) {
                double a = 0, b = 0;
                for (int k = 0; k < win; ++k) { a += ol[i + k] * (double)ol[i + k]; b += orr[i + k] * (double)orr[i + k]; }
                el.push_back(std::sqrt(a / win)); er.push_back(std::sqrt(b / win));
            }
            double ml = 0, mr = 0;
            for (size_t i = 0; i < el.size(); ++i) { ml += el[i]; mr += er[i]; }
            ml /= el.size(); mr /= er.size();
            double cov = 0, vl = 0, vr = 0;
            for (size_t i = 0; i < el.size(); ++i) { cov += (el[i] - ml) * (er[i] - mr); vl += (el[i] - ml) * (el[i] - ml); vr += (er[i] - mr) * (er[i] - mr); }
            const double ec = cov / std::sqrt(vl * vr);
            const double swing = std::sqrt(vl / el.size()) / ml;
            std::printf("      (nivakorrelation L/R %.2f, nivasving %.0f %%)\n", ec, swing * 100.0);
            check(ec < -0.3 && swing > 0.15, "LESLIE: ljudet snurrar mellan vanster och hoger (motsatta nivaer)");
        }
        // Stereo: vänster och höger olika
        double sl = 0, sr = 0, slr = 0;
        for (int i = n / 2; i < n; ++i) { sl += ol[i] * (double)ol[i]; sr += orr[i] * (double)orr[i]; slr += ol[i] * (double)orr[i]; }
        const double corr = slr / std::sqrt(sl * sr);
        std::printf("      (korrelation L/R %.2f)\n", corr);
        check(corr < 0.6, "LESLIE: brett i stereo (vanster och hoger tydligt olika)");
        {
            const double pw = (rms(ol, n / 2) * rms(ol, n / 2) + rms(orr, n / 2) * rms(orr, n / 2)) / (2.0 * rms(m, n / 2) * rms(m, n / 2));
            const double lesDb = 10.0 * std::log10(pw);
            std::printf("      (Leslie-niva %+.1f dB mot torrt)\n", lesDb);
            check(lesDb > 0.3 && lesDb < 3.0, "LESLIE: nagot over torr niva (upplevs lika starkt som PAN)");
        }
        // Doppler: periodtiden svajar (mäts på mikrofonen utan rumsreflexer)
        {
            Leslie les; les.prepare(kSr); les.setRoom(0.0); les.setTargets(8.0, 6.8);
            std::vector<double> out(n);
            for (int i = 0; i < n; ++i) {
                double a, b;
                les.advance();
                les.process(0.5 * std::sin(2.0 * kPi * 2000.0 * i / kSr), a, b);
                out[i] = a;
            }
            double lastCross = -1.0, minP = 1e9, maxP = 0.0;
            for (int i = n / 2; i < n; ++i) {
                if (out[i - 1] < 0.0 && out[i] >= 0.0) {
                    const double t0 = (i - 1) + out[i - 1] / (out[i - 1] - out[i]);
                    if (lastCross > 0.0) { minP = std::fmin(minP, t0 - lastCross); maxP = std::fmax(maxP, t0 - lastCross); }
                    lastCross = t0;
                }
            }
            std::printf("      (periodtid max/min %.3f)\n", maxP / minP);
            check(maxP / minP > 1.02 && maxP / minP < 1.15, "LESLIE: dopplereffekt (tonhojden svajar 1-7 %)");
        }
        // Inbromsning till 1/2 (1 varv/s vid 120 bpm)
        p.v[kParamLesSpeed] = choiceNorm(kLesSyncFirst, kNumLesSpeeds);
        std::vector<float> m2 = sine(2000.0, 48000 * 4);
        std::vector<float> a, b;
        run(e, p, m2, m2, a, b);
        check(e.leslieHornHz() < 1.5 && e.leslieDrumHz() > e.leslieHornHz(), "LESLIE 1/2: hornet bromsar in pa nagra sekunder, trumman langsammare");
        // Farten följer Lives tempo
        {
            Engine t; t.prepare(kSr);
            Params q = p; q.v[kParamLesSpeed] = choiceNorm(kLesSyncFirst + 1, kNumLesSpeeds);   // 1/4
            std::vector<float> z(48000, 0.0f), a2, b2;
            for (int k = 0; k < 8; ++k) {
                for (int pos = 0; pos < 48000; pos += 480) {
                    t.setParams(q.v);
                    t.setTransport(true, 140.0, false, false, 0.0);
                    t.process(&z[pos], &z[pos], &z[pos], &z[pos], 480, false);
                }
            }
            check(std::fabs(t.leslieHornHz() - 140.0 / 60.0) < 0.02, "LESLIE SPEED 1/4 vid 140 bpm: hornet ett varv per fjardedelsnot");
        }
        {
            bool ok = true;
            for (int i = 0; i < kNumLesSpeeds; ++i) ok = ok && lesSpeedFromNorm(choiceNorm(i, kNumLesSpeeds)) == i;
            ok = ok && kLesSyncBeats[0] > kLesSyncBeats[kNumLesSync - 1] && kNumLesSpeeds == 11;
            check(ok && lesSpeedFromNorm(lesSpeedNormFromV11(1.0f)) == kLesTremolo && lesSpeedFromNorm(lesSpeedNormFromV11(0.0f)) == kLesChorale
                  && lesSpeedFromNorm(lesSpeedNormFromV12(5.0f / 7.0f)) == kLesSyncFirst + 5,
                  "SPEED-ratten: STOP, CHORALE, TREMOLO, 1/2 ... 1/32; aldre projekt raknas om ratt");
        }
        // Originalfarter, oberoende av tempot (uppmätta värden för 122/147)
        {
            auto settle = [&](int model, int speed, double tempo, double seconds, double& horn, double& drum) {
                Engine x; x.prepare(kSr);
                Params q; q.v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes); q.v[kParamDepth] = 1.0f;
                q.v[kParamLesModel] = choiceNorm(model, kNumLesModels); q.v[kParamLesSpeed] = choiceNorm(speed, kNumLesSpeeds);
                std::vector<float> z(480, 0.0f), o1(480), o2(480);
                for (int k = 0; k < (int)(seconds * 100); ++k) {
                    x.setParams(q.v);
                    x.setTransport(true, tempo, false, false, 0.0);
                    x.process(z.data(), z.data(), o1.data(), o2.data(), 480, false);
                }
                horn = x.leslieHornHz(); drum = x.leslieDrumHz();
            };
            double h, d, h2, d2;
            settle(kModel122, kLesTremolo, 90.0, 30.0, h, d);
            settle(kModel122, kLesTremolo, 150.0, 30.0, h2, d2);
            std::printf("      (122 TREMOLO: horn %.0f varv/min, trumma %.0f varv/min)\n", h * 60.0, d * 60.0);
            check(std::fabs(h * 60.0 - 400.0) < 2.0 && std::fabs(d * 60.0 - 342.0) < 2.0 && h == h2 && d == d2,
                  "MOD 122 TREMOLO: horn 400, trumma 342 varv/min, samma i alla tempon");
            settle(kModel122, kLesChorale, 120.0, 30.0, h, d);
            check(std::fabs(h * 60.0 - 48.0) < 1.0 && std::fabs(d * 60.0 - 40.0) < 1.0, "MOD 122 CHORALE: horn 48, trumma 40 varv/min");
            settle(kModelVibratone, kLesTremolo, 120.0, 30.0, h, d);
            check(std::fabs(d * 60.0 - 340.0) < 2.0, "VIBRATONE TREMOLO: rotorn 340 varv/min");
            settle(kModelVibratone, kLesChorale, 120.0, 30.0, h, d);
            check(std::fabs(d * 60.0 - 40.0) < 1.0, "VIBRATONE CHORALE: rotorn 40 varv/min");
        }
        // Uppvarvning CHORALE -> TREMOLO som på en riktig 122: hornet ca 1 s, trumman ca 5 s
        {
            Engine x; x.prepare(kSr);
            Params q; q.v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes); q.v[kParamDepth] = 1.0f;
            q.v[kParamLesSpeed] = choiceNorm(kLesChorale, kNumLesSpeeds);
            std::vector<float> z(480, 0.0f), o1(480), o2(480);
            for (int k = 0; k < 2000; ++k) { x.setParams(q.v); x.process(z.data(), z.data(), o1.data(), o2.data(), 480, false); }
            q.v[kParamLesSpeed] = choiceNorm(kLesTremolo, kNumLesSpeeds);
            const LesModelSpec& sp = kLesModels[kModel122];
            for (int k = 0; k < 100; ++k) { x.setParams(q.v); x.process(z.data(), z.data(), o1.data(), o2.data(), 480, false); }  // 1 s
            const double hornPart = (x.leslieHornHz() - sp.hornSlow) / (sp.hornFast - sp.hornSlow);
            const double drumPart = (x.leslieDrumHz() - sp.drumSlow) / (sp.drumFast - sp.drumSlow);
            for (int k = 0; k < 450; ++k) { x.setParams(q.v); x.process(z.data(), z.data(), o1.data(), o2.data(), 480, false); }  // 5,5 s
            const double drumPart55 = (x.leslieDrumHz() - sp.drumSlow) / (sp.drumFast - sp.drumSlow);
            std::printf("      (efter 1 s: horn %.0f %%, trumma %.0f %%; trumma efter 5,5 s %.0f %%)\n", hornPart * 100.0, drumPart * 100.0, drumPart55 * 100.0);
            check(hornPart > 0.9 && drumPart < 0.5 && drumPart55 > 0.9, "uppvarvning: hornet ar framme efter ca 1 s, trumman efter ca 5,5 s");
            // STOP: båda stannar helt
            q.v[kParamLesSpeed] = choiceNorm(kLesStop, kNumLesSpeeds);
            for (int k = 0; k < 1500; ++k) { x.setParams(q.v); x.process(z.data(), z.data(), o1.data(), o2.data(), 480, false); }
            check(x.leslieHornHz() == 0.0 && x.leslieDrumHz() == 0.0, "STOP: horn och trumma stannar helt");
        }
        // WIDTH styr även Leslien: 0 % = mono, 200 % = bredare
        {
            auto sideRms = [&](float w) {
                Params q; q.v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes); q.v[kParamDepth] = 1.0f;
                q.v[kParamLesSpeed] = choiceNorm(kLesTremolo, kNumLesSpeeds); q.v[kParamWidth] = widthNorm(w);
                std::vector<float> s2 = sine(1500.0, 48000 * 3), a2, b2, sd(48000 * 3);
                Engine x; x.prepare(kSr); run(x, q, s2, s2, a2, b2);
                for (size_t i = 0; i < sd.size(); ++i) sd[i] = 0.5f * (a2[i] - b2[i]);
                return rms(sd, 48000);
            };
            const double w0 = sideRms(0.0f), w1 = sideRms(1.0f), w2 = sideRms(2.0f);
            std::printf("      (Leslie sidosignal: WIDTH 0 %% %.3f, 100 %% %.3f, 200 %% %.3f)\n", w0, w1, w2);
            check(w0 < 1e-6 && w1 > 0.05 && std::fabs(w2 / w1 - 2.0) < 0.01, "WIDTH fungerar i LESLIE-laget (0 % mono, 200 % dubbelt sa brett)");
        }
        // Riktverkan: diskanten svajar mycket mer än mellanregistret (dovt bakåt)
        {
            auto swingDb = [&](double hz) {
                Leslie les; les.prepare(kSr); les.setRoom(0.0); les.setTargets(400.0 / 60.0, 342.0 / 60.0);
                const int n2 = 48000 * 2, win = 240;
                std::vector<double> out(n2);
                for (int i = 0; i < n2; ++i) {
                    double a, b;
                    les.advance();
                    les.process(0.5 * std::sin(2.0 * kPi * hz * i / kSr), a, b);
                    out[i] = a;
                }
                double mx = 0.0, mn = 1e9;
                for (int i = n2 / 2; i + win <= n2; i += win) {
                    double e = 0.0;
                    for (int k = 0; k < win; ++k) e += out[i + k] * out[i + k];
                    e = std::sqrt(e / win);
                    mx = std::fmax(mx, e); mn = std::fmin(mn, e);
                }
                return 20.0 * std::log10(mx / std::fmax(mn, 1e-9));
            };
            const double s12 = swingDb(1200.0), s8 = swingDb(8000.0);
            std::printf("      (nivasving 1,2 kHz %.1f dB, 8 kHz %.1f dB)\n", s12, s8);
            check(s8 > s12 + 6.0, "LESLIE riktverkan: diskanten (8 kHz) forsvinner mer an mellanregistret nar hornet pekar bort");
        }
        // Alla modeller: rörelse i stereo och rimlig nivå
        {
            bool ok = true;
            for (int model = 0; model < kNumLesModels; ++model) {
                Params q; q.v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes); q.v[kParamDepth] = 1.0f;
                q.v[kParamLesModel] = choiceNorm(model, kNumLesModels); q.v[kParamLesSpeed] = choiceNorm(kLesTremolo, kNumLesSpeeds);
                std::vector<float> in(48000 * 4), a2, b2;
                std::srand(7);
                double z = 0.0;
                for (size_t i = 0; i < in.size(); ++i) { z = 0.9 * z + (std::rand() / (double)RAND_MAX - 0.5); in[i] = (float)(0.2 * z); }
                Engine x; x.prepare(kSr);
                run(x, q, in, in, a2, b2);
                const int from = 48000 * 2;
                const double pw = (rms(a2, from) * rms(a2, from) + rms(b2, from) * rms(b2, from)) / (2.0 * rms(in, from) * rms(in, from));
                double slr = 0, sl = 0, sr2 = 0;
                for (size_t i = from; i < a2.size(); ++i) { slr += a2[i] * (double)b2[i]; sl += a2[i] * (double)a2[i]; sr2 += b2[i] * (double)b2[i]; }
                const double corr = slr / std::sqrt(sl * sr2);
                std::printf("      (%s: niva %+.1f dB, korrelation L/R %.2f)\n", kLesModelLongNames[model], 10.0 * std::log10(pw), corr);
                ok = ok && 10.0 * std::log10(pw) > -2.0 && 10.0 * std::log10(pw) < 3.0 && corr < 0.8;
            }
            check(ok, "MOD 122, MOD 145 och VIBRATONE: bred stereo och jamn niva");
        }
        // Byte PAN -> LESLIE klickar inte
        {
            std::vector<float> s5 = sine(500.0, 24000), cl(24000), cr(24000);
            Params off; off.v[kParamDepth] = 1.0f; off.v[kParamRate] = 0.0f; // långsam auto-pan
            Params on = off; on.v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes);
            Engine c; c.prepare(kSr);
            for (int pos = 0; pos < 24000; pos += 240) {
                c.setParams(pos < 12000 ? off.v : on.v);
                c.process(&s5[pos], &s5[pos], &cl[pos], &cr[pos], 240, false);
            }
            double jump = 0.0;
            for (int i = 11000; i < 16000; ++i) jump = std::fmax(jump, std::fabs(cl[i] - cl[i - 1]));
            const double maxSlope = 0.5 * 2.0 * kPi * 500.0 / kSr;
            check(jump < maxSlope * 2.0, "byte fran AUTO-PAN till LESLIE ger inget klick");
        }
    }

    // ------------------------------------------------------------------
    // HEAD: sfärisk huvudmodell (Woodworth ITD, Brown & Duda huvudskugga)
    // ------------------------------------------------------------------
    {
        const double a = 0.0875;   // standard: effektiv huvudradie 8,75 cm
        // Formlerna
        const double itd90 = headItd(0.5 * kPi, a);
        check(std::fabs(itd90 - (a / kSoundSpeed) * (0.5 * kPi + 1.0)) < 1e-12 && std::fabs(itd90 * 1000.0 - 0.656) < 0.005,
              "ITD rakt fran sidan = Woodworth (a/c)(pi/2 + 1) = 0,656 ms");
        bool woodOk = true;
        for (int d = 0; d <= 90; d += 5) {
            const double az = d * kPi / 180.0;
            woodOk = woodOk && std::fabs(headItd(az, a) - (a / kSoundSpeed) * (az + std::sin(az))) < 1e-12
                     && std::fabs(headItd(-az, a) + headItd(az, a)) < 1e-15;
        }
        check(woodOk, "ITD foljer Woodworth (az + sin az) for alla vinklar, symmetriskt, 0 i mitten");
        check(std::fabs(headEarGainDb(0.0, true, 8000.0, a)) < 1e-12 && std::fabs(headEarGainDb(0.0, false, 8000.0, a)) < 1e-12,
              "huvudskugga: rakt fram = 0 dB (normerat)");
        const double far8 = headEarGainDb(0.5 * kPi, false, 8000.0, a), near8 = headEarGainDb(0.5 * kPi, true, 8000.0, a);
        const double far200 = headEarGainDb(0.5 * kPi, false, 200.0, a), near200 = headEarGainDb(0.5 * kPi, true, 200.0, a);
        std::printf("      (ljud fran hoger: 8 kHz narmaste %+.1f dB, bortre %+.1f dB; 200 Hz %+.1f / %+.1f dB)\n", near8, far8, near200, far200);
        check(near8 - far8 > 15.0 && near200 - far200 < 1.0, "huvudskugga: stor nivaskillnad i diskanten, nastan ingen i basen");
        check(near8 > 2.5, "narmaste orat far mer diskant (huvudet reflekterar)");

        // Hela motorn
        const int n = 48000;
        Params p; p.v[kParamHead] = 1.0f;
        std::vector<float> l(n), r(n), ol, orr;
        std::srand(11);
        for (int i = 0; i < n; ++i) { l[i] = (float)(std::rand() / (double)RAND_MAX - 0.5); r[i] = (float)(std::rand() / (double)RAND_MAX - 0.5); }
        {
            Engine e; e.prepare(kSr);
            run(e, p, l, r, ol, orr);
            double d = 0.0;
            for (int i = 0; i < n; ++i) d = std::fmax(d, std::fmax(std::fabs(ol[i] - l[i]), std::fabs(orr[i] - r[i])));
            check(d < 1e-5, "HEAD med PAN i mitten: ljudet helt oforandrat");
        }
        // Tidsskillnad i utsignalen (korskorrelation, brusig signal), PAN helt höger
        {
            Params q = p; q.v[kParamPan] = 1.0f;
            Engine e; e.prepare(kSr);
            run(e, q, l, l, ol, orr);
            int best = 0; double bestV = -1e30;
            auto xc = [&](int lag) { double s2 = 0; for (int i = 4000; i < n - 100; ++i) s2 += (double)orr[i] * ol[i + lag]; return s2; };
            for (int lag = 0; lag < 80; ++lag) { const double v = xc(lag); if (v > bestV) { bestV = v; best = lag; } }
            const double ym = xc(best - 1), y0 = xc(best), yp = xc(best + 1);
            const double frac = best + 0.5 * (ym - yp) / (ym - 2.0 * y0 + yp);
            const double ms = frac / kSr * 1000.0;
            std::printf("      (uppmatt tidsskillnad vanster efter hoger: %.3f ms, teori %.3f ms)\n", ms, itd90 * 1000.0);
            check(std::fabs(ms - itd90 * 1000.0) < 0.06, "HEAD helt hoger: vanster ora hor ljudet ca 0,66 ms senare");
            // nivå i diskant/bas per kanal
            std::vector<float> hi = sine(8000.0, n), lo = sine(200.0, n), a1, b1;
            Engine eh; eh.prepare(kSr); run(eh, q, hi, hi, a1, b1);
            const double dHi = db(rms(b1, 4000) / rms(a1, 4000));
            Engine el; el.prepare(kSr); run(el, q, lo, lo, a1, b1);
            const double dLo = db(rms(b1, 4000) / rms(a1, 4000));
            std::printf("      (nivaskillnad hoger-vanster: 8 kHz %.1f dB, 200 Hz %.1f dB)\n", dHi, dLo);
            // + närfältet vid 1 m (avståndslagen per öra)
            const double nf = db(HeadModel::nearFieldGain(0.5 * kPi, true, 1.0, a) / HeadModel::nearFieldGain(0.5 * kPi, false, 1.0, a));
            check(std::fabs(dHi - (near8 - far8 + nf)) < 1.0 && std::fabs(dLo - (near200 - far200 + nf)) < 1.0,
                  "HEAD i utsignalen stammer med modellen (diskant, bas och narfalt vid 1 m)");
        }
        // Större huvud = större effekt (överdrift)
        {
            Params q = p; q.v[kParamHeadSize] = headSizeNorm(30.0);
            check(std::fabs(headSizeCm(q.v[kParamHeadSize]) - 30.0) < 1e-4 && headItd(0.5 * kPi, 0.15) > 1.7 * itd90,
                  "HEAD SIZE 30 cm: ca 1,7 ganger storre tidsskillnad an standard (overdriven effekt)");
        }
        // Av/på och AUTO-PAN med HEAD: inga klick
        {
            std::vector<float> s5 = sine(300.0, n), cl(n), cr(n);
            Params off; off.v[kParamPan] = 0.85f;
            Params on = off; on.v[kParamHead] = 1.0f;
            Engine c; c.prepare(kSr);
            for (int pos = 0; pos < n; pos += 480) {
                c.setParams(pos < n / 2 ? off.v : on.v);
                c.process(&s5[pos], &s5[pos], &cl[pos], &cr[pos], 480, false);
            }
            double jump = 0.0;
            for (int i = n / 2 - 2000; i < n / 2 + 4000; ++i) jump = std::fmax(jump, std::fmax(std::fabs(cl[i] - cl[i - 1]), std::fabs(cr[i] - cr[i - 1])));
            const double maxSlope = 0.5 * 2.0 * kPi * 300.0 / kSr;
            check(jump < maxSlope * 1.8, "HEAD pa/av ger inget klick");

            Params ap = on; ap.v[kParamPan] = 0.5f; ap.v[kParamDepth] = 1.0f; ap.v[kParamShape] = choiceNorm(kShapeSquare, kNumShapes);
            Engine c2; c2.prepare(kSr);
            run(c2, ap, s5, s5, cl, cr);
            jump = 0.0;
            bool fin = true;
            for (int i = 1; i < n; ++i) { jump = std::fmax(jump, std::fabs(cl[i] - cl[i - 1])); fin = fin && std::isfinite(cl[i]); }
            check(fin && jump < maxSlope * 2.5, "HEAD med AUTO-PAN (SQUARE): mjuka overgangar");
        }
        char tx[32];
        paramDisplayText(kParamHeadSize, defaultParam(kParamHeadSize), tx, sizeof tx);
        bool txOk = std::strcmp(tx, "17.5") == 0;
        paramDisplayText(kParamHead, 1.0f, tx, sizeof tx);
        check(txOk && std::strcmp(tx, "On") == 0 && defaultParam(kParamHead) == 0.0f, "texter: HeadSz 17.5 cm, Head On; HEAD av som standard");
    }

    // ------------------------------------------------------------------
    // DISTANCE: avståndslagen, luftdämpning (ISO 9613-1), närfält
    // ------------------------------------------------------------------
    {
        // ISO 9613-1 vid 20 grader C, 50 % RH (standardens tabell: 4,66 / 29,7 / 105 dB/km)
        const double a1k = isoAirAbsorptionDbPerM(1000.0) * 1000.0, a4k = isoAirAbsorptionDbPerM(4000.0) * 1000.0,
                     a8k = isoAirAbsorptionDbPerM(8000.0) * 1000.0;
        std::printf("      (ISO 9613-1: 1 kHz %.2f, 4 kHz %.1f, 8 kHz %.1f dB/km)\n", a1k, a4k, a8k);
        check(std::fabs(a1k - 4.66) < 0.05 && std::fabs(a4k - 29.7) < 0.3 && std::fabs(a8k - 105.0) < 1.0,
              "luftens dampning enligt ISO 9613-1 stammer med standardens tabell");

        // Filtret följer ISO-kurvan
        bool fitOk = true;
        double worst = 0.0;
        const double rates[3] = { 44100.0, 48000.0, 96000.0 };
        for (int ri = 0; ri < 3; ++ri) {
            AirTable tb; tb.build(rates[ri]);
            const double dists[6] = { 2.0, 5.0, 10.0, 25.0, 50.0, 100.0 };
            for (int di = 0; di < 6; ++di) {
                double k1, k2;
                tb.lookup(dists[di] - 1.0, k1, k2);
                for (double f = 250.0; f <= 16000.0 && f <= 0.45 * rates[ri]; f *= 1.1) {
                    const double target = isoAirAbsorptionDbPerM(f) * (dists[di] - 1.0);
                    if (target > 30.0) continue;
                    const double e = std::fabs(airFilterDb(k1, k2, f, rates[ri]) - target);
                    worst = std::fmax(worst, e);
                }
            }
        }
        fitOk = worst < 1.6;
        std::printf("      (luftfiltret: storsta avvikelse fran ISO-kurvan %.2f dB, 250 Hz-16 kHz, 2-100 m)\n", worst);
        check(fitOk, "luftfiltret foljer ISO 9613-1 inom 1,6 dB (44,1 / 48 / 96 kHz, 2-100 m)");

        // Hela motorn vid 10 m: 1/r = -20 dB, plus luftens dämpning i diskanten
        const int n = 48000;
        Params p; p.v[kParamDistance] = distanceNorm(10.0);
        std::vector<float> s1 = sine(1000.0, n, 0.5), s8 = sine(8000.0, n, 0.5), ol, orr;
        Engine e1; e1.prepare(kSr); run(e1, p, s1, s1, ol, orr);
        const double g1 = db(rms(ol, 8000) / rms(s1, 8000));
        Engine e8; e8.prepare(kSr); run(e8, p, s8, s8, ol, orr);
        const double g8 = db(rms(ol, 8000) / rms(s8, 8000));
        const double want8 = -20.0 - isoAirAbsorptionDbPerM(8000.0) * 9.0;
        std::printf("      (10 m: 1 kHz %.2f dB, 8 kHz %.2f dB, ISO %.2f dB)\n", g1, g8, want8);
        check(std::fabs(g1 + 20.0 - (-isoAirAbsorptionDbPerM(1000.0) * 9.0)) < 0.15 && std::fabs(g8 - want8) < 0.6,
              "DISTANCE 10 m: -20 dB (1/r) och luftdampning i diskanten enligt ISO");
        Params p100 = p; p100.v[kParamDistance] = 1.0f;
        Engine e9; e9.prepare(kSr); run(e9, p100, s8, s8, ol, orr);
        const double g8_100 = db(rms(ol, 8000) / rms(s8, 8000));
        std::printf("      (100 m: 8 kHz %.1f dB, ISO %.1f dB)\n", g8_100, -40.0 - isoAirAbsorptionDbPerM(8000.0) * 99.0);
        check(std::fabs(g8_100 - (-40.0 - isoAirAbsorptionDbPerM(8000.0) * 99.0)) < 1.6, "DISTANCE 100 m: -40 dB och ca -10 dB extra vid 8 kHz");

        // 1 m = oförändrat (standard) = klockan 9 på ratten
        check(std::fabs(distanceM(defaultParam(kParamDistance)) - 1.0) < 1e-6 && std::fabs(defaultParam(kParamDistance) - 1.0f / 6.0f) < 1e-6
              && std::fabs(distanceM(0.0f) - 0.25) < 1e-9 && std::fabs(distanceM(1.0f) - 100.0) < 1e-9
              && std::fabs(distanceM(1.0f / 12.0f) - 0.5) < 1e-6 && std::fabs(distanceM(1.0f / 6.0f + 5.0f / 12.0f) - 10.0) < 1e-5,
              "DISTANCE: 1 m klockan 9, 0,25-1 m fore, 1-100 m efter (10 m mitt pa hogra delen)");
        bool rt = true;
        for (double m = 0.25; m <= 100.0; m *= 1.37) rt = rt && std::fabs(distanceM(distanceNorm(m)) / m - 1.0) < 1e-5;
        check(rt && std::fabs(distanceM(distanceNormFromV17(0.231378f)) - 1.0) < 1e-3
              && std::fabs(distanceM(distanceNormFromV17(1.0f)) - 100.0) < 1e-3
              && std::fabs(distanceM(distanceNormFromV22(0.75f)) - 10.0) < 1e-3 && std::fabs(distanceM(distanceNormFromV22(0.25f)) - 0.5) < 1e-4,
              "DISTANCE: aldre projekt (v1.7-v2.2) behaller sitt avstand");
        // OUTPUT-kompensation: höjningen närmare än 1 m (fönstret sänker OUTPUT lika mycket)
        check(std::fabs(distanceNearBoostDb(distanceNorm(0.5)) - 6.0206) < 1e-3 && std::fabs(distanceNearBoostDb(distanceNorm(0.25)) - 12.0412) < 1e-3
              && distanceNearBoostDb(distanceNorm(1.0)) == 0.0 && distanceNearBoostDb(distanceNorm(20.0)) == 0.0,
              "narmare an 1 m: +6 dB per halverat avstand (OUTPUT sanks lika mycket i fonstret)");
        {
            // Hela motorn: 0,5 m med OUTPUT -6 dB = samma nivå som 1 m med OUTPUT 0 dB
            Params a1; Params a2; a2.v[kParamDistance] = distanceNorm(0.5); a2.v[kParamOutput] = outputDbToNorm(-distanceNearBoostDb(a2.v[kParamDistance]));
            std::vector<float> s1k = sine(1000.0, n, 0.25), x1, y1, x2, y2;
            Engine ea; ea.prepare(kSr); run(ea, a1, s1k, s1k, x1, y1);
            Engine eb; eb.prepare(kSr); run(eb, a2, s1k, s1k, x2, y2);
            check(std::fabs(db(rms(x2, 8000) / rms(x1, 8000))) < 0.05, "DISTANCE 0,5 m + OUTPUT -6 dB: samma volym som 1 m");
        }
        // Närfält med HEAD: nivåskillnad i basen växer nära huvudet
        auto lowIld = [&](double m) {
            Params q; q.v[kParamHead] = 1.0f; q.v[kParamPan] = 1.0f; q.v[kParamDistance] = distanceNorm(m);
            std::vector<float> lo = sine(200.0, n), a2, b2;
            Engine x; x.prepare(kSr); run(x, q, lo, lo, a2, b2);
            return db(rms(b2, 8000) / rms(a2, 8000));
        };
        const double i025 = lowIld(0.25), i1 = lowIld(1.0), i10 = lowIld(10.0);
        std::printf("      (200 Hz nivaskillnad mellan oronen: 0,25 m %.1f dB, 1 m %.1f dB, 10 m %.1f dB)\n", i025, i1, i10);
        check(i025 > 5.0 && i1 < 2.5 && i10 < 0.7 && i025 > i1 && i1 > i10, "narfalt: basens nivaskillnad mellan oronen vaxer nara huvudet");

        // Avståndet ändras utan klick
        {
            std::vector<float> s3 = sine(300.0, n), cl(n), cr(n);
            Params near; Params far = near; far.v[kParamDistance] = distanceNorm(20.0);
            Engine c; c.prepare(kSr);
            for (int pos = 0; pos < n; pos += 480) {
                c.setParams(pos < n / 2 ? near.v : far.v);
                c.process(&s3[pos], &s3[pos], &cl[pos], &cr[pos], 480, false);
            }
            double jump = 0.0;
            for (int i = 1; i < n; ++i) jump = std::fmax(jump, std::fabs(cl[i] - cl[i - 1]));
            check(jump < 0.5 * 2.0 * kPi * 300.0 / kSr * 1.3, "DISTANCE 1 m -> 20 m: mjuk overgang, inget klick");
        }
        char tx[32];
        paramDisplayText(kParamDistance, defaultParam(kParamDistance), tx, sizeof tx);
        bool ok = std::strcmp(tx, "1.00") == 0;
        paramDisplayText(kParamDistance, 1.0f, tx, sizeof tx);
        check(ok && std::strcmp(tx, "100") == 0, "texter: Dist 1.00 m ... 100 m");
    }

    // ------------------------------------------------------------------
    // Egna presetfiler
    // ------------------------------------------------------------------
    {
        float v[kNumParams], back[kNumParams];
        for (int i = 0; i < kNumParams; ++i) v[i] = (float)((i * 37 % 100) / 100.0);
        const std::string text = presetToText("My Wide Pan", v);
        std::string name;
        bool ok = presetFromText(text, back, &name) && name == "My Wide Pan";
        for (int i = 0; i < kNumParams; ++i) ok = ok && std::fabs(back[i] - v[i]) < 1e-6;
        check(ok, "preset sparas som text och lases tillbaka exakt (alla parametrar + namn)");

        check(!presetFromText("TrueSub preset 2\r\nName=x\r\n", back, &name), "fil som inte ar TruePan-preset avvisas");
        ok = presetFromText("TruePan preset 1\nName=Odd\nPan=0.25\nWidth=7\nJunk line\nUnknown=0.3\n", back, &name);
        check(ok && name == "Odd" && back[kParamPan] == 0.25f && back[kParamWidth] == defaultParam(kParamWidth)
              && back[kParamOutput] == defaultParam(kParamOutput), "trasiga rader och varden utanfor 0..1 ignoreras, saknade far standard");
        {
            float w[kNumParams];
            const bool okOld = presetFromText("TruePan preset 1\r\nName=Old\r\nLesSpd=1.000000\r\n", w, &name);
            check(okOld && lesSpeedFromNorm(w[kParamLesSpeed]) == kLesTremolo, "preset fran v1.1 (FAST) laddas som TREMOLO");
            const bool okV12 = presetFromText("TruePan preset 2\r\nName=V12\r\nLesSpd=0.714286\r\n", w, &name);
            check(okV12 && lesSpeedFromNorm(w[kParamLesSpeed]) == kLesSyncFirst + 5, "preset fran v1.2-1.3 (1/16) laddas som 1/16");
            const bool okV14 = presetFromText("TruePan preset 3\r\nName=Sq\r\nAPShape=1.000000\r\n", w, &name);
            check(okV14 && shapeFromNorm(w[kParamShape]) == kShapeSquare, "preset fran v1.4-1.7 med SQUARE laddas som SQUARE");
            const bool okV18 = presetFromText("TruePan preset 4\r\nName=Pd\r\nAPShape=1.000000\r\n", w, &name);
            check(okV18 && shapeFromNorm(w[kParamShape]) == kShapeParaVariant, "preset fran v1.8 med PARADIDDLE laddas som PARAVARIANT");
        }
        check(cleanPresetName("A=B\x01\xC3\xA5") == "AB__" && cleanPresetName("") == "Untitled", "presetnamn rensas fran konstiga tecken");
        const std::string u = uniquePresetName("Wide", [](const std::string& s) { return s == "Wide" || s == "Wide 2"; });
        check(u == "Wide 3", "SAVE foreslar ett namn som inte finns (Wide 3)");
    }

    // ------------------------------------------------------------------
    // Tål konstiga värden
    // ------------------------------------------------------------------
    {
        Engine e;
        e.prepare(0.0);
        check(e.sampleRate() == 44100.0, "ogiltig samplingsfrekvens -> 44,1 kHz");
        e.prepare(96000.0);
        Params p;
        p.v[kParamPan] = 1.0f; p.v[kParamWidth] = 1.0f; p.v[kParamLowMono] = 1.0f; p.v[kParamDepth] = 1.0f;
        p.v[kParamRate] = 1.0f; p.v[kParamOutput] = 1.0f; p.v[kParamShape] = 1.0f; p.v[kParamLaw] = 1.0f;
        std::vector<float> l(4096), r(4096), ol, orr;
        for (int i = 0; i < 4096; ++i) { l[i] = (i % 2) ? 1.0f : -1.0f; r[i] = (i % 3) ? 1e-30f : -1.0f; }
        run(e, p, l, r, ol, orr);
        bool finite = true;
        for (int i = 0; i < 4096; ++i) finite = finite && std::isfinite(ol[i]) && std::isfinite(orr[i]);
        check(finite, "extrema installningar: inga NaN/oandliga varden");

        e.setTransport(true, NAN, true, true, NAN);
        e.process(l.data(), nullptr, ol.data(), orr.data(), 512, false);
        finite = true;
        for (int i = 0; i < 512; ++i) finite = finite && std::isfinite(ol[i]) && std::isfinite(orr[i]);
        check(finite, "trasigt tempo/position och bara en ingang hanteras");
    }

    // ------------------------------------------------------------------
    // Texter och presets
    // ------------------------------------------------------------------
    {
        char t[32];
        paramDisplayText(kParamPan, 0.5f, t, sizeof t);      bool ok = std::strcmp(t, "C") == 0;
        paramDisplayText(kParamPan, 0.0f, t, sizeof t);      ok = ok && std::strcmp(t, "50L") == 0;
        paramDisplayText(kParamPan, 0.75f, t, sizeof t);     ok = ok && std::strcmp(t, "25R") == 0;
        paramDisplayText(kParamWidth, 0.5f, t, sizeof t);    ok = ok && std::strcmp(t, "100") == 0;
        paramDisplayText(kParamMode, 1.0f, t, sizeof t);     ok = ok && std::strcmp(t, "Stereo") == 0;
        paramDisplayText(kParamLaw, defaultParam(kParamLaw), t, sizeof t); ok = ok && std::strcmp(t, "-3.0") == 0;
        paramDisplayText(kParamLowMono, 0.0f, t, sizeof t);  ok = ok && std::strcmp(t, "Off") == 0;
        paramDisplayText(kParamOutput, defaultParam(kParamOutput), t, sizeof t); ok = ok && std::strcmp(t, "0.0") == 0;
        paramDisplayText(kParamMonitor, 1.0f, t, sizeof t);  ok = ok && std::strcmp(t, "Sides") == 0;
        paramDisplayText(kParamFxMode, 1.0f, t, sizeof t);   ok = ok && std::strcmp(t, "Leslie") == 0;
        paramDisplayText(kParamLesSpeed, 1.0f, t, sizeof t); ok = ok && std::strcmp(t, "1/32") == 0;
        paramDisplayText(kParamLesSpeed, defaultParam(kParamLesSpeed), t, sizeof t); ok = ok && std::strcmp(t, "Chorale") == 0;
        paramDisplayText(kParamLesModel, 1.0f, t, sizeof t); ok = ok && std::strcmp(t, "Vibra") == 0;
        check(ok, "texter i Live: C, 50L, 25R, 100, Stereo, -3.0, Off, 0.0, Sides, Leslie, 1/32, Chorale, Vibra");

        bool namesShort = true;
        for (int i = 0; i < kNumParams; ++i) namesShort = namesShort && std::strlen(kParamNames[i]) <= 7;
        check(namesShort, "parameternamn hogst 7 tecken");

        bool presetsOk = true;
        for (int i = 0; i < kNumFactoryPresets; ++i) {
            float v[kNumParams];
            factoryPresetValues(i, v);
            presetsOk = presetsOk && std::strlen(factoryPresetName(i)) > 0 && std::strlen(factoryPresetName(i)) < 24;
            for (int k = 0; k < kNumParams; ++k) presetsOk = presetsOk && v[k] >= 0.0f && v[k] <= 1.0f;
        }
        float v0[kNumParams];
        factoryPresetValues(0, v0);
        for (int k = 0; k < kNumParams; ++k) presetsOk = presetsOk && v0[k] == defaultParam(k);
        check(presetsOk && kNumFactoryPresets == 19, "19 fabrikspresets med giltiga varden; 'Center' = standard");
    }

    // ------------------------------------------------------------------
    // Processortid (allt påslaget, 10 s ljud vid 48 kHz)
    // ------------------------------------------------------------------
    {
        Engine e; e.prepare(kSr);
        Params p;
        p.v[kParamLowMono] = lowMonoNorm(120.0); p.v[kParamDepth] = 0.7f; p.v[kParamWidth] = 0.7f;
        p.v[kParamFxMode] = choiceNorm(kFxLeslie, kNumFxModes);
        std::vector<float> l = sine(300.0, 512), r = sine(310.0, 512), ol(512), orr(512);
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < 48000 * 10 / 512; ++b) {
            e.setParams(p.v);
            e.process(l.data(), r.data(), ol.data(), orr.data(), 512, false);
        }
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("      (10 s ljud tog %.3f s att rakna)\n", sec);
        check(sec < 1.0, "processortid: under 10 % av realtid");
    }

    std::printf("\n%s: %d fel\n", failures ? "MISSLYCKADES" : "ALLA DSP-TESTER OK", failures);
    return failures ? 1 : 0;
}
