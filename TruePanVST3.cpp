// TruePanVST3.cpp - VST3-versionen av TruePan. Samma ljudmotor (TruePanDSP.h),
// samma kärna (TruePanCore.h) och samma fönster (TruePanEditor.cpp) som
// VST2-versionen; bara kopplingen till värdprogrammet skiljer sig.
//
// Byggs med Steinbergs VST 3 SDK (MIT-licens, se LICENSE-VST3-SDK.txt).
// Projekt sparas i samma format som VST2-versionen.
#include "TruePanCore.h"

#include "pluginterfaces/base/keycodes.h"
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/common/pluginview.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vsttypes.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

// Egna, slumpade id-nummer för TruePan VST3 (får aldrig ändras)
static const FUID kTruePanUid(0x5E6A1C42, 0x9B3D4F17, 0xA2C85E01, 0x7F3B9D64);

// Långa parameternamn (VST3 visar fler tecken än VST2)
static const char* const kLongNames[truepan::kNumParams] = {
    "Pan", "Width", "Mode", "Pan Law", "Low Mono", "Auto-Pan Depth", "Auto-Pan Rate", "Auto-Pan Shape",
    "Output", "Monitor", "Auto-Pan Mode", "Leslie Speed", "Leslie Model", "Head", "Head Size", "Distance"
};

// Antal steg för valparametrar (0 = steglös)
int32 stepCountFor(int index)
{
    using namespace truepan;
    switch (index) {
    case kParamMode:     return 1;
    case kParamLaw:      return kNumLaws - 1;
    case kParamRate:     return kNumRates - 1;
    case kParamShape:    return kNumShapes - 1;
    case kParamMonitor:  return kNumMonitors - 1;
    case kParamFxMode:   return kNumFxModes - 1;
    case kParamLesSpeed: return kNumLesSpeeds - 1;
    case kParamLesModel: return kNumLesModels - 1;
    case kParamHead:     return 1;
    default:             return 0;
    }
}

void toTChar(const char* in, TChar* out, int maxLen)
{
    int i = 0;
    for (; in && in[i] && i < maxLen - 1; ++i) out[i] = (TChar)(unsigned char)in[i];
    out[i] = 0;
}

void fromTChar(const TChar* in, char* out, int maxLen)
{
    int i = 0;
    for (; in && in[i] && i < maxLen - 1; ++i) out[i] = (in[i] < 128) ? (char)in[i] : '?';
    out[i] = 0;
}

// Parameter som visar sitt värde med samma text som VST2-versionen
class TPParameter : public Parameter
{
public:
    TPParameter(int index)
    {
        toTChar(kLongNames[index], info.title, 128);
        toTChar(truepan::kParamNames[index], info.shortTitle, 128);
        toTChar(truepan::kParamLabels[index], info.units, 128);
        info.id = (ParamID)index;
        info.stepCount = stepCountFor(index);
        info.defaultNormalizedValue = truepan::defaultParam(index);
        info.unitId = kRootUnitId;
        info.flags = ParameterInfo::kCanAutomate | (info.stepCount > 0 ? ParameterInfo::kIsList : 0);
        valueNormalized = info.defaultNormalizedValue;
    }

    void toString(ParamValue normValue, String128 string) const override
    {
        char buf[64];
        truepan::paramDisplayText((int)info.id, (float)normValue, buf, sizeof buf);
        toTChar(buf, string, 128);
    }

    bool fromString(const TChar* string, ParamValue& normValue) const override
    {
        char buf[64];
        fromTChar(string, buf, sizeof buf);
        float v;
        if (!truepan::paramFromText((int)info.id, buf, v)) return false;
        normValue = v;
        return true;
    }
};

class TruePanView;

// Bypass (VST3-standard): värden visar den som sin egen bypass-knapp.
const ParamID kBypassId = 1000;

// ---------------------------------------------------------------------------
// Effekten: ljud och parametrar i samma objekt (SingleComponentEffect)
// ---------------------------------------------------------------------------
class TruePanVst3 : public SingleComponentEffect, public truepan::Core
{
public:
    static FUnknown* createInstance(void*) { return (IAudioProcessor*)new TruePanVst3; }

