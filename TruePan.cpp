// TruePan.cpp - VST2-versionen: kopplar kärnan (TruePanCore.h) och ljudmotorn
// (TruePanDSP.h) till värdprogrammet via VST2-gränssnittet. Fönstret finns i
// TruePanEditor.cpp och är detsamma som i VST3-versionen.
//
// Trådar:
//   - Ljudtråden anropar processReplacing. Där sker ingen allokering, inga
//     lås och inget ritande.
//   - Gränssnittet (fönstret) läser bara atomiska värden som ljudtråden
//     skriver, och skriver parametrar via atomiska värden.
#include "vst2.h"
#include "TruePanCore.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <chrono>

#if defined(_WIN32)
  #define TRUEPAN_HAS_EDITOR 1
#else
  #define TRUEPAN_HAS_EDITOR 0
#endif

using namespace truepan;

namespace {

const VstInt32 kUniqueId = ('T' << 24) | ('r' << 16) | ('P' << 8) | 'n';
const VstInt32 kVersion = 2630;           // TruePan 2.6.3

struct Plugin : Core
{
    AEffect effect;
    audioMasterCallback host = nullptr;
    ERect rect;
    // När projektet öppnas läser Live först in tillståndet (chunk) och väljer sedan
    // samma "program" igen. Det får inte skriva över inställningarna med fabrikspresetet.
    std::chrono::steady_clock::time_point chunkLoadedAt{};
    bool chunkLoaded = false;

