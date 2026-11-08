#include "PluginEditor.h"
#include "VoiceUtils.h"
#include "BinaryData.h"

using namespace cupido;

// =============================================================================
//  Gravador do microfone (comandos de voz)
// =============================================================================
juce::String MicRecorder::start()
{
    if (recording.load()) return {};
    if (dm == nullptr)
    {
        dm = std::make_unique<juce::AudioDeviceManager>();
        const auto err = dm->initialiseWithDefaultDevices (1, 0);
        if (err.isNotEmpty()) { dm.reset(); return "Microfone indisponível: " + err; }
    }
    if (dm->getCurrentAudioDevice() == nullptr) return "Nenhum microfone encontrado.";
    rate = dm->getCurrentAudioDevice()->getCurrentSampleRate();
    buf.assign ((size_t) (rate * 20.0), 0.0f); pos.store (0); recording.store (true);
    dm->addAudioCallback (this);
    return {};
}

std::vector<float> MicRecorder::stop()
{
    if (dm == nullptr) return {};
    recording.store (false);
    dm->removeAudioCallback (this);
    std::vector<float> rec (buf.begin(), buf.begin() + std::min<int> (pos.load(), (int) buf.size()));
    dm->closeAudioDevice(); dm.reset();       // libera o mic (não briga com o DAW)
    return resampleTo (rec, rate, 16000.0);
}

void MicRecorder::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int n,
                                                     const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c) if (out[c]) juce::FloatVectorOperations::clear (out[c], n);
    if (! recording.load() || numIn <= 0 || in[0] == nullptr) return;
    int p = pos.load(); float pk = 0;
    for (int i = 0; i < n && p < (int) buf.size(); ++i, ++p)
    {
        float s = in[0][i]; if (numIn > 1 && in[1]) s = 0.5f * (s + in[1][i]);
        buf[(size_t) p] = s; pk = std::max (pk, std::abs (s));
    }
    pos.store (p); lvl.store (pk);
}

// =============================================================================
//  Editor
// =============================================================================
static juce::var toVar (const AnalysisResult& r)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("valid", r.valid); o->setProperty ("seconds", r.seconds);
    o->setProperty ("bpm", r.bpm); o->setProperty ("bpmConfidence", r.bpmConfidence);
    o->setProperty ("key", r.key); o->setProperty ("keyName", juce::String (keyName (r.key)));
    o->setProperty ("minor", r.minor); o->setProperty ("keyConfidence", r.keyConfidence);
    o->setProperty ("rmsDb", r.rmsDb); o->setProperty ("peakDb", r.peakDb); o->setProperty ("crestDb", r.crestDb);
    o->setProperty ("noiseFloorDb", r.noiseFloorDb); o->setProperty ("sibilanceDb", r.sibilanceDb);
    juce::Array<juce::var> b, bd; for (int i = 0; i < 6; ++i) { b.add (r.bands[i]); bd.add (r.bandsDb[i]); }
    o->setProperty ("bands", b); o->setProperty ("bandsDb", bd);
    return juce::var (o);
}

CupidoEditor::CupidoEditor (CupidoProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lastSent.assign ((size_t) kNumParams, std::numeric_limits<float>::quiet_NaN());
    web = std::make_unique<juce::WebBrowserComponent> (buildOptions());
    addAndMakeVisible (*web);
    web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    setResizeLimits (960, 640, 2600, 1600);
    setSize (1240, 800);
    startTimerHz (30);
}

CupidoEditor::~CupidoEditor()
{
    alive->store (false);
    stopTimer();
    if (mic.isRecording()) mic.stop();
}

void CupidoEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0b0614)); }
void CupidoEditor::resized() { if (web) web->setBounds (getLocalBounds()); }

void CupidoEditor::finishAsync (std::shared_ptr<std::atomic<bool>> a, Completion done, juce::var result)
{
    juce::MessageManager::callAsync ([a, done, result] { if (a->load()) done (result); });
}

