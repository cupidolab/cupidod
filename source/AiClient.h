// =============================================================================
//  CUPIDO — Cliente de IA
//   • Claude (API da Anthropic): entende o pedido em português/gíria e devolve
//     um JSON com resposta + mudanças de parâmetros.
//   • Transcrição de voz (endpoint OpenAI-compatível /audio/transcriptions):
//     Groq (grátis/rápido), OpenAI, ou servidor local (whisper.cpp / faster-whisper).
//   • Configurações persistidas fora da sessão (chaves NUNCA vão para o projeto do DAW).
// =============================================================================
#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

namespace cupido
{
/** Configurações globais (arquivo em AppData/Application Support). */
class AiSettings
{
public:
    AiSettings();
    juce::String get (const juce::String& key, const juce::String& fallback = {}) const;
    void set (const juce::String& key, const juce::String& value);
    juce::var toPublicJson() const;          // sem expor as chaves inteiras
    void applyFromJson (const juce::var& obj);
    static juce::File presetsFolder();
private:
    std::unique_ptr<juce::PropertiesFile> props;
};

struct AiResult
{
    bool ok = false;
    juce::String text;      // resposta (chat) ou transcrição (voz)
    juce::var    json;      // objeto devolvido pelo Claude (reply/changes/...)
    juce::String error;
};

class AiClient
{
public:
    /** Pede ao Claude: userText + contexto (estado atual, análise) → JSON. Bloqueante: chamar em thread de fundo. */
    static AiResult askClaude (const AiSettings& s, const juce::String& userText, const juce::String& contextJson,
                               const juce::String& paramSpecCompact);

    /** Transcreve WAV 16 kHz mono. Bloqueante: chamar em thread de fundo. */
    static AiResult transcribe (const AiSettings& s, const juce::MemoryBlock& wav);

    /** Testa as credenciais configuradas (Claude e/ou voz). */
    static AiResult testClaude (const AiSettings& s);

    static juce::String buildSystemPrompt (const juce::String& paramSpecCompact);
};
} // namespace cupido
