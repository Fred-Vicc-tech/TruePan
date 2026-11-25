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
        check(std::fabs(mn - 0.125) < 0.01, "TREMOLO DEPTH 50 %: tydlig sankning, -12 dB i dalarna");
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
        p.v[kParamLesSpeed] = choiceNorm(5, kNumLesSpeeds);   // 1/16 vid 120 bpm = 8 varv/s
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
        check(std::fabs(db(rms(ol, n / 2) / rms(m, n / 2))) < 3.0, "LESLIE: ungefar samma niva som utan effekt (+-3 dB)");
        // Doppler: periodtiden svajar (mäts på mikrofonen utan rumsreflexer)
        {
            Leslie les; les.prepare(kSr); les.setRoom(0.0); les.setHornHz(8.0);
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
        p.v[kParamLesSpeed] = choiceNorm(0, kNumLesSpeeds);
        std::vector<float> m2 = sine(2000.0, 48000 * 4);
        std::vector<float> a, b;
        run(e, p, m2, m2, a, b);
        check(e.leslieHornHz() < 1.5 && e.leslieDrumHz() > e.leslieHornHz(), "LESLIE 1/2: hornet bromsar in pa nagra sekunder, trumman langsammare");
        // Farten följer Lives tempo
        {
            Engine t; t.prepare(kSr);
            Params q = p; q.v[kParamLesSpeed] = choiceNorm(1, kNumLesSpeeds);   // 1/4
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
            ok = ok && kLesSpeedBeats[0] > kLesSpeedBeats[kNumLesSpeeds - 1];
            check(ok && lesSpeedFromNorm(lesSpeedNormFromV11(1.0f)) == 5 && lesSpeedFromNorm(lesSpeedNormFromV11(0.0f)) == 0,
                  "SPEED-ratten: 8 steg 1/2 ... 1/32; v1.1 FAST -> 1/16, SLOW -> 1/2");
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
            check(okOld && lesSpeedFromNorm(w[kParamLesSpeed]) == 5, "preset fran v1.1 (FAST) laddas som SPEED 1/16");
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
        paramDisplayText(kParamPan, 0.0f, t, sizeof t);      ok = ok && std::strcmp(t, "L100") == 0;
        paramDisplayText(kParamPan, 0.75f, t, sizeof t);     ok = ok && std::strcmp(t, "R50") == 0;
        paramDisplayText(kParamWidth, 0.5f, t, sizeof t);    ok = ok && std::strcmp(t, "100") == 0;
        paramDisplayText(kParamMode, 1.0f, t, sizeof t);     ok = ok && std::strcmp(t, "Stereo") == 0;
        paramDisplayText(kParamLaw, defaultParam(kParamLaw), t, sizeof t); ok = ok && std::strcmp(t, "-3.0") == 0;
        paramDisplayText(kParamLowMono, 0.0f, t, sizeof t);  ok = ok && std::strcmp(t, "Off") == 0;
        paramDisplayText(kParamOutput, defaultParam(kParamOutput), t, sizeof t); ok = ok && std::strcmp(t, "0.0") == 0;
        paramDisplayText(kParamMonitor, 1.0f, t, sizeof t);  ok = ok && std::strcmp(t, "Sides") == 0;
        paramDisplayText(kParamFxMode, 1.0f, t, sizeof t);   ok = ok && std::strcmp(t, "Leslie") == 0;
        paramDisplayText(kParamLesSpeed, 1.0f, t, sizeof t); ok = ok && std::strcmp(t, "1/32") == 0;
        check(ok, "texter i Live: C, L100, R50, 100, Stereo, -3.0, Off, 0.0, Sides, Leslie, 1/32");

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
        check(presetsOk && kNumFactoryPresets == 13, "13 fabrikspresets med giltiga varden; 'Center' = standard");
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