juce::WebBrowserComponent::Options CupidoEditor::buildOptions()
{
    using WBC = juce::WebBrowserComponent;
    auto userData = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("CupidoWebView2");

    return WBC::Options{}
        .withBackend (WBC::Options::Backend::webview2)
        .withWinWebView2Options (WBC::Options::WinWebView2{}
                                    .withUserDataFolder (userData)
                                    .withBackgroundColour (juce::Colour (0xff0b0614))
                                    .withStatusBarDisabled())
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
        .withNativeFunction ("getSpec", [this] (const juce::Array<juce::var>&, Completion done) { done (getSpec()); })
        .withNativeFunction ("setParam", [this] (const juce::Array<juce::var>& a, Completion done)
            { if (a.size() >= 2) setParam (a[0].toString(), (float) a[1], a.size() < 3 || (bool) a[2]); done ({}); })
        .withNativeFunction ("beginGesture", [this] (const juce::Array<juce::var>& a, Completion done)
            { if (auto* p = proc.getParam (a[0].toString())) p->beginChangeGesture(); done ({}); })
        .withNativeFunction ("endGesture", [this] (const juce::Array<juce::var>& a, Completion done)
            { if (auto* p = proc.getParam (a[0].toString())) p->endChangeGesture(); done ({}); })
        .withNativeFunction ("setParams", [this] (const juce::Array<juce::var>& a, Completion done)
            { if (a.size() > 0) setParams (a[0]); done ({}); })
        .withNativeFunction ("setPresetName", [this] (const juce::Array<juce::var>& a, Completion done)
            { proc.currentPresetName = a[0].toString(); done ({}); })
        .withNativeFunction ("learnNoise", [this] (const juce::Array<juce::var>&, Completion done)
            { proc.getChain().learnNoise (2.0f); done (true); })
        .withNativeFunction ("resetLufs", [this] (const juce::Array<juce::var>&, Completion done)
            { proc.getChain().resetLoudness(); done (true); })
        .withNativeFunction ("analyze", [this] (const juce::Array<juce::var>& a, Completion done)
            { runAnalysis (a.size() > 0 && a[0].toString() == "beat", std::move (done)); })
        .withNativeFunction ("askAI", [this] (const juce::Array<juce::var>& a, Completion done)
            { askAI (a[0].toString(), a.size() > 1 ? a[1].toString() : "{}", std::move (done)); })
        .withNativeFunction ("startVoice", [this] (const juce::Array<juce::var>& a, Completion done)
            { startVoice (a.size() > 0 ? a[0].toString() : settings->get ("voiceSource", "mic"), std::move (done)); })
        .withNativeFunction ("stopVoice", [this] (const juce::Array<juce::var>&, Completion done) { stopVoice (std::move (done)); })
        .withNativeFunction ("getSettings", [this] (const juce::Array<juce::var>&, Completion done) { done (settings->toPublicJson()); })
        .withNativeFunction ("setSettings", [this] (const juce::Array<juce::var>& a, Completion done)
            { if (a.size() > 0) settings->applyFromJson (a[0]); done (settings->toPublicJson()); })
        .withNativeFunction ("testAI", [this] (const juce::Array<juce::var>&, Completion done)
            {
                auto s = settings; auto a = alive;
                pool().addJob ([s, a, done]
                {
                    auto r = AiClient::testClaude (*s);
                    auto* o = new juce::DynamicObject(); o->setProperty ("ok", r.ok); o->setProperty ("text", r.text); o->setProperty ("error", r.error);
                    finishAsync (a, done, juce::var (o));
                });
            })
        .withNativeFunction ("listPresets", [this] (const juce::Array<juce::var>&, Completion done) { done (listPresets()); })
        .withNativeFunction ("savePreset", [this] (const juce::Array<juce::var>& a, Completion done)
            { done (a.size() >= 2 && savePreset (a[0].toString(), a[1].toString())); })
        .withNativeFunction ("deletePreset", [this] (const juce::Array<juce::var>& a, Completion done)
            { done (a.size() >= 1 && deletePreset (a[0].toString())); })
        .withNativeFunction ("openPresetsFolder", [] (const juce::Array<juce::var>&, Completion done)
            { AiSettings::presetsFolder().startAsProcess(); done (true); })
        .withNativeFunction ("openUrl", [] (const juce::Array<juce::var>& a, Completion done)
            { const auto u = a[0].toString(); if (u.startsWith ("https://")) juce::URL (u).launchInDefaultBrowser(); done (true); });
}