    tresult PLUGIN_API initialize(FUnknown* context) override
    {
        const tresult r = SingleComponentEffect::initialize(context);
        if (r != kResultOk) return r;
        addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
        for (int i = 0; i < truepan::kNumParams; ++i) parameters.addParameter(new TPParameter(i));
        parameters.addParameter(STR16("Bypass"), nullptr, 1, 0.0, ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass, kBypassId);
        // Tempo, position, loop och transport behövs för AUTO-PAN och LESLIE
        processContextRequirements.needTempo().needProjectTimeMusic().needCycleMusic().needTransportState();
        return kResultOk;
    }

    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                          SpeakerArrangement* outputs, int32 numOuts) override
    {
        // Bara stereo in och stereo ut
        if (numIns == 1 && numOuts == 1 && inputs[0] == SpeakerArr::kStereo && outputs[0] == SpeakerArr::kStereo)
            return SingleComponentEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
        return kResultFalse;
    }

    tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) override
    {
        return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override
    {
        engine.prepare(setup.sampleRate);
        const int32 maxN = setup.maxSamplesPerBlock > 0 ? setup.maxSamplesPerBlock : 4096;
        dryL_.assign((size_t)maxN, 0.0f);
        dryR_.assign((size_t)maxN, 0.0f);
        bypassCoef_ = 1.0f - (float)std::exp(-1.0 / (0.010 * (setup.sampleRate > 0.0 ? setup.sampleRate : 44100.0)));
        return SingleComponentEffect::setupProcessing(setup);
    }

    tresult PLUGIN_API setActive(TBool state) override
    {
        if (state) engine.reset();
        return SingleComponentEffect::setActive(state);
    }

    tresult PLUGIN_API process(ProcessData& data) override
    {
        // Parameterändringar från värden (automation): senaste värdet i blocket
        if (IParameterChanges* changes = data.inputParameterChanges) {
            const int32 count = changes->getParameterCount();
            for (int32 i = 0; i < count; ++i) {
                IParamValueQueue* q = changes->getParameterData(i);
                if (!q) continue;
                const int32 points = q->getPointCount();
                int32 offset = 0;
                ParamValue value = 0.0;
                if (points > 0 && q->getPoint(points - 1, offset, value) == kResultTrue) {
                    if (q->getParameterId() == kBypassId) {
                        bypass_ = value >= 0.5;
                        // Värden förväntar sig att bypass-läget syns direkt i parametern
                        if (Parameter* bp = parameters.getParameter(kBypassId)) bp->setNormalized(value);
                    }
                    else setParam((int)q->getParameterId(), (float)value);
                }
            }
        }
        if (data.numSamples <= 0 || data.numOutputs < 1 || !data.outputs) return kResultOk;   // bara parametrar
        AudioBusBuffers& out = data.outputs[0];
        if (out.numChannels < 1 || !out.channelBuffers32) return kResultOk;

        truepan::TransportInfo t;
        if (const ProcessContext* c = data.processContext) {
            t.tempoValid = (c->state & ProcessContext::kTempoValid) != 0;
            t.tempo = c->tempo;
            t.playing = (c->state & ProcessContext::kPlaying) != 0;
            t.ppqValid = (c->state & ProcessContext::kProjectTimeMusicValid) != 0;
            t.ppq = c->projectTimeMusic;
            t.cycleOn = (c->state & ProcessContext::kCycleActive) && (c->state & ProcessContext::kCycleValid);
            t.cycleStart = c->cycleStartMusic;
            t.cycleEnd = c->cycleEndMusic;
        }

        const float* inL = nullptr;
        const float* inR = nullptr;
        if (data.numInputs > 0 && data.inputs && data.inputs[0].numChannels > 0 && data.inputs[0].channelBuffers32) {
            inL = data.inputs[0].channelBuffers32[0];
            inR = data.inputs[0].numChannels > 1 ? data.inputs[0].channelBuffers32[1] : inL;
        }
        float* outL = out.channelBuffers32[0];
        float* outR = out.numChannels > 1 ? out.channelBuffers32[1] : nullptr;
        const int32 n = data.numSamples;
        // Bypass: spara originalet (in och ut kan vara samma minne) och tona över på 10 ms
        const bool fading = bypass_ || bypassMix_ > 0.0f;
        const bool canFade = fading && (size_t)n <= dryL_.size();
        if (canFade) {
            for (int32 i = 0; i < n; ++i) {
                dryL_[(size_t)i] = inL ? inL[i] : 0.0f;
                dryR_[(size_t)i] = inR ? inR[i] : 0.0f;
            }
        }
        processAudio(inL, inR, outL, outR, n, false, t);
        if (canFade) {
            const float target = bypass_ ? 1.0f : 0.0f;
            for (int32 i = 0; i < n; ++i) {
                bypassMix_ += (target - bypassMix_) * bypassCoef_;
                if (std::fabs(bypassMix_ - target) < 1e-5f) bypassMix_ = target;
                outL[i] += (dryL_[(size_t)i] - outL[i]) * bypassMix_;
                if (outR) outR[i] += (dryR_[(size_t)i] - outR[i]) * bypassMix_;
            }
        }
        out.silenceFlags = 0;
        return kResultOk;
    }

    // --- Sparat tillstånd (samma format som VST2) ---
    tresult PLUGIN_API getState(IBStream* state) override
    {
        if (!state) return kInvalidArgument;
        const truepan::StateChunk& c = saveState();
        int32 written = 0;
        if (state->write((void*)&c, (int32)sizeof c, &written) != kResultOk || written != (int32)sizeof c) return kResultFalse;
        // Efter chunken: bypass-läget (bara VST3; VST2 läser inte så här långt)
        unsigned char tail[8] = { 'T', 'P', 'b', 'y', 0, 0, 0, 0 };
        tail[4] = bypass_ ? 1 : 0;
        return state->write(tail, 8, &written) == kResultOk && written == 8 ? kResultOk : kResultFalse;
    }

    tresult PLUGIN_API setState(IBStream* state) override
    {
        if (!state) return kInvalidArgument;
        std::vector<unsigned char> data;
        unsigned char buf[512];
        for (;;) {
            int32 got = 0;
            if (state->read(buf, (int32)sizeof buf, &got) != kResultOk || got <= 0) break;
            data.insert(data.end(), buf, buf + got);
            if (data.size() > 65536) break;
        }
        if (!loadState(data.data(), data.size())) return kResultFalse;
        // Bypass-läget efter chunken (finns bara i VST3-projekt)
        bool bp = false;
        const size_t tailAt = sizeof(truepan::StateChunk);
        if (data.size() >= tailAt + 8 && std::memcmp(&data[tailAt], "TPby", 4) == 0) bp = data[tailAt + 4] != 0;
        bypass_ = bp;
        bypassMix_ = bp ? 1.0f : 0.0f;
        SingleComponentEffect::setParamNormalized(kBypassId, bp ? 1.0 : 0.0);
        // Visa de inlästa värdena för värden
        for (int i = 0; i < truepan::kNumParams; ++i) SingleComponentEffect::setParamNormalized((ParamID)i, params[i].load());
        return kResultOk;
    }

    // --- Parametrar från värdens gränssnitt ---
    tresult PLUGIN_API setParamNormalized(ParamID id, ParamValue value) override
    {
        const tresult r = SingleComponentEffect::setParamNormalized(id, value);
        if (id == kBypassId) bypass_ = value >= 0.5;
        else setParam((int)id, (float)value);
        return r;
    }

    // --- Fönstret ---
    IPlugView* PLUGIN_API createView(FIDString name) override;

    // --- Kopplingar från kärnan (fönstret ändrar parametrar) ---
    void hostAutomate(int index, float value) override
    {
        SingleComponentEffect::setParamNormalized((ParamID)index, value);
        EditController::performEdit((ParamID)index, value);
    }
    void hostBeginEdit(int index) override { EditController::beginEdit((ParamID)index); }
    void hostEndEdit(int index) override { EditController::endEdit((ParamID)index); }

private:
    bool bypass_ = false;
    float bypassMix_ = 0.0f, bypassCoef_ = 0.003f;
    std::vector<float> dryL_, dryR_;
};

