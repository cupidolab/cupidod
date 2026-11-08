// =============================================================================
//  CUPIDO — utilidades de voz: reamostragem para 16 kHz e codificação WAV PCM16
//  (formato aceito por Whisper / Groq / whisper.cpp / qualquer endpoint OpenAI-compatível)
// =============================================================================
#pragma once
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace cupido
{
/** Reamostra (passa-baixas simples + interpolação linear). Suficiente p/ fala. */
inline std::vector<float> resampleTo (const std::vector<float>& in, double fromRate, double toRate)
{
    if (in.empty() || fromRate <= 0) return {};
    std::vector<float> filtered (in);
    if (toRate < fromRate)
    {   // média móvel ~ anti-aliasing leve antes de decimar
        const int k = std::max (1, (int) std::floor (fromRate / toRate));
        double acc = 0;
        for (size_t i = 0; i < in.size(); ++i)
        {
            acc += in[i]; if (i >= (size_t) k) acc -= in[i - (size_t) k];
            filtered[i] = (float) (acc / std::min<size_t> (i + 1, (size_t) k));
        }
    }
    const double ratio = fromRate / toRate;
    const size_t outN = (size_t) std::floor ((double) in.size() / ratio);
    std::vector<float> out (outN);
    for (size_t i = 0; i < outN; ++i)
    {
        const double pos = (double) i * ratio; const size_t a = (size_t) pos; const double f = pos - (double) a;
        const float x0 = filtered[std::min (a, filtered.size() - 1)], x1 = filtered[std::min (a + 1, filtered.size() - 1)];
        out[i] = (float) (x0 + (x1 - x0) * f);
    }
    return out;
}

/** Normaliza o volume da fala (ajuda o reconhecimento quando o mic está baixo). */
inline void normalisePeak (std::vector<float>& x, float target = 0.7f)
{
    float pk = 0; for (float v : x) pk = std::max (pk, std::abs (v));
    if (pk > 1.0e-5f) { const float g = std::min (target / pk, 30.0f); for (float& v : x) v *= g; }
}

/** WAV mono PCM 16 bits em memória. */
inline std::vector<uint8_t> encodeWav16 (const std::vector<float>& x, int sampleRate)
{
    const uint32_t dataBytes = (uint32_t) x.size() * 2u;
    std::vector<uint8_t> out; out.reserve (44 + dataBytes);
    auto u32 = [&] (uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back ((uint8_t) ((v >> (8 * i)) & 0xFF)); };
    auto u16 = [&] (uint16_t v) { out.push_back ((uint8_t) (v & 0xFF)); out.push_back ((uint8_t) (v >> 8)); };
    auto tag = [&] (const char* s) { for (int i = 0; i < 4; ++i) out.push_back ((uint8_t) s[i]); };
    tag ("RIFF"); u32 (36 + dataBytes); tag ("WAVE");
    tag ("fmt "); u32 (16); u16 (1); u16 (1); u32 ((uint32_t) sampleRate); u32 ((uint32_t) sampleRate * 2u); u16 (2); u16 (16);
    tag ("data"); u32 (dataBytes);
    for (float v : x) { const int s = (int) std::lround (std::max (-1.0f, std::min (1.0f, v)) * 32767.0f); u16 ((uint16_t) (int16_t) s); }
    return out;
}
} // namespace cupido