    void hostAutomate(int index, float value) override
    {
        if (host) host(&effect, audioMasterAutomate, index, 0, nullptr, value);
    }
    void hostBeginEdit(int index) override { if (host) host(&effect, audioMasterBeginEdit, index, 0, nullptr, 0.0f); }
    void hostEndEdit(int index) override { if (host) host(&effect, audioMasterEndEdit, index, 0, nullptr, 0.0f); }
    void hostUpdateDisplay() override { if (host) host(&effect, audioMasterUpdateDisplay, 0, 0, nullptr, 0.0f); }
};

Plugin* fromEffect(AEffect* e) { return e ? static_cast<Plugin*>(e->object) : nullptr; }

// ---------------------------------------------------------------------------
// Ljudbehandling
// ---------------------------------------------------------------------------

void processBlock(Plugin* p, float** inputs, float** outputs, VstInt32 n, bool accumulate)
{
    if (!p || !outputs || !outputs[0] || n <= 0) return;

    // Tempo och position från värden (AUTO-PAN följer takten när den spelar)
    TransportInfo t;
    if (p->host) {
        const VstIntPtr r = p->host(&p->effect, audioMasterGetTime, 0, kVstTempoValid | kVstPpqPosValid | kVstCyclePosValid, nullptr, 0.0f);
        const VstTimeInfo* ti = reinterpret_cast<const VstTimeInfo*>(r);
        if (ti) {
            t.tempoValid = (ti->flags & kVstTempoValid) != 0;
            t.tempo = ti->tempo;
            t.playing = (ti->flags & kVstTransportPlaying) != 0;
            t.ppqValid = (ti->flags & kVstPpqPosValid) != 0;
            t.ppq = ti->ppqPos;
            t.cycleOn = (ti->flags & kVstTransportCycleActive) && (ti->flags & kVstCyclePosValid);
            t.cycleStart = ti->cycleStartPos;
            t.cycleEnd = ti->cycleEndPos;
        }
    }
    const float* inL = inputs ? inputs[0] : nullptr;
    const float* inR = inputs ? inputs[1] : nullptr;
    p->processAudio(inL, inR, outputs[0], outputs[1], n, accumulate, t);
}

void VSTCALLBACK_processReplacing(AEffect* e, float** in, float** out, VstInt32 n)
{
    processBlock(fromEffect(e), in, out, n, false);
}

void VSTCALLBACK_process(AEffect* e, float** in, float** out, VstInt32 n)
{
    processBlock(fromEffect(e), in, out, n, true);
}

// ---------------------------------------------------------------------------
// Parametrar
// ---------------------------------------------------------------------------

void setParameter(AEffect* e, VstInt32 index, float value)
{
    if (Plugin* p = fromEffect(e)) p->setParam(index, value);
}

float getParameter(AEffect* e, VstInt32 index)
{
    Plugin* p = fromEffect(e);
    if (!p || index < 0 || index >= kNumParams) return 0.0f;
    return p->params[index].load();
}

// ---------------------------------------------------------------------------
// Dispatcher: värdens kommandon till pluginet
// ---------------------------------------------------------------------------

VstIntPtr dispatcher(AEffect* e, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
{
    Plugin* p = fromEffect(e);
    if (!p) return 0;

    switch (opcode) {
    case effOpen: return 0;

    case effClose:
#if TRUEPAN_HAS_EDITOR
        editorClose(p);
#endif
        delete p;
        return 0;

    case effSetSampleRate:
        p->engine.prepare(opt);
        return 0;

    case effSetBlockSize: return 0;

    case effMainsChanged:
        if (value) p->engine.reset();
        return 0;

    case effGetParamName:
        if (ptr && index >= 0 && index < kNumParams) copyString((char*)ptr, kParamNames[index], 8);
        return 0;
    case effGetParamLabel:
        if (ptr && index >= 0 && index < kNumParams) copyString((char*)ptr, kParamLabels[index], 8);
        return 0;
    case effGetParamDisplay:
        if (ptr && index >= 0 && index < kNumParams) {
            char buf[32];
            paramDisplayText(index, p->params[index].load(), buf, sizeof buf);
            copyString((char*)ptr, buf, 8);
        }
        return 0;
    case effCanBeAutomated:
        return (index >= 0 && index < kNumParams) ? 1 : 0;

    // Fabrikspresets visas som "program" i Lives presetlista.
    case effGetProgram: return p->program < 0 ? 0 : p->program;
    case effSetProgram: {
        // Samma program som redan är valt: gör ingenting (Live gör så när ett projekt
        // öppnas, direkt efter att tillståndet lästs in). Strax efter inläsning
        // ignoreras programbyten helt av samma skäl.
        const int current = p->program < 0 ? 0 : p->program;
        if ((int)value == current) return 0;
        if (p->chunkLoaded && std::chrono::steady_clock::now() - p->chunkLoadedAt < std::chrono::milliseconds(1500)) return 0;
        p->loadFactoryPreset((int)value, false);
        return 0;
    }
    case effGetProgramName:
        if (ptr) copyString((char*)ptr, p->presetName, 24);
        return 0;
    case effSetProgramName:
        if (ptr) copyString(p->presetName, cleanPresetName((const char*)ptr).c_str(), sizeof p->presetName);
        return 0;
    case effGetProgramNameIndexed:
        if (ptr && index >= 0 && index < kNumFactoryPresets) { copyString((char*)ptr, factoryPresetName(index), 24); return 1; }
        return 0;

    case effGetChunk: {
        if (!ptr) return 0;
        *static_cast<void**>(ptr) = (void*)&p->saveState();
        return (VstIntPtr)sizeof(StateChunk);
    }
    case effSetChunk:
        if (!ptr || value <= 0) return 0;
        if (!p->loadState(ptr, (size_t)value)) return 0;
        p->chunkLoaded = true;
        p->chunkLoadedAt = std::chrono::steady_clock::now();
        return 1;

    case effGetPlugCategory: return kPlugCategEffect;
    case effGetEffectName:
        if (ptr) copyString((char*)ptr, "TruePan", 32);
        return 1;
    case effGetProductString:
        if (ptr) copyString((char*)ptr, "TruePan Psychoacoustic Stereo Panner", 64);
        return 1;
    case effGetVendorString:
        if (ptr) copyString((char*)ptr, "TruePan", 64);
        return 1;
    case effGetVendorVersion: return kVersion;
    case effGetVstVersion: return 2400;
    case effGetTailSize: return 0;

    case effCanDo: {
        const char* s = static_cast<const char*>(ptr);
        if (!s) return 0;
        if (!std::strcmp(s, "receiveVstTimeInfo")) return 1;
        if (!std::strcmp(s, "receiveVstKeys")) return 1;
        return 0;
    }

#if TRUEPAN_HAS_EDITOR
    case effEditGetRect:
        if (ptr) *static_cast<ERect**>(ptr) = &p->rect;
        return 1;
    case effEditOpen:
        editorOpen(p, ptr);
        return 1;
    case effEditClose:
        editorClose(p);
        return 1;
    case effEditIdle: return 0;
    case effEditKeyDown:   // value = virtuell tangent: VKEY_LEFT 11, VKEY_RIGHT 13
        if (value == 11 || value == 13) return editorKey(p, value == 11 ? -1 : 1) ? 1 : 0;
        return 0;
#endif

    default: return 0;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Ingång: värden anropar denna funktion för att skapa pluginet
// ---------------------------------------------------------------------------
VST_EXPORT AEffect* VSTPluginMain(audioMasterCallback host)
{
    if (!host) return nullptr;
    if (host(nullptr, audioMasterVersion, 0, 0, nullptr, 0.0f) == 0) return nullptr;

    Plugin* p = new (std::nothrow) Plugin();
    if (!p) return nullptr;
    p->host = host;

    AEffect& e = p->effect;
    std::memset(&e, 0, sizeof(AEffect));
    e.magic = kEffectMagic;
    e.dispatcher = dispatcher;
    e.process = VSTCALLBACK_process;
    e.processReplacing = VSTCALLBACK_processReplacing;
    e.setParameter = setParameter;
    e.getParameter = getParameter;
    e.numPrograms = kNumFactoryPresets;
    e.numParams = kNumParams;
    e.numInputs = 2;
    e.numOutputs = 2;
    e.flags = effFlagsCanReplacing | effFlagsProgramChunks
#if TRUEPAN_HAS_EDITOR
            | effFlagsHasEditor
#endif
            ;
    e.object = p;
    e.uniqueID = kUniqueId;
    e.version = kVersion;

    p->rect.top = 0; p->rect.left = 0;
    p->rect.bottom = (short)kEditorHeight; p->rect.right = (short)kEditorWidth;

    return &e;   // motorn är redan förberedd för 44,1 kHz (Live skickar rätt frekvens före start)
}

