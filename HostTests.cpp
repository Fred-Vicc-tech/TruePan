// HostTests.cpp - låtsas vara en VST2-värd (som Live) och testar pluginets
// VST2-del: att det anmäler sig som stereoeffekt, behandlar ljud, läser tempo,
// sparar/läser tillstånd och tål felaktiga data. Fönstret testas inte här.
#include "vst2.h"
#include "TruePanDSP.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

VST_EXPORT AEffect* VSTPluginMain(audioMasterCallback host);

using namespace truepan;

static int failures = 0;
static int automateCalls = 0;

static void check(bool ok, const char* name)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}

static VstTimeInfo g_time;
static bool g_timeValid = false;

static VstIntPtr hostCallback(AEffect*, VstInt32 opcode, VstInt32, VstIntPtr, void*, float)
{
    if (opcode == audioMasterVersion) return 2400;
    if (opcode == audioMasterGetTime) return g_timeValid ? (VstIntPtr)&g_time : 0;
    if (opcode == audioMasterAutomate) ++automateCalls;
    return 0;
}

static VstIntPtr oldHost(AEffect*, VstInt32, VstInt32, VstIntPtr, void*, float) { return 0; }

// Kör ett block. Returnerar största skillnad mellan ut och in.
static double processBlock(AEffect* e, std::vector<float>& l, std::vector<float>& r, std::vector<float>& ol, std::vector<float>& orr)
{
    float* ins[2] = { l.data(), r.data() };
    float* outs[2] = { ol.data(), orr.data() };
    e->processReplacing(e, ins, outs, (VstInt32)l.size());
    double d = 0.0;
    for (size_t i = 0; i < l.size(); ++i) d = std::fmax(d, std::fmax(std::fabs(ol[i] - l[i]), std::fabs(orr[i] - r[i])));
    return d;
}

