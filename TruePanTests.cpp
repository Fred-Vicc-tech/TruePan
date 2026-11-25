// TruePanTests.cpp - automatiska tester av ljudmotorn (TruePanDSP.h).
// Körs i varje bygge på GitHub. Skriver PASS/FAIL per test.
#include "TruePanDSP.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

        std::vector<float> l = sine(500.0, n), r(n, 0.0f);
        Engine mc; mc.prepare(kSr);
        mc.setMonoCheck(true);
        Params d;
        run(mc, d, l, r, gl, gr);
        bool eq = true;
        for (int i = 0; i < n; ++i) eq = eq && gl[i] == gr[i];
        check(eq, "MONO-knappen: L och R blir lika (monokontroll)");
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
        check(ok, "texter i Live: C, L100, R50, 100, Stereo, -3.0, Off, 0.0");

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
        check(presetsOk, "10 fabrikspresets med giltiga varden; 'Center' = standard");
    }

    // ------------------------------------------------------------------
    // Processortid (allt påslaget, 10 s ljud vid 48 kHz)
    // ------------------------------------------------------------------
    {
        Engine e; e.prepare(kSr);
        Params p;
        p.v[kParamLowMono] = lowMonoNorm(120.0); p.v[kParamDepth] = 0.7f; p.v[kParamWidth] = 0.7f;
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
