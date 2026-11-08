// =============================================================================
//  CUPIDO DSP — Timbre: EQ paramétrico, PRISMA (saturação multibanda paralela,
//  conceito de enhancer por bandas), AR (excitador dinâmico tipo "Dolby-A trick"),
//  Tilt EQ e Imager (largura M/S + grave mono)
// =============================================================================
#pragma once
#include "Core.h"

namespace cupido
{
// -----------------------------------------------------------------------------
//  EQ: HPF 24 dB/oct, low shelf, 2 peaks, high shelf, LPF 12 dB/oct
// -----------------------------------------------------------------------------
class Equalizer
{
public:
    struct Settings { float hpf, lowF, lowG, m1F, m1G, m1Q, m2F, m2G, m2Q, highF, highG, lpf; };

    void prepare (double sampleRate) { sr = sampleRate; last.hpf = -1; reset(); }
    void reset() { for (auto& ch : f) for (auto& b : ch) b.reset(); }

    void setParams (const Settings& s)
    {
        if (std::memcmp (&s, &last, sizeof (Settings)) == 0) return;
        last = s;
        BiquadCoefs c[kBands] = {
            BiquadCoefs::make (BqType::HighPass, sr, s.hpf, 0.5412),
            BiquadCoefs::make (BqType::HighPass, sr, s.hpf, 1.3066),
            BiquadCoefs::make (BqType::LowShelf, sr, s.lowF, 0.707, s.lowG),
            BiquadCoefs::make (BqType::Peak,     sr, s.m1F, s.m1Q, s.m1G),
            BiquadCoefs::make (BqType::Peak,     sr, s.m2F, s.m2Q, s.m2G),
            BiquadCoefs::make (BqType::HighShelf,sr, s.highF, 0.707, s.highG),
            BiquadCoefs::make (BqType::LowPass,  sr, std::min ((double) s.lpf, sr * 0.45), 0.707)
        };
        active[0] = active[1] = s.hpf > 21.0f;
        active[2] = std::abs (s.lowG) > 0.01f; active[3] = std::abs (s.m1G) > 0.01f;
        active[4] = std::abs (s.m2G) > 0.01f; active[5] = std::abs (s.highG) > 0.01f;
        active[6] = s.lpf < 21500.0f && s.lpf < sr * 0.45;
        for (int ch = 0; ch < 2; ++ch) for (int b = 0; b < kBands; ++b) f[ch][(size_t) b].set (c[b]);
    }

    void process (float* L, float* R, int n)
    {
        for (int b = 0; b < kBands; ++b)
        {
            if (! active[b]) continue;
            for (int i = 0; i < n; ++i) L[i] = f[0][(size_t) b].process (L[i]);
            if (R) for (int i = 0; i < n; ++i) R[i] = f[1][(size_t) b].process (R[i]);
        }
    }
private:
    static constexpr int kBands = 7;
    double sr = 48000; std::array<std::array<Biquad, kBands>, 2> f; bool active[kBands] {}; Settings last {};
};

// -----------------------------------------------------------------------------
//  PRISMA — cada banda extrai a diferença que um "boost" de EQ criaria,
//  satura SÓ essa diferença e soma em paralelo. Resultado: um "EQ que gera
//  harmônicos" — aumentar a banda 5 = saturação nos agudos.
// -----------------------------------------------------------------------------
class Prisma
{
public:
    static constexpr int kBands = 5;
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : dc) d.prepare (sr);
        for (auto& f : lastF) f = -1;
        for (auto& s : mixS) s.prepare (sr, 30.0f);
        reset();
    }
    void reset()
    {
        for (auto& ch : bp) for (auto& b : ch) b.reset();
        for (auto& d : dc) d.reset();
    }

    void setParams (int type, float drivePct, const float* freqs, const float* amtsDb, float mixPct, float outDb)
    {
        shape = (SatShape) type;
        drive = 1.0f + drivePct * 0.01f * 11.0f; // 1x .. 12x
        for (int b = 0; b < kBands; ++b)
        {
            if (freqs[b] != lastF[b])
            {
                lastF[b] = freqs[b];
                const double q = (b == 0 || b == kBands - 1) ? 0.6 : 0.9;
                auto c = BiquadCoefs::make (b == kBands - 1 ? BqType::HighPass : (b == 0 ? BqType::LowPass : BqType::BandPass), sr, freqs[b] * (b == kBands - 1 ? 0.8 : (b == 0 ? 1.25 : 1.0)), q);
                bp[0][(size_t) b].set (c); bp[1][(size_t) b].set (c);
            }
            bandGain[b] = dbToGain (amtsDb[b]) - 1.0f; // 0 dB -> 0 (banda desligada)
        }
        mixS[0].set (mixPct * 0.01f); outGain = dbToGain (outDb);
    }

    void process (float* L, float* R, int n)
    {
        bool any = false; for (float g : bandGain) any |= g > 1.0e-4f;
        for (int i = 0; i < n; ++i)
        {
            const float mix = mixS[0].next();
            for (int ch = 0; ch < (R ? 2 : 1); ++ch)
            {
                float* x = ch == 0 ? L : R; const float in = x[i];
                float wet = 0;
                if (any)
                    for (int b = 0; b < kBands; ++b)
                    {
                        const float band = bp[ch][(size_t) b].process (in);
                        if (bandGain[b] <= 1.0e-4f) continue;
                        const float d = band * bandGain[b];                    // "diferença do boost"
                        wet += saturate (shape, d * drive) / drive;            // satura só a diferença
                    }
                wet = dc[(size_t) ch].process (wet);
                x[i] = (in + wet * mix * 2.0f) * outGain;   // mix 50% = ganho nominal da banda
            }
        }
    }