// -----------------------------------------------------------------------------
//  Arquivos da interface embutidos no binário (BinaryData)
// -----------------------------------------------------------------------------
std::optional<juce::WebBrowserComponent::Resource> CupidoEditor::getResource (const juce::String& url)
{
    auto path = url.fromFirstOccurrenceOf ("/", false, false).upToFirstOccurrenceOf ("?", false, false);
    if (path.isEmpty() || url == "/") path = "index.html";
    const auto file = path.fromLastOccurrenceOf ("/", false, false);

    for (int i = 0; i < CupidoUI::namedResourceListSize; ++i)
    {
        if (file != CupidoUI::originalFilenames[i]) continue;
        int size = 0;
        if (const char* data = CupidoUI::getNamedResource (CupidoUI::namedResourceList[i], size))
        {
            std::vector<std::byte> bytes ((size_t) size);
            std::memcpy (bytes.data(), data, (size_t) size);
            const auto ext = file.fromLastOccurrenceOf (".", false, false).toLowerCase();
            juce::String mime = "application/octet-stream";
            if (ext == "html") mime = "text/html"; else if (ext == "js") mime = "text/javascript";
            else if (ext == "css") mime = "text/css"; else if (ext == "svg") mime = "image/svg+xml";
            else if (ext == "png") mime = "image/png"; else if (ext == "json") mime = "application/json";
            else if (ext == "woff2") mime = "font/woff2";
            return juce::WebBrowserComponent::Resource { std::move (bytes), mime };
        }
    }
    return std::nullopt;
}

// -----------------------------------------------------------------------------
//  Especificação + estado
// -----------------------------------------------------------------------------
juce::var CupidoEditor::getSpec()
{
    juce::Array<juce::var> spec;
    auto* values = new juce::DynamicObject();
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& p = paramTable()[i];
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", p.id); o->setProperty ("label", juce::String (juce::CharPointer_UTF8 (p.label)));
        o->setProperty ("module", p.module); o->setProperty ("min", p.min); o->setProperty ("max", p.max);
        o->setProperty ("def", p.def); o->setProperty ("step", p.step); o->setProperty ("skew", p.skew);
        o->setProperty ("unit", juce::String (juce::CharPointer_UTF8 (p.unit)));
        juce::Array<juce::var> ch;
        for (auto& s : juce::StringArray::fromTokens (juce::String (juce::CharPointer_UTF8 (p.choices)), "|", "")) if (s.isNotEmpty()) ch.add (s);
        o->setProperty ("choices", ch);
        spec.add (juce::var (o));
        values->setProperty (p.id, proc.getPlainValue (i));
    }
    auto* info = new juce::DynamicObject();
    info->setProperty ("version", JucePlugin_VersionString);
    info->setProperty ("format", juce::String (juce::AudioProcessor::getWrapperTypeDescription (proc.wrapperType)));
    info->setProperty ("sampleRate", proc.getSampleRate());
    info->setProperty ("latency", proc.getLatencySamples());
    info->setProperty ("preset", proc.currentPresetName);
    info->setProperty ("host", juce::PluginHostType().getHostDescription());

    auto* res = new juce::DynamicObject();
    res->setProperty ("spec", spec); res->setProperty ("values", juce::var (values)); res->setProperty ("info", juce::var (info));
    res->setProperty ("settings", settings->toPublicJson());
    return juce::var (res);
}

void CupidoEditor::setParam (const juce::String& id, float value, bool gesture)
{
    if (auto* p = proc.getParam (id))
    {
        const float norm = p->convertTo0to1 (value);
        if (gesture) p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        if (gesture) p->endChangeGesture();
    }
}