int main()
{
    check(VSTPluginMain(nullptr) == nullptr && VSTPluginMain(oldHost) == nullptr, "ingen/for gammal vard: pluginet skapas inte");

    AEffect* e = VSTPluginMain(hostCallback);
    check(e != nullptr && e->magic == kEffectMagic, "plugin skapas");
    check((e->flags & effFlagsIsSynth) == 0 && (e->flags & effFlagsCanReplacing) && (e->flags & effFlagsProgramChunks),
          "anmaler sig som effekt (inte instrument), processReplacing, sparar chunk");
    check(e->numInputs == 2 && e->numOutputs == 2, "2 in, 2 ut (stereo)");
    check(e->dispatcher(e, effGetPlugCategory, 0, 0, nullptr, 0) == kPlugCategEffect, "kategori effekt");
    check(e->uniqueID == (('T' << 24) | ('r' << 16) | ('P' << 8) | 'n'), "eget unikt ID (TrPn), krockar inte med TrueSub");
    char name[64] = {};
    e->dispatcher(e, effGetEffectName, 0, 0, name, 0);
    check(std::strcmp(name, "TruePan") == 0, "namnet TruePan");

    e->dispatcher(e, effOpen, 0, 0, nullptr, 0);
    e->dispatcher(e, effSetSampleRate, 0, 0, nullptr, 48000.0f);
    e->dispatcher(e, effSetBlockSize, 0, 512, nullptr, 0);
    e->dispatcher(e, effMainsChanged, 0, 1, nullptr, 0);

    // Standardläge: ljudet passerar oförändrat
    std::vector<float> l(512), r(512), ol(512), orr(512);
    for (int i = 0; i < 512; ++i) { l[i] = (float)std::sin(i * 0.05); r[i] = (float)std::cos(i * 0.031) * 0.5f; }
    double d = 0.0;
    for (int b = 0; b < 10; ++b) d = std::fmax(d, processBlock(e, l, r, ol, orr));
    check(d < 1e-6, "standardlage: ljudet gar igenom oforandrat");

    // Samma buffert in och ut
    {
        std::vector<float> bl = l, br = r;
        float* io[2] = { bl.data(), br.data() };
        e->processReplacing(e, io, io, 512);
        double dd = 0.0;
        for (int i = 0; i < 512; ++i) dd = std::fmax(dd, std::fmax(std::fabs(bl[i] - l[i]), std::fabs(br[i] - r[i])));
        check(dd < 1e-6, "in- och utbuffert samma minne fungerar");
    }

    // Saknade ingångar
    {
        float* outs[2] = { ol.data(), orr.data() };
        e->processReplacing(e, nullptr, outs, 512);
        bool silent = true;
        for (int i = 0; i < 512; ++i) silent = silent && ol[i] == 0.0f && orr[i] == 0.0f;
        check(silent, "inga ingangar: tyst, ingen krasch");
    }

    // Parametrar
    char text[64] = {};
    const char* expectedNames[kNumParams] = { "Pan", "Width", "Mode", "Law", "LoMono", "APDepth", "APRate", "APShape", "Output",
                                              "Monitor", "APMode", "LesSpd" };
    bool namesOk = e->numParams == 12;
    for (int i = 0; i < kNumParams; ++i) {
        e->dispatcher(e, effGetParamName, i, 0, text, 0);
        namesOk = namesOk && std::strcmp(text, expectedNames[i]) == 0;
    }
    check(namesOk, "12 parametrar med stabila namn och index (de 9 forsta som i v1.0)");
    check(e->dispatcher(e, effCanBeAutomated, 0, 0, nullptr, 0) == 1 && e->dispatcher(e, effCanBeAutomated, 99, 0, nullptr, 0) == 0,
          "parametrarna kan automatiseras");

    e->dispatcher(e, effGetParamDisplay, kParamPan, 0, text, 0);
    check(std::strcmp(text, "C") == 0, "PAN standard visar C");
    e->setParameter(e, kParamPan, 0.25f);
    e->dispatcher(e, effGetParamDisplay, kParamPan, 0, text, 0);
    check(std::strcmp(text, "L50") == 0, "PAN 0,25 visar L50");
    e->setParameter(e, kParamPan, NAN);
    check(e->getParameter(e, kParamPan) == 0.25f, "NaN fran varden ignoreras");
    e->setParameter(e, kParamPan, 3.0f);
    check(e->getParameter(e, kParamPan) == 0.25f, "varde utanfor 0..1 ignoreras");
    e->dispatcher(e, effGetParamLabel, kParamOutput, 0, text, 0);
    check(std::strcmp(text, "dB") == 0, "OUTPUT har enheten dB");

    // Panorerat helt höger i Live: vänster blir tyst
    e->setParameter(e, kParamPan, 1.0f);
    e->setParameter(e, kParamMode, 0.0f); // BALANCE
    for (int b = 0; b < 20; ++b) processBlock(e, l, r, ol, orr);
    double pk = 0.0;
    for (int i = 0; i < 512; ++i) pk = std::fmax(pk, std::fabs(ol[i]));
    check(pk < 1e-6, "PAN helt hoger (BALANCE): vanster tyst");

    // Tempo och position från värden
    {
        std::memset(&g_time, 0, sizeof g_time);
        g_time.tempo = 140.0;
        g_time.ppqPos = 3.0;
        g_time.flags = kVstTempoValid | kVstPpqPosValid | kVstTransportPlaying;
        g_timeValid = true;
        AEffect* t = VSTPluginMain(hostCallback);
        t->dispatcher(t, effSetSampleRate, 0, 0, nullptr, 44100.0f);
        t->dispatcher(t, effMainsChanged, 0, 1, nullptr, 0);
        t->setParameter(t, kParamDepth, 1.0f);
        std::vector<float> one(512, 0.5f), a(512), b(512);
        // ppq 3.0 med 1/4: fasen = 0 -> mitten; ppq 3.25: fas 0,25 -> helt höger
        g_time.ppqPos = 3.25;
        for (int k = 0; k < 30; ++k) processBlock(t, one, one, a, b);
        check(a[511] < 0.02 && b[511] > 0.6, "AUTO-PAN foljer Lives position (ppq) nar Live spelar");
        t->dispatcher(t, effClose, 0, 0, nullptr, 0);
        g_timeValid = false;
    }
    check(e->dispatcher(e, effCanDo, 0, 0, (void*)"receiveVstTimeInfo", 0) == 1 &&
          e->dispatcher(e, effCanDo, 0, 0, (void*)"receiveVstMidiEvent", 0) == 0, "canDo: tidsinfo ja, MIDI nej");

    // Spara och läsa tillstånd (som när Live sparar och öppnar ett projekt)
    e->setParameter(e, kParamWidth, 0.8f);
    e->setParameter(e, kParamOutput, outputDbToNorm(-6.0));
    void* data = nullptr;
    VstIntPtr size = e->dispatcher(e, effGetChunk, 0, 0, &data, 0);
    check(size > 0 && data != nullptr, "tillstand kan sparas");
    std::vector<unsigned char> saved((unsigned char*)data, (unsigned char*)data + size);

    AEffect* e2 = VSTPluginMain(hostCallback);
    check(e2->dispatcher(e2, effSetChunk, 0, (VstIntPtr)saved.size(), saved.data(), 0) == 1, "tillstand kan lasas");
    check(e2->getParameter(e2, kParamPan) == 1.0f && e2->getParameter(e2, kParamWidth) == 0.8f &&
          e2->getParameter(e2, kParamMode) == 0.0f && e2->getParameter(e2, kParamOutput) == outputDbToNorm(-6.0),
          "alla parametrar aterstalls efter oppning");

    // Projekt sparat med v1.0 (9 parametrar): nya parametrar får standardvärden
    {
        std::vector<unsigned char> v10(48 + 9 * 4, 0);
        std::memcpy(v10.data(), "TPst", 4);
        const uint32_t ver = 1, cnt = 9; const int32_t prog = -1;
        std::memcpy(v10.data() + 4, &ver, 4); std::memcpy(v10.data() + 8, &cnt, 4); std::memcpy(v10.data() + 12, &prog, 4);
        float vals[9]; for (int i = 0; i < 9; ++i) vals[i] = defaultParam(i);
        vals[kParamPan] = 0.3f;
        std::memcpy(v10.data() + 48, vals, sizeof vals);
        AEffect* o = VSTPluginMain(hostCallback);
        o->setParameter(o, kParamMonitor, 1.0f); o->setParameter(o, kParamFxMode, 1.0f);
        o->dispatcher(o, effSetChunk, 0, (VstIntPtr)v10.size(), v10.data(), 0);
        check(o->getParameter(o, kParamPan) == 0.3f && o->getParameter(o, kParamMonitor) == 0.0f && o->getParameter(o, kParamFxMode) == 0.0f,
              "projekt fran v1.0 oppnas: MONITOR STEREO, AUTO-PAN-lage PAN");
        o->dispatcher(o, effClose, 0, 0, nullptr, 0);
    }

    // Projekt sparat med v1.1 (version 2, LESLIE FAST): blir SPEED 1/16
    {
        std::vector<unsigned char> v11(48 + 12 * 4, 0);
        std::memcpy(v11.data(), "TPst", 4);
        const uint32_t ver = 2, cnt = 12; const int32_t prog = -1;
        std::memcpy(v11.data() + 4, &ver, 4); std::memcpy(v11.data() + 8, &cnt, 4); std::memcpy(v11.data() + 12, &prog, 4);
        float vals[12]; for (int i = 0; i < 12; ++i) vals[i] = defaultParam(i);
        vals[kParamLesSpeed] = 1.0f;
        std::memcpy(v11.data() + 48, vals, sizeof vals);
        AEffect* o = VSTPluginMain(hostCallback);
        o->dispatcher(o, effSetChunk, 0, (VstIntPtr)v11.size(), v11.data(), 0);
        o->dispatcher(o, effGetParamDisplay, kParamLesSpeed, 0, text, 0);
        check(std::strcmp(text, "1/16") == 0, "projekt fran v1.1 med LESLIE FAST oppnas som SPEED 1/16");
        o->dispatcher(o, effClose, 0, 0, nullptr, 0);
    }

    // Kortare chunk (framtida/äldre version med färre parametrar): resten får standardvärden
    {
        std::vector<unsigned char> shortChunk(48 + 2 * 4, 0);
        std::memcpy(shortChunk.data(), "TPst", 4);
        const uint32_t ver = 1, cnt = 2; const int32_t prog = -1;
        std::memcpy(shortChunk.data() + 4, &ver, 4); std::memcpy(shortChunk.data() + 8, &cnt, 4); std::memcpy(shortChunk.data() + 12, &prog, 4);
        std::memcpy(shortChunk.data() + 16, "Mine", 4);
        const float v[2] = { 0.1f, 0.9f };
        std::memcpy(shortChunk.data() + 48, v, sizeof v);
        e2->dispatcher(e2, effSetChunk, 0, (VstIntPtr)shortChunk.size(), shortChunk.data(), 0);
        e2->dispatcher(e2, effGetProgramName, 0, 0, text, 0);
        check(e2->getParameter(e2, kParamPan) == 0.1f && e2->getParameter(e2, kParamWidth) == 0.9f
              && e2->getParameter(e2, kParamOutput) == defaultParam(kParamOutput) && std::strcmp(text, "Mine") == 0,
              "chunk med farre parametrar: resten far standardvarden, namnet behalls");
    }

    unsigned char junk[7] = { 1, 2, 3, 4, 5, 6, 7 };
    check(e2->dispatcher(e2, effSetChunk, 0, sizeof junk, junk, 0) == 0, "trasig data avvisas utan krasch");
    {
        std::vector<unsigned char> bad(52, 0);
        std::memcpy(bad.data(), "TPst", 4);
        const uint32_t ver = 1, cnt = 200;
        std::memcpy(bad.data() + 4, &ver, 4); std::memcpy(bad.data() + 8, &cnt, 4);
        std::memset(bad.data() + 16, 0xFF, 32);     // namn utan nolla, konstiga tecken
        const float nanv = NAN;
        std::memcpy(bad.data() + 48, &nanv, 4);
        check(e2->dispatcher(e2, effSetChunk, 0, (VstIntPtr)bad.size(), bad.data(), 0) == 1 &&
              std::isfinite(e2->getParameter(e2, 0)), "felaktigt antal parametrar, NaN och trasigt namn hanteras");
    }

    // Fabrikspresets som program
    {
        AEffect* q = VSTPluginMain(hostCallback);
        check(q->numPrograms == kNumFactoryPresets, "fabrikspresets syns som program i Live");
        char nm[64] = {};
        bool ok = true;
        for (int i = 0; i < q->numPrograms; ++i)
            ok = ok && q->dispatcher(q, effGetProgramNameIndexed, i, 0, nm, 0) == 1 && std::strcmp(nm, factoryPresetName(i)) == 0;
        check(ok, "alla fabrikspresets har namn");
        q->dispatcher(q, effSetProgram, 0, 2, nullptr, 0); // Hard Right
        q->dispatcher(q, effGetProgramName, 0, 0, nm, 0);
        check(std::strcmp(nm, "Hard Right") == 0 && q->getParameter(q, kParamPan) == 1.0f &&
              q->dispatcher(q, effGetProgram, 0, 0, nullptr, 0) == 2, "program 'Hard Right' laddas");
        q->dispatcher(q, effSetProgram, 0, 99, nullptr, 0);
        check(q->dispatcher(q, effGetProgram, 0, 0, nullptr, 0) == 2, "ogiltigt programnummer ignoreras");
        q->dispatcher(q, effClose, 0, 0, nullptr, 0);
    }

    e->dispatcher(e, effClose, 0, 0, nullptr, 0);
    e2->dispatcher(e2, effClose, 0, 0, nullptr, 0);

    std::printf("\n%s: %d fel\n", failures ? "MISSLYCKADES" : "ALLA VARDTESTER OK", failures);
    return failures ? 1 : 0;
}
