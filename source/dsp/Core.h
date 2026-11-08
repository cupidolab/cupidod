// =============================================================================
//  CUPIDO DSP — utilidades básicas (sem dependência do JUCE, testável com g++)
// =============================================================================
#pragma once
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <complex>
#include <cstring>
#include <cstdint>
#include <atomic>

namespace cupido
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float dbToGain (float db)   { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)    { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clampf (float x, float a, float b) { return std::min (std::max (x, a), b); }
inline float lerp (float a, float b, float t)   { return a + (b - a) * t; }
inline float msToCoef (float ms, double sr)     { return ms <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (ms * 0.001 * sr)); }
inline float flushDenormal (float x)            { return std::abs (x) < 1.0e-15f ? 0.0f : x; }

/** Suavizador exponencial de parâmetros (evita zipper noise). */
struct Smoother
{
    float cur = 0, target = 0, coef = 0;
    void prepare (double sr, float ms) { coef = msToCoef (ms, sr); }
    void reset (float v)               { cur = target = v; }
    void set (float t)                 { target = t; }
    float next()                       { cur = target + (cur - target) * coef; return cur; }
    bool  settled() const              { return std::abs (cur - target) < 1.0e-5f; }
};

/** Gerador pseudo-aleatório rápido (xorshift32). */
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 0x9E3779B9u) : s (seed ? seed : 1u) {}
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xFFFFFF) / 8388608.0f - 1.0f; } // [-1,1)
};

/** Ruído suavizado (LFO aleatório) — para modulação orgânica. */
struct SmoothRandom
{
    Rng rng; float value = 0, target = 0, coef = 0.999f; int counter = 0, period = 4800;
    void prepare (double sr, float rateHz, uint32_t seed)
    {
        rng = Rng (seed); period = std::max (1, (int) (sr / std::max (0.05f, rateHz)));
        coef = msToCoef (1000.0f / std::max (0.05f, rateHz) * 0.5f, sr);
    }
    float next()
    {
        if (--counter <= 0) { counter = period; target = rng.next(); }
        value = target + (value - target) * coef; return value;
    }
};

// -----------------------------------------------------------------------------
//  Biquad (RBJ Audio EQ Cookbook) — forma direta transposta II, 1 canal
// -----------------------------------------------------------------------------
enum class BqType { LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf, Notch, AllPass };

struct BiquadCoefs
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    static BiquadCoefs make (BqType t, double sr, double f, double q, double gainDb = 0)
    {
        BiquadCoefs c;
        f = std::min (std::max (f, 5.0), sr * 0.49);
        q = std::max (q, 0.05);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * 3.14159265358979323846 * f / sr;
        const double cw = std::cos (w), sw = std::sin (w);
        const double alpha = sw / (2.0 * q);
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
        switch (t)
        {
            case BqType::LowPass:  b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case BqType::HighPass: b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case BqType::BandPass: b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break; // 0 dB no pico
            case BqType::Notch:    b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case BqType::AllPass:  b0 = 1 - alpha; b1 = -2 * cw; b2 = 1 + alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case BqType::Peak:     b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
            case BqType::LowShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) - (A - 1) * cw + sq); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - sq);
                a0 = (A + 1) + (A - 1) * cw + sq;       a1 = -2 * ((A - 1) + (A + 1) * cw);    a2 = (A + 1) + (A - 1) * cw - sq; break;
            }
            case BqType::HighShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) + (A - 1) * cw + sq); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - sq);
                a0 = (A + 1) - (A - 1) * cw + sq;       a1 = 2 * ((A - 1) - (A + 1) * cw);      a2 = (A + 1) - (A - 1) * cw - sq; break;
            }
        }
        c.b0 = (float) (b0 / a0); c.b1 = (float) (b1 / a0); c.b2 = (float) (b2 / a0);
        c.a1 = (float) (a1 / a0); c.a2 = (float) (a2 / a0);
        return c;
    }

    /** Magnitude (dB) da resposta na frequência f — usado para desenhar curvas na UI/testes. */
    double magnitudeDb (double f, double sr) const
    {
        const double w = 2.0 * 3.14159265358979323846 * f / sr;
        std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2 * w);
        auto num = (double) b0 + (double) b1 * z1 + (double) b2 * z2;
        auto den = 1.0 + (double) a1 * z1 + (double) a2 * z2;
        return 20.0 * std::log10 (std::max (1e-12, std::abs (num / den)));
    }
};

struct Biquad
{
    BiquadCoefs c; float z1 = 0, z2 = 0;
    void set (const BiquadCoefs& nc) { c = nc; }
    void set (BqType t, double sr, double f, double q, double g = 0) { c = BiquadCoefs::make (t, sr, f, q, g); }
    void reset() { z1 = z2 = 0; }
    inline float process (float x)
    {
        const float y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        z1 = flushDenormal (z1); z2 = flushDenormal (z2);
        return y;
    }
};

