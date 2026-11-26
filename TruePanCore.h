// TruePanCore.h - det som är gemensamt för VST2- och VST3-versionen:
// parametrar (atomiska, delas mellan ljudtråd och fönster), ljudbehandling per
// block, sparat tillstånd (chunk) och fabrikspresets. Värdprogrammet (VST2
// eller VST3) kopplas in via de virtuella host*-funktionerna.
#pragma once

#include "TruePanDSP.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
  #include <xmmintrin.h>
  #define TRUEPAN_HAS_SSE 1
#else
  #define TRUEPAN_HAS_SSE 0
#endif

namespace truepan {

const int kEditorWidth = 900, kEditorHeight = 701;
const uint32_t kStateVersion = 11;   // se StateChunk

inline void copyString(char* dst, const char* src, size_t maxLen)
{
    size_t i = 0;
    for (; i + 1 < maxLen && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
}

// Sparat tillstånd ("chunk") som värdprogrammet lagrar i projektet. Samma
// format i VST2 och VST3. Har ett versionsnummer så att framtida versioner kan
// läsa gamla projekt.
#pragma pack(push, 1)
struct StateChunk
{
    char magic[4];          // "TPst"
    uint32_t version;       // 11 (v2.3: DISTANCE med 1 m klockan 9; 10 = v2.2 med 1 m i mitten; 9 = v2.0-2.1.1 rudiment, 10 former; 8 = v1.9 med 5 former; 7 = v1.8 med 4 former; 6 = v1.7 DISTANCE; 5 = v1.6; 4 = v1.4-1.5; 3 = v1.2-1.3; 2 = v1.1; 1 = v1.0)
    uint32_t numParams;
    int32_t program;        // vald fabrikspreset (-1 = egen)
    char name[32];          // presetnamn
    float values[kNumParams];
};
#pragma pack(pop)
const size_t kChunkHeader = 48;

// Tempo och position från värdprogrammet
struct TransportInfo
{
    bool tempoValid = false, playing = false, ppqValid = false, cycleOn = false;
    double tempo = 120.0, ppq = 0.0, cycleStart = 0.0, cycleEnd = 0.0;
};

struct Core
{
    Engine engine;
    std::atomic<float> params[kNumParams];

    // För gränssnittet
    std::atomic<float> displayPos{ 0.0f };     // position just nu (PAN + AUTO-PAN)
    std::atomic<float> peakL{ 0.0f };          // högsta nivå sedan fönstret läste
    std::atomic<float> peakR{ 0.0f };
    std::atomic<int> synced{ 0 };              // AUTO-PAN följer takten

    StateChunk chunk;
    int program = 0;
    char presetName[32] = "Center";
    void* editor = nullptr;                    // fönstret (TruePanEditor.cpp), om öppet

    Core() { for (int i = 0; i < kNumParams; ++i) params[i].store(defaultParam(i)); }
    virtual ~Core() {}

    // --- Kopplingar till värdprogrammet (VST2 eller VST3) ---
    virtual void hostAutomate(int /*index*/, float /*value*/) {}   // parameter ändrad i fönstret
    virtual void hostBeginEdit(int /*index*/) {}
    virtual void hostEndEdit(int /*index*/) {}
    virtual void hostUpdateDisplay() {}                            // presetnamn m.m. ändrat

    void setParam(int index, float value)
    {
        if (index < 0 || index >= kNumParams) return;
        if (!(value >= 0.0f && value <= 1.0f)) return; // NaN / utanför område
        params[index].store(value);
    }

    // Används när gränssnittet ändrar en parameter: meddelar även värden så att
    // ändringen kan spelas in som automation.
    void setParamFromEditor(int index, float value)
    {
        setParam(index, value);
        if (index >= 0 && index < kNumParams) hostAutomate(index, params[index].load());
    }
    void beginEdit(int index) { hostBeginEdit(index); }
    void endEdit(int index) { hostEndEdit(index); }

    void applyValues(const float* v, bool fromEditor)
    {
        for (int i = 0; i < kNumParams; ++i) {
            if (fromEditor) { beginEdit(i); setParamFromEditor(i, v[i]); endEdit(i); }
            else setParam(i, v[i]);
        }
    }

    void loadFactoryPreset(int index, bool fromEditor)
    {
        if (index < 0 || index >= kNumFactoryPresets) return;
        float v[kNumParams];
        factoryPresetValues(index, v);
        applyValues(v, fromEditor);
        program = index;
        copyString(presetName, factoryPresetName(index), sizeof presetName);
    }

    // --- Ljudbehandling (ljudtråden) ---
    static void storeMax(std::atomic<float>& a, double v)
    {
        if ((float)v > a.load()) a.store((float)v);
    }

    void processAudio(const float* inL, const float* inR, float* outL, float* outR, int n, bool accumulate,
                      const TransportInfo& t)
    {
        if (!outL || n <= 0) return;
#if TRUEPAN_HAS_SSE
        // Stäng av denormala tal (kan annars göra processorn mycket långsam).
        const unsigned oldCsr = _mm_getcsr();
        _mm_setcsr(oldCsr | 0x8040); // FTZ + DAZ
#endif
        float v[kNumParams];
        for (int i = 0; i < kNumParams; ++i) v[i] = params[i].load();
        engine.setParams(v);
        engine.setTransport(t.tempoValid, t.tempo, t.playing, t.ppqValid, t.ppq, t.cycleOn, t.cycleStart, t.cycleEnd);
        engine.process(inL, inR, outL, outR, n, accumulate);

        displayPos.store((float)engine.displayPos());
        storeMax(peakL, engine.peakL());
        storeMax(peakR, engine.peakR());
        synced.store(engine.synced() ? 1 : 0);
#if TRUEPAN_HAS_SSE
        _mm_setcsr(oldCsr);
#endif
    }

    // --- Sparat tillstånd ---
    const StateChunk& saveState()
    {
        std::memset(&chunk, 0, sizeof chunk);
        std::memcpy(chunk.magic, "TPst", 4);
        chunk.version = kStateVersion;
        chunk.numParams = kNumParams;
        chunk.program = program;
        copyString(chunk.name, presetName, sizeof chunk.name);
        for (int i = 0; i < kNumParams; ++i) chunk.values[i] = params[i].load();
        return chunk;
    }

    bool loadState(const void* ptr, size_t size)
    {
        if (!ptr || size < kChunkHeader) return false;
        const unsigned char* data = static_cast<const unsigned char*>(ptr);
        if (std::memcmp(data, "TPst", 4) != 0) return false;
        uint32_t version = 0, count = 0;
        std::memcpy(&version, data + 4, 4);
        std::memcpy(&count, data + 8, 4);
        if (version < 1) return false;
        int32_t prog = 0;
        std::memcpy(&prog, data + 12, 4);
        program = (prog >= -1 && prog < kNumFactoryPresets) ? prog : -1;
        char nm[33];
        std::memcpy(nm, data + 16, 32);
        nm[32] = 0;
        copyString(presetName, cleanPresetName(nm).c_str(), sizeof presetName);
        // Börja från standardvärden: parametrar som saknas får standardvärden.
        for (int i = 0; i < kNumParams; ++i) setParam(i, defaultParam(i));
        const size_t available = (size - kChunkHeader) / sizeof(float);
        if (count > available) count = (uint32_t)available;
        for (uint32_t i = 0; i < count && i < (uint32_t)kNumParams; ++i) {
            float v;
            std::memcpy(&v, data + kChunkHeader + i * sizeof(float), sizeof(float));
            if (version == 2 && (int)i == kParamLesSpeed && v >= 0.0f && v <= 1.0f) v = lesSpeedNormFromV11(v);
            if (version == 3 && (int)i == kParamLesSpeed && v >= 0.0f && v <= 1.0f) v = lesSpeedNormFromV12(v);
            if (version < 10 && (int)i == kParamDistance && v >= 0.0f && v <= 1.0f) v = distanceNormFromV17(v); // ny skala
            else if (version == 10 && (int)i == kParamDistance && v >= 0.0f && v <= 1.0f) v = distanceNormFromV22(v); // 1 m i mitten -> klockan 9
            if (version < 9 && (int)i == kParamShape && v >= 0.0f && v <= 1.0f)               // 3 / 4 / 5 -> 10 former
                v = shapeNormFromOld(v, version < 7 ? kNumShapesV16 : (version == 7 ? kNumShapesV18 : kNumShapesV19));
            setParam((int)i, v); // ogiltiga värden ignoreras
        }
        return true;
    }
};

// Fönstret (Windows). Öppnas i ett föräldrafönster från värdprogrammet.
void editorOpen(Core* core, void* parentWindow);
void editorClose(Core* core);

} // namespace truepan