void CupidoEditor::setParams (const juce::var& obj)
{
    if (auto* o = obj.getDynamicObject())
        for (auto& prop : o->getProperties()) setParam (prop.name.toString(), (float) prop.value, true);
}

// -----------------------------------------------------------------------------
//  Análise (beat no sidechain ou voz na entrada) — thread de fundo
// -----------------------------------------------------------------------------
void CupidoEditor::runAnalysis (bool beat, Completion done)
{
    auto* chain = &proc.getChain(); auto a = alive; const double hostBpm = proc.getHostBpm();
    pool().addJob ([chain, beat, done, a, hostBpm]
    {
        auto r = chain->analyse (beat);
        if (beat && r.valid && r.bpm > 0) chain->setAnalysedBpm (r.bpm);
        auto v = toVar (r);
        v.getDynamicObject()->setProperty ("source", beat ? "beat" : "voice");
        v.getDynamicObject()->setProperty ("sidechainActive", chain->meters.sidechainActive.load());
        v.getDynamicObject()->setProperty ("hostBpm", hostBpm);
        finishAsync (a, done, v);
    });
}

juce::String CupidoEditor::compactSpec() const
{
    juce::String s;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& p = paramTable()[i];
        s << p.id << " | " << juce::String (juce::CharPointer_UTF8 (p.label)) << " | " << p.module << " | "
          << juce::String (p.min) << ".." << juce::String (p.max) << " | " << juce::String (juce::CharPointer_UTF8 (p.unit));
        if (*p.choices)
        {
            auto ch = juce::StringArray::fromTokens (juce::String (juce::CharPointer_UTF8 (p.choices)), "|", "");
            s << " | "; for (int k = 0; k < ch.size(); ++k) s << k << "=" << ch[k] << (k + 1 < ch.size() ? ", " : "");
        }
        s << "\n";
    }
    return s;
}

void CupidoEditor::askAI (const juce::String& text, const juce::String& context, Completion done)
{
    auto spec = compactSpec(); auto s = settings; auto a = alive;
    pool().addJob ([text, context, spec, s, a, done]
    {
        auto r = AiClient::askClaude (*s, text, context, spec);
        auto* o = new juce::DynamicObject();
        o->setProperty ("ok", r.ok); o->setProperty ("error", r.error); o->setProperty ("reply", r.text); o->setProperty ("data", r.json);
        finishAsync (a, done, juce::var (o));
    });
}

// -----------------------------------------------------------------------------
//  Comandos de voz: segura o botão → grava → solta → transcreve
// -----------------------------------------------------------------------------
void CupidoEditor::startVoice (const juce::String& source, Completion done)
{
    voiceSourceInUse = source;
    auto* o = new juce::DynamicObject();
    if (source == "channel") { proc.startChannelVoiceCapture(); o->setProperty ("ok", true); }
    else
    {
        const auto err = mic.start();
        o->setProperty ("ok", err.isEmpty()); o->setProperty ("error", err);
    }
    done (juce::var (o));
}

void CupidoEditor::stopVoice (Completion done)
{
    std::vector<float> audio = voiceSourceInUse == "channel" ? proc.stopChannelVoiceCapture() : mic.stop();
    if (audio.size() < 16000 / 3)
    {
        auto* o = new juce::DynamicObject(); o->setProperty ("ok", false); o->setProperty ("error", "Áudio muito curto — segura o botão enquanto fala.");
        done (juce::var (o)); return;
    }
    normalisePeak (audio);
    auto wav = encodeWav16 (audio, 16000);
    juce::MemoryBlock mb (wav.data(), wav.size());
    auto s = settings; auto a = alive;
    pool().addJob ([mb, s, a, done]
    {
        auto r = AiClient::transcribe (*s, mb);
        auto* o = new juce::DynamicObject(); o->setProperty ("ok", r.ok); o->setProperty ("text", r.text); o->setProperty ("error", r.error);
        finishAsync (a, done, juce::var (o));
    });
}

// -----------------------------------------------------------------------------
//  Presets do usuário (Documentos/CUPIDO/Presets/*.json)
// -----------------------------------------------------------------------------
static juce::String safeName (const juce::String& n)
{ return juce::File::createLegalFileName (n.trim()).substring (0, 64); }

