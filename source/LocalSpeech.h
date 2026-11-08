// =============================================================================
//  CUPIDO — Reconhecimento de voz local (offline e grátis) com whisper.cpp.
//  O modelo é baixado uma vez (Documentos/CUPIDO/Modelos) na primeira vez que
//  o usuário usa o microfone. Depois funciona sem internet.
// =============================================================================
#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <atomic>

namespace cupido
{
class LocalSpeech
{
public:
    static LocalSpeech& instance();
    static bool isAvailable();                      // compilado com whisper?
    static juce::File modelsFolder();

    /** "tiny" | "base" | "small" */
    juce::File modelFile (const juce::String& size) const;
    bool hasModel (const juce::String& size) const { return modelFile (size).existsAsFile(); }

    /** Baixa o modelo (bloqueante; chamar em thread de fundo). progress 0..1. */
    juce::String download (const juce::String& size);
    float downloadProgress() const { return progress.load(); }
    bool isDownloading() const { return downloading.load(); }

    /** Transcreve áudio mono 16 kHz. Bloqueante. */
    juce::String transcribe (const std::vector<float>& pcm16k, const juce::String& size, const juce::String& language, juce::String& error);

private:
    LocalSpeech() = default;
    ~LocalSpeech();
    juce::CriticalSection lock;
    void* ctx = nullptr; juce::String ctxModel;
    std::atomic<float> progress { 0 }; std::atomic<bool> downloading { false };
};
} // namespace cupido
