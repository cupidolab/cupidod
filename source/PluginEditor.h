// =============================================================================
//  CUPIDO — Editor: interface em HTML/CSS/JS (pasta ui/) rodando num WebView
//  nativo (WebView2 no Windows, WKWebView no macOS, WebKitGTK no Linux),
//  conectada ao processador por funções nativas e eventos.
// =============================================================================
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "PluginProcessor.h"
#include "AiClient.h"
#include "LocalSpeech.h"

/** Gravador do microfone do sistema para comandos de voz (fonte "mic"). */
class MicRecorder : private juce::AudioIODeviceCallback
{
public:
    ~MicRecorder() override { stop(); }
    juce::String start();                 // "" = ok, senão mensagem de erro
    std::vector<float> stop();            // devolve mono 16 kHz
    bool isRecording() const { return recording.load(); }
    float level() const { return lvl.load(); }
private:
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int n,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* d) override { rate = d->getCurrentSampleRate(); }
    void audioDeviceStopped() override {}
    std::unique_ptr<juce::AudioDeviceManager> dm;
    std::vector<float> buf; std::atomic<int> pos { 0 }; std::atomic<bool> recording { false }; std::atomic<float> lvl { 0 };
    double rate = 48000.0;
};

class CupidoEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit CupidoEditor (CupidoProcessor&);
    ~CupidoEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;
    juce::WebBrowserComponent::Options buildOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    void timerCallback() override;

    // ---- funções nativas chamadas pelo JavaScript ----
    juce::var  getSpec();
    void setParam (const juce::String& id, float value, bool notifyGesture);
    void setParams (const juce::var& obj);
    void runAnalysis (bool beat, Completion done);
    void askAI (const juce::String& text, const juce::String& context, Completion done);
    void startVoice (const juce::String& source, Completion done);
    void stopVoice (Completion done);
    juce::var listPresets();
    bool savePreset (const juce::String& name, const juce::String& json);
    bool deletePreset (const juce::String& name);
    juce::String compactSpec() const;
    void pickAudio (const juce::String& kind, Completion done);
    void runAudioJob (const juce::String& kind, std::function<juce::String()> job, Completion done);
    static void finishAsync (std::shared_ptr<std::atomic<bool>> alive, Completion done, juce::var result);

    CupidoProcessor& proc;
    std::shared_ptr<cupido::AiSettings> settings = std::make_shared<cupido::AiSettings>();
    MicRecorder mic;
    juce::String voiceSourceInUse;
    struct SharedPool { juce::ThreadPool pool { 2 }; };          // compartilhado entre instâncias e sobrevive ao editor
    juce::SharedResourcePointer<SharedPool> sharedPool;
    juce::ThreadPool& pool() { return sharedPool->pool; }
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::WebBrowserComponent> web;
    std::vector<float> lastSent;      // últimos valores enviados à UI (para mandar só o que mudou)
    int frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CupidoEditor)
};