// ---------------------------------------------------------------------------
// Fönstret: samma GDI+-fönster som i VST2-versionen
// ---------------------------------------------------------------------------
class TruePanView : public CPluginView
{
public:
    explicit TruePanView(truepan::Core* core) : CPluginView(nullptr), core_(core)
    {
        rect = ViewRect(0, 0, truepan::kEditorWidth, truepan::kEditorHeight);
    }

    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override
    {
#if defined(_WIN32)
        return (type && std::strcmp(type, kPlatformTypeHWND) == 0) ? kResultTrue : kResultFalse;
#else
        (void)type;
        return kResultFalse;
#endif
    }

    tresult PLUGIN_API attached(void* parent, FIDString type) override
    {
        if (isPlatformTypeSupported(type) != kResultTrue) return kResultFalse;
        const tresult r = CPluginView::attached(parent, type);
#if defined(_WIN32)
        truepan::editorOpen(core_, parent);
#endif
        return r;
    }

    tresult PLUGIN_API removed() override
    {
#if defined(_WIN32)
        truepan::editorClose(core_);
#endif
        return CPluginView::removed();
    }

    tresult PLUGIN_API canResize() override { return kResultFalse; }

    // Piltangenterna vänster/höger flyttar PAN ett steg
    tresult PLUGIN_API onKeyDown(char16 key, int16 keyCode, int16 modifiers) override
    {
#if defined(_WIN32)
        if (keyCode == KEY_LEFT || keyCode == KEY_RIGHT)
            return truepan::editorKey(core_, keyCode == KEY_LEFT ? -1 : 1) ? kResultTrue : kResultFalse;
#endif
        return CPluginView::onKeyDown(key, keyCode, modifiers);
    }

private:
    truepan::Core* core_;
};

IPlugView* PLUGIN_API TruePanVst3::createView(FIDString name)
{
#if defined(_WIN32)
    if (name && std::strcmp(name, ViewType::kEditor) == 0) return new TruePanView(this);
#else
    (void)name;
#endif
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// Fabriken: värden hittar TruePan här
// ---------------------------------------------------------------------------
BEGIN_FACTORY_DEF("2F Productions", "https://2f.se", "mailto:info@2f.se")

    DEF_CLASS2(INLINE_UID_FROM_FUID(kTruePanUid),
               PClassInfo::kManyInstances,
               kVstAudioEffectClass,
               "TruePan",
               0,                       // SingleComponentEffect: ljud och parametrar i samma objekt
               "Fx|Spatial",
               "2.6.3",
               kVstVersionString,
               TruePanVst3::createInstance)

END_FACTORY
