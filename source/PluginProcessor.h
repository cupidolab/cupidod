// =============================================================================
//  CUPIDO — Processador do plugin (VST3 / AU / AAX / LV2 / Standalone)
// =============================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "dsp/Chain.h"

class CupidoProcessor : public juce::AudioProcessor,
                        private juce::AsyncUpdater
{
public:
    CupidoProcessor();
    ~CupidoProcessor() override;

    // ---- AudioProcessor ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- API usada pelo editor / IA ----
    juce::AudioProcessorValueTreeState apvts;
    cupido::VocalChain& getChain() { return chain; }

    juce::RangedAudioParameter* getParam (const juce::String& id) const { return apvts.getParameter (id); }
    float getPlainValue (int index) const { return raw[(size_t) index]->load(); }
    double getHostBpm() const { return hostBpm.load(); }

    /** Captura de voz pelo próprio canal (comandos de voz, fonte "canal"). Retorna mono 16 kHz. */
    void startChannelVoiceCapture();
    std::vector<float> stopChannelVoiceCapture();

    /** Nome do último preset carregado (salvo junto com a sessão). */
    juce::String currentPresetName { "Init" };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void handleAsyncUpdate() override;   // re-prepara quando o modo de latência muda

    cupido::VocalChain chain;
    std::array<std::atomic<float>*, cupido::kNumParams> raw {};
    cupido::ParamValues values;
    std::atomic<double> hostBpm { 0.0 };
    double currentSr = 48000.0; int currentBlock = 512;
    std::atomic<int> preparedLatencyMode { -1 };
    juce::AudioBuffer<float> monoScratch;

    // captura de voz pelo canal
    std::vector<float> voiceBuf; std::atomic<int> voicePos { 0 }; std::atomic<bool> voiceRecording { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CupidoProcessor)
};