juce::var CupidoEditor::listPresets()
{
    juce::Array<juce::var> out;
    for (auto& f : AiSettings::presetsFolder().findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        auto parsed = juce::JSON::parse (f.loadFileAsString());
        if (! parsed.isObject()) continue;
        auto* o = new juce::DynamicObject(); o->setProperty ("name", f.getFileNameWithoutExtension()); o->setProperty ("data", parsed);
        out.add (juce::var (o));
    }
    return out;
}

bool CupidoEditor::savePreset (const juce::String& name, const juce::String& json)
{
    const auto n = safeName (name); if (n.isEmpty()) return false;
    return AiSettings::presetsFolder().getChildFile (n + ".json").replaceWithText (json);
}

bool CupidoEditor::deletePreset (const juce::String& name)
{
    auto f = AiSettings::presetsFolder().getChildFile (safeName (name) + ".json");
    return f.existsAsFile() && f.moveToTrash();
}

// -----------------------------------------------------------------------------
//  30 fps: valores que mudaram (automação/host) + medidores + espectro
// -----------------------------------------------------------------------------
void CupidoEditor::timerCallback()
{
    if (web == nullptr) return;
    ++frame;
    auto& m = proc.getChain().meters;
    auto* o = new juce::DynamicObject();

    auto* changed = new juce::DynamicObject(); int nChanged = 0;
    for (int i = 0; i < kNumParams; ++i)
    {
        const float v = proc.getPlainValue (i);
        if (! (v == lastSent[(size_t) i])) { lastSent[(size_t) i] = v; changed->setProperty (paramTable()[i].id, v); ++nChanged; }
    }
    if (nChanged > 0) o->setProperty ("v", juce::var (changed)); else delete changed;

    auto* mt = new juce::DynamicObject();
    mt->setProperty ("inL", m.inL.load()); mt->setProperty ("inR", m.inR.load());
    mt->setProperty ("outL", m.outL.load()); mt->setProperty ("outR", m.outR.load());
    mt->setProperty ("gate", m.grGate.load()); mt->setProperty ("deess", m.grDeess.load());
    mt->setProperty ("comp", m.grComp.load()); mt->setProperty ("opto", m.grOpto.load());
    mt->setProperty ("glue", m.grGlue.load()); mt->setProperty ("lim", m.grLim.load());
    mt->setProperty ("hz", m.pitchHz.load()); mt->setProperty ("midi", m.pitchMidi.load());
    mt->setProperty ("target", m.targetMidi.load()); mt->setProperty ("corr", m.correction.load());
    mt->setProperty ("lufsM", m.lufsM.load()); mt->setProperty ("lufsS", m.lufsS.load()); mt->setProperty ("lufsI", m.lufsI.load());
    mt->setProperty ("sc", m.sidechainActive.load());
    mt->setProperty ("bpm", proc.getChain().currentBpm());
    mt->setProperty ("learning", proc.getChain().isLearningNoise());
    mt->setProperty ("noiseProfile", proc.getChain().hasNoiseProfile());
    mt->setProperty ("mic", mic.level());
    mt->setProperty ("latency", proc.getLatencySamples());
    o->setProperty ("m", juce::var (mt));

    if (frame % 2 == 0)
    {
        std::vector<float> bands; const double sr = proc.getChain().sampleRate();
        computeDisplaySpectrum (proc.getChain().outputSnapshot(), sr, 96, bands);
        juce::Array<juce::var> a; for (float b : bands) a.add (juce::roundToInt (b * 10.0f) / 10.0);
        o->setProperty ("sp", a);
        if (m.sidechainActive.load())
        {
            computeDisplaySpectrum (proc.getChain().beatSnapshot(), sr, 96, bands);
            juce::Array<juce::var> b; for (float x : bands) b.add (juce::roundToInt (x * 10.0f) / 10.0);
            o->setProperty ("bsp", b);
        }
    }
    web->emitEventIfBrowserIsVisible ("cupido", juce::var (o));
}