private:
    double sr = 48000; SatShape shape = SatShape::Tube; float drive = 1, outGain = 1;
    std::array<std::array<Biquad, kBands>, 2> bp; std::array<DcBlocker, 2> dc;
    float bandGain[kBands] {}; float lastF[kBands] {}; Smoother mixS[1];
};

// -----------------------------------------------------------------------------
//  AR — excitador dinâmico: duas bandas passa-altas sobrepostas (Mid ~3 kHz,
//  High ~10 kHz), cada uma comprimida com threshold baixo (eleva detalhe
//  baixinho, quase não mexe no que já é brilhante) + leve geração harmônica,
//  somadas em paralelo. Nunca fica "ardido" porque o brilho alto é segurado.
// -----------------------------------------------------------------------------
class AirExciter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (int ch = 0; ch < 2; ++ch)
        {
            midHp[ch][0].set (BqType::HighPass, sr, 2800, 0.6); midHp[ch][1].set (BqType::HighPass, sr, 2800, 0.6);
            hiHp[ch][0].set (BqType::HighPass, sr, 9500, 0.6);  hiHp[ch][1].set (BqType::HighPass, sr, 9500, 0.6);
            midTop[ch].set (BqType::LowPass, sr, std::min (16000.0, sr * 0.45), 0.6);
        }
        atk = msToCoef (1.0f, sr); rel = msToCoef (60.0f, sr); reset();
    }
    void reset() { for (auto& c : midHp) for (auto& b : c) b.reset(); for (auto& c : hiHp) for (auto& b : c) b.reset(); for (auto& b : midTop) b.reset(); envM = envH = 0; }
    void setParams (float midPct, float highPct, float trimDb)
    { midAmt = midPct * 0.01f; hiAmt = highPct * 0.01f; trim = dbToGain (trimDb); }

    void process (float* L, float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R ? R[i] : l;
            float bm[2], bh[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                const float in = ch == 0 ? l : r;
                bm[ch] = midTop[ch].process (midHp[ch][1].process (midHp[ch][0].process (in)));
                bh[ch] = hiHp[ch][1].process (hiHp[ch][0].process (in));
            }
            const float am = std::max (std::abs (bm[0]), std::abs (bm[1])), ah = std::max (std::abs (bh[0]), std::abs (bh[1]));
            envM = am > envM ? am + (envM - am) * atk : am + (envM - am) * rel;
            envH = ah > envH ? ah + (envH - ah) * atk : ah + (envH - ah) * rel;
            // compressão "para cima" no paralelo: thr -40 dB, ratio 4:1
            const float gm = dbToGain (-compGain (gainToDb (envM))), gh = dbToGain (-compGain (gainToDb (envH)));
            for (int ch = 0; ch < (R ? 2 : 1); ++ch)
            {
                const float in = ch == 0 ? l : r;
                const float pm = bm[ch] * gm, ph = bh[ch] * gh;
                const float exc = std::tanh (ph * 3.0f) / 3.0f;            // toque harmônico no topo
                const float out = in + midAmt * 2.2f * pm + hiAmt * (2.8f * ph + 0.8f * exc);
                (ch == 0 ? L : R)[i] = out * trim;
            }
        }
    }
private:
    static float compGain (float db) { const float thr = -40.0f; return db > thr ? (db - thr) * 0.75f : 0.0f; }
    double sr = 48000; Biquad midHp[2][2], hiHp[2][2], midTop[2];
    float atk = 0, rel = 0, envM = 0, envH = 0, midAmt = 0, hiAmt = 0, trim = 1;
};

// -----------------------------------------------------------------------------
//  Tilt EQ (gira o espectro em torno de ~1 kHz)
// -----------------------------------------------------------------------------
class Tilt
{
public:
    void prepare (double sampleRate) { sr = sampleRate; last = 99; }
    void setParams (float db)
    {
        if (db == last) return;
        last = db;
        for (int ch = 0; ch < 2; ++ch) { lo[ch].set (BqType::LowShelf, sr, 900, 0.5, -db * 0.5); hi[ch].set (BqType::HighShelf, sr, 1100, 0.5, db * 0.5); }
        active = std::abs (db) > 0.01f;
    }
    void process (float* L, float* R, int n)
    {
        if (! active) return;
        for (int i = 0; i < n; ++i) { L[i] = hi[0].process (lo[0].process (L[i])); if (R) R[i] = hi[1].process (lo[1].process (R[i])); }
    }
private:
    double sr = 48000; float last = 99; bool active = false; Biquad lo[2], hi[2];
};

// -----------------------------------------------------------------------------
//  Imager: largura M/S + grave mono (side passa-altas em 120 Hz)
// -----------------------------------------------------------------------------
class Imager
{
public:
    void prepare (double sampleRate) { sr = sampleRate; sideHp[0].set (BqType::HighPass, sr, 120, 0.707); sideHp[1].set (BqType::HighPass, sr, 120, 0.707); ws.prepare (sr, 40); ws.reset (1); }
    void setParams (float widthPct, bool monoBass_) { ws.set (widthPct * 0.01f); monoBass = monoBass_; }
    void process (float* L, float* R, int n)
    {
        if (! R) return;
        for (int i = 0; i < n; ++i)
        {
            const float w = ws.next();
            float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]);
            if (monoBass) s = sideHp[1].process (sideHp[0].process (s));
            s *= w; L[i] = m + s; R[i] = m - s;
        }
    }
private:
    double sr = 48000; Biquad sideHp[2]; Smoother ws; bool monoBass = false;
};
} // namespace cupido
