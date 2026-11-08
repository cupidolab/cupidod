// =============================================================================
//  CUPIDO — MediaLab: o "ouvido" do Cupidinho para arquivos anexados.
//   • Decodifica beat / voz (acapella) / referência (WAV, AIFF, FLAC, OGG, MP3; M4A no Mac/Win)
//   • Analisa a faixa inteira (BPM, tom, loudness, espectro, marcas de estilo)
//   • Renderiza a mix offline (voz processada com os ajustes atuais + beat),
//     com uma versão "antes" para comparar, e salva em Documentos/CUPIDO/Mixes
//   • Gera imagem do espectrograma (para o Claude "ver" o som)
// =============================================================================
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_graphics/juce_graphics.h>
#include "Params.h"
#include "dsp/Chain.h"

namespace cupido
{
struct AudioClip
{
    juce::String kind, name;          // kind: "beat" | "voice" | "reference"
    double sampleRate = 44100;
    std::vector<float> L, R;          // R vazio = mono
    TrackFeatures features;
};

class MediaLab
{
public:
    MediaLab();

    /** Decodifica e analisa (bloqueante — chamar em thread de fundo). */
    juce::String load (const juce::String& kind, const juce::File& file);          // "" = ok, senão erro
    juce::String loadFromBase64 (const juce::String& kind, const juce::String& name, const juce::String& base64);
    void clear (const juce::String& kind);

    std::shared_ptr<const AudioClip> get (const juce::String& kind) const;
    juce::var describe (const juce::String& kind) const;    // features + espectro + forma de onda (JSON)
    juce::var describeAll() const;

    /** Renderiza a mix com os parâmetros atuais. Retorna {ok, after, before, lufs, ...}. */
    juce::var renderMix (const ParamValues& params, double hostBpm);

    /** PNG (base64) com espectrogramas de beat e voz — para o Claude analisar visualmente. */
    juce::String spectrogramPngBase64() const;

    /** Arquivos renderizados servidos para o player da interface. */
    juce::File mediaFolder() const;
    static juce::File mixesFolder();

private:
    juce::String decode (const juce::String& kind, juce::InputStream* in, const juce::String& name, const juce::String& ext);
    mutable juce::CriticalSection lock;
    std::map<juce::String, std::shared_ptr<AudioClip>> clips;
    juce::AudioFormatManager formats;
};
} // namespace cupido