/** Filtro de 1 polo (LP), útil para amortecimento e suavização. */
struct OnePole
{
    float a = 0, z = 0;
    void setCutoff (double sr, double f) { a = (float) std::exp (-2.0 * 3.14159265358979323846 * std::min (f, sr * 0.49) / sr); }
    void reset() { z = 0; }
    inline float lp (float x) { z = x + (z - x) * a; z = flushDenormal (z); return z; }
    inline float hp (float x) { return x - lp (x); }
};

/** Remove DC (passa-altas de 1 polo ~10 Hz). */
struct DcBlocker
{
    float x1 = 0, y1 = 0, r = 0.9995f;
    void prepare (double sr) { r = (float) (1.0 - (2.0 * 3.14159265358979323846 * 10.0 / sr)); }
    void reset() { x1 = y1 = 0; }
    inline float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = flushDenormal (y); return y; }
};

// -----------------------------------------------------------------------------
//  Linha de atraso com leitura fracionária (Hermite cúbica)
// -----------------------------------------------------------------------------
struct DelayLine
{
    std::vector<float> buf; int mask = 0, w = 0;
    void prepare (int maxSamples)
    {
        int n = 1; while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    int  size() const { return mask + 1; }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    /** delay em amostras (>= 1). 1 = a amostra mais recente. */
    inline float read (float d) const
    {
        d = clampf (d, 1.0f, (float) (mask - 3));
        const float pos = (float) w - d;
        int i = (int) std::floor (pos); const float f = pos - (float) i;
        const float xm1 = buf[(size_t) ((i - 1) & mask)], x0 = buf[(size_t) (i & mask)];
        const float x1  = buf[(size_t) ((i + 1) & mask)], x2 = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    inline float readInt (int d) const { return buf[(size_t) ((w - d) & mask)]; }
};

// -----------------------------------------------------------------------------
//  FFT radix-2 iterativa (complexa, in-place)
// -----------------------------------------------------------------------------
class FFT
{
public:
    void prepare (int order)
    {
        n = 1 << order; rev.resize ((size_t) n); tw.resize ((size_t) n / 2);
        for (int i = 0; i < n; ++i)
        {
            int r = 0; for (int b = 0; b < order; ++b) if (i & (1 << b)) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }
        for (int i = 0; i < n / 2; ++i) tw[(size_t) i] = std::polar (1.0f, -kTwoPi * (float) i / (float) n);
    }
    int size() const { return n; }

    void forward (std::complex<float>* d) const { transform (d, false); }
    void inverse (std::complex<float>* d) const
    {
        transform (d, true);
        const float s = 1.0f / (float) n; for (int i = 0; i < n; ++i) d[i] *= s;
    }

private:
    int n = 0; std::vector<int> rev; std::vector<std::complex<float>> tw;
    void transform (std::complex<float>* d, bool inv) const
    {
        for (int i = 0; i < n; ++i) if (i < rev[(size_t) i]) std::swap (d[i], d[rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    std::complex<float> w = tw[(size_t) (j * step)]; if (inv) w = std::conj (w);
                    const auto u = d[i + j], v = d[i + j + half] * w;
                    d[i + j] = u + v; d[i + j + half] = u - v;
                }
        }
    }
};

inline void makeHann (std::vector<float>& w, int n)
{
    w.resize ((size_t) n);
    for (int i = 0; i < n; ++i) w[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) n); // periódica
}

// -----------------------------------------------------------------------------
//  Curvas de saturação — todas com derivada ~1 em zero (ganho unitário p/ sinal baixo)
// -----------------------------------------------------------------------------
enum class SatShape { Tube = 0, Tape, Transistor, Warm, Digital, Fuzz };

inline float saturate (SatShape s, float x)
{
    switch (s)
    {
        case SatShape::Tube:        // assimétrica -> harmônicos pares (2º, 4º)
        {
            constexpr float bias = 0.28f; const float t0 = std::tanh (bias);
            const float d0 = 1.0f - t0 * t0;
            return (std::tanh (x + bias) - t0) / d0;
        }
        case SatShape::Tape:        // compressão suave, ímpares baixos
            return x / (1.0f + std::abs (x) * 0.85f) * (1.0f + 0.0f);
        case SatShape::Transistor:  // soft clip cúbico, mais "duro"
        {
            const float y = clampf (x * 0.6666f, -1.0f, 1.0f);
            return (y - y * y * y / 3.0f) * 1.5f;
        }
        case SatShape::Warm:        // arctan, macio e cheio
            return (2.0f / kPi) * std::atan (kPi * 0.5f * x);
        case SatShape::Digital:     // clipping duro
            return clampf (x, -1.0f, 1.0f);
        case SatShape::Fuzz:        // assimétrico agressivo + retificação
        {
            const float a = std::tanh (2.2f * x) / 2.2f * 1.6f;
            return a + 0.25f * (std::abs (std::tanh (x)) - 0.0f) * (x > 0 ? 1.0f : 0.6f) - 0.0f;
        }
    }
    return x;
}

/** Medidor de pico com decaimento (para os meters). */
struct PeakMeter
{
    float value = 0, decay = 0.9995f;
    void prepare (double sr) { decay = msToCoef (300.0f, sr); }
    inline void push (float x) { const float a = std::abs (x); value = a > value ? a : value * decay; }
};
} // namespace cupido
