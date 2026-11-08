#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "VoiceUtils.h"

using namespace cupido;

// -----------------------------------------------------------------------------
//  Layout de parâmetros gerado a partir da tabela única (Params.h)
// -----------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout CupidoProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& p = paramTable()[i];
        const juce::ParameterID pid { p.id, 1 };
        const juce::String unit (juce::CharPointer_UTF8 (p.unit));
        const juce::String label (juce::CharPointer_UTF8 (p.label));
        if (unit == "bool")
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, label, p.def > 0.5f));
        }
        else if (unit == "choice")
        {
            auto choices = juce::StringArray::fromTokens (juce::String (juce::CharPointer_UTF8 (p.choices)), "|", "");
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, label, choices, (int) p.def));
        }
        else
        {
            juce::NormalisableRange<float> range (p.min, p.max, p.step > 0 ? p.step : 0.0f);
            if (p.skew == 1 && p.min > 0) range.setSkewForCentre (std::sqrt (p.min * p.max));
            auto attrs = juce::AudioParameterFloatAttributes().withLabel (unit)
                .withStringFromValueFunction ([unit] (float v, int) {
                    if (unit == "Hz" && v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                    return juce::String (v, std::abs (v) < 10.0f ? 2 : 1) + (unit.isEmpty() ? "" : " " + unit); });
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, label, range, p.def, attrs));
        }
    }
    return layout;
}

CupidoProcessor::CupidoProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Entrada",       juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Saída",         juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain (Beat)", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "CUPIDO", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (paramTable()[i].id);
    voiceBuf.assign ((size_t) (48000 * 20), 0.0f);
}

CupidoProcessor::~CupidoProcessor() { cancelPendingUpdate(); }

bool CupidoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    if (in.size() > out.size()) return false;       // mono->estéreo ok, estéreo->mono não
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void CupidoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr = sampleRate; currentBlock = std::max (samplesPerBlock, 32);
    const int mode = (int) raw[(size_t) P::LatencyMode]->load();
    chain.prepare (sampleRate, currentBlock, mode == 1);
    preparedLatencyMode.store (mode);
    setLatencySamples (chain.getLatency());
    monoScratch.setSize (2, currentBlock, false, true, true);
    voiceBuf.assign ((size_t) (sampleRate * 20.0), 0.0f);
}

void CupidoProcessor::handleAsyncUpdate()
{
    // Mudança de modo (HQ <-> baixa latência) exige realocação: fora da thread de áudio.
    suspendProcessing (true);
    prepareToPlay (currentSr, currentBlock);
    suspendProcessing (false);
    updateHostDisplay (ChangeDetails().withLatencyChanged (true));
}

void CupidoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    if ((int) raw[(size_t) P::LatencyMode]->load() != preparedLatencyMode.load()) triggerAsyncUpdate();

    // BPM do host (para o delay sincronizado)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) hostBpm.store (*bpm);

    for (int i = 0; i < kNumParams; ++i) values.v[i] = raw[(size_t) i]->load();
    chain.setParams (values, hostBpm.load());

    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int inCh = mainIn.getNumChannels(), outCh = mainOut.getNumChannels();

    // sidechain (beat) — opcional
    const float* scL = nullptr; const float* scR = nullptr;
    if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0) { scL = sc.getReadPointer (0); scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : nullptr; }
    }
    // copiamos o sidechain antes de escrever na saída (alguns hosts compartilham buffers)
    if (scL != nullptr)
    {
        if (monoScratch.getNumSamples() < n) return;   // nunca deveria acontecer
        monoScratch.copyFrom (0, 0, scL, n); scL = monoScratch.getReadPointer (0);
        if (scR) { monoScratch.copyFrom (1, 0, scR, n); scR = monoScratch.getReadPointer (1); }
    }

    // captura de voz pelo canal (comando de voz) — antes do processamento
    if (voiceRecording.load() && inCh > 0)
    {
        int pos = voicePos.load();
        for (int i = 0; i < n && pos < (int) voiceBuf.size(); ++i, ++pos)
            voiceBuf[(size_t) pos] = inCh > 1 ? 0.5f * (mainIn.getSample (0, i) + mainIn.getSample (1, i)) : mainIn.getSample (0, i);
        voicePos.store (pos);
    }

    float* L = mainOut.getWritePointer (0);
    float* R = outCh > 1 ? mainOut.getWritePointer (1) : nullptr;
    if (inCh == 1 && R != nullptr) std::copy (L, L + n, R);   // mono -> estéreo
    chain.process (L, R, scL, scR, n);
}

void CupidoProcessor::startChannelVoiceCapture() { voicePos.store (0); voiceRecording.store (true); }

std::vector<float> CupidoProcessor::stopChannelVoiceCapture()
{
    voiceRecording.store (false);
    const int count = voicePos.load();
    std::vector<float> rec (voiceBuf.begin(), voiceBuf.begin() + std::min<int> (count, (int) voiceBuf.size()));
    return resampleTo (rec, currentSr, 16000.0);
}

// -----------------------------------------------------------------------------
//  Estado (sessão do DAW)
// -----------------------------------------------------------------------------
void CupidoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPresetName, nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void CupidoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPresetName = tree.getProperty ("preset", "Init").toString();
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* CupidoProcessor::createEditor() { return new CupidoEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CupidoProcessor(); }
