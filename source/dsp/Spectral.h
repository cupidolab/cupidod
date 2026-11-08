// =============================================================================
//  CUPIDO DSP — LIMPA: redução de ruído espectral (STFT 75% overlap) com perfil
//  aprendido ("Aprender ruído") e/ou rastreamento adaptativo do piso de ruído;
//  ganho Wiener com suavização tempo/frequência contra "ruído musical".
// =============================================================================
#pragma once
#include "Core.h"

namespace cupido
{
class NoiseReducer
{
public:
    static constexpr int kOrder = 9, kN = 1 << kOrder, kHop = kN / 4, kBins = kN / 2 + 1;

    void prepare (double sampleRate)
    {
        sr = sampleRate; fft.prepare (kOrder); makeHann (win, kN);
        for (auto& c : ch) { c.in.assign (kN, 0.0f); c.out.assign (kN, 0.0f); c.bypass.prepare (kN + 4); }
        buf.assign (kN, {}); spec0.assign (kN, {}); spec1.assign (kN, {});
        noise.assign (kBins, 1.0e-9f); learned.assign (kBins, 0.0f); smoothP.assign (kBins, 0.0f);
        gain.assign (kBins, 1.0f); tmpG.assign (kBins, 1.0f); reset();
    }
    void reset()
    {
        for (auto& c : ch) { std::fill (c.in.begin(), c.in.end(), 0.0f); std::fill (c.out.begin(), c.out.end(), 0.0f); c.bypass.reset(); }
        pos = 0; hopCount = 0; std::fill (gain.begin(), gain.end(), 1.0f);
    }
    int latency() const { return kN; }

    void setParams (float amountPct, float sensPct, bool adaptive_)
    {
        floorGain = dbToGain (-amountPct * 0.3f);    // até -30 dB
        over = 1.0f + sensPct * 0.035f;               // 1..4.5
        adaptive = adaptive_;
    }
    /** Começa a aprender o perfil por ~seconds (chamado da UI). */
    void startLearning (float seconds) { learnFrames.store ((int) (seconds * sr / kHop)); learnCount = 0; std::fill (learned.begin(), learned.end(), 0.0f); }
    bool isLearning() const { return learnFrames.load() > 0; }
    bool hasProfile() const { return profileReady; }

    void process (float* L, float* R, int n, bool enabled)
    {
        float* chans[2] = { L, R };
        const int numCh = R ? 2 : 1;
        if (! enabled)
        {   // latência constante: só atrasa
            for (int c = 0; c < numCh; ++c) for (int i = 0; i < n; ++i) { ch[c].bypass.push (chans[c][i]); chans[c][i] = ch[c].bypass.readInt (kN); }
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < numCh; ++c)
            {
                ch[c].in[(size_t) pos] = chans[c][i];
                chans[c][i] = ch[c].out[(size_t) pos];
                ch[c].out[(size_t) pos] = 0.0f;
            }
            pos = (pos + 1) % kN;
            if (++hopCount == kHop) { hopCount = 0; processFrame (numCh); }
        }
    }

private:
    struct Channel { std::vector<float> in, out; DelayLine bypass; };

    void processFrame (int numCh)
    {
        // 1) Espectro de potência médio (dos canais) para estimar ruído/ganho
        std::vector<float>& P = tmpG; std::fill (P.begin(), P.end(), 0.0f);
        for (int c = 0; c < numCh; ++c)
        {
            for (int i = 0; i < kN; ++i) buf[(size_t) i] = { ch[c].in[(size_t) ((pos + i) % kN)] * win[(size_t) i], 0.0f };
            fft.forward (buf.data());
            for (int k = 0; k < kBins; ++k) P[(size_t) k] += std::norm (buf[(size_t) k]) / (float) numCh;
            std::copy (buf.begin(), buf.end(), (c == 0 ? spec0 : spec1).begin());
        }
        // 2) Atualiza estimativas
        int lf = learnFrames.load();
        for (int k = 0; k < kBins; ++k)
        {
            const float p = P[(size_t) k];
            smoothP[(size_t) k] = 0.7f * smoothP[(size_t) k] + 0.3f * p;
            if (lf > 0) learned[(size_t) k] += p;
            if (adaptive || ! profileReady)
            {   // mínimo rastreado: desce rápido, sobe devagar (~+3 dB/s)
                float& nz = noise[(size_t) k];
                nz = smoothP[(size_t) k] < nz ? smoothP[(size_t) k] : nz * 1.0035f;
                if (profileReady) nz = std::max (nz, learned[(size_t) k] * 0.5f);
            }
        }
        if (lf > 0)
        {
            ++learnCount; learnFrames.store (lf - 1);
            if (lf - 1 == 0 && learnCount > 0)
            {
                for (int k = 0; k < kBins; ++k) { learned[(size_t) k] /= (float) learnCount; noise[(size_t) k] = learned[(size_t) k]; }
                profileReady = true;
            }
        }
        // 3) Ganho Wiener com piso + suavização temporal assimétrica + suavização em frequência
        for (int k = 0; k < kBins; ++k)
        {
            const float snrInv = noise[(size_t) k] * over / std::max (smoothP[(size_t) k], 1.0e-12f);
            const float g = std::max (floorGain, 1.0f - snrInv);
            float& gs = gain[(size_t) k];
            gs = g > gs ? 0.5f * gs + 0.5f * g : 0.85f * gs + 0.15f * g;   // abre rápido, fecha devagar
        }
        for (int k = 1; k < kBins - 1; ++k) P[(size_t) k] = 0.25f * gain[(size_t) k - 1] + 0.5f * gain[(size_t) k] + 0.25f * gain[(size_t) k + 1];
        P[0] = gain[0]; P[(size_t) kBins - 1] = gain[(size_t) kBins - 1];
        // 4) Aplica, IFFT e overlap-add (Hann² com 75% overlap soma 1.5)
        for (int c = 0; c < numCh; ++c)
        {
            auto& s = c == 0 ? spec0 : spec1;
            for (int k = 0; k < kBins; ++k) s[(size_t) k] *= P[(size_t) k];
            for (int k = kBins; k < kN; ++k) s[(size_t) k] = std::conj (s[(size_t) (kN - k)]);
            fft.inverse (s.data());
            constexpr float norm = 1.0f / 1.5f;
            for (int i = 0; i < kN; ++i) ch[c].out[(size_t) ((pos + i) % kN)] += s[(size_t) i].real() * win[(size_t) i] * norm;
        }
    }

    double sr = 48000; FFT fft; std::vector<float> win; Channel ch[2];
    std::vector<std::complex<float>> buf, spec0, spec1;
    std::vector<float> noise, learned, smoothP, gain, tmpG;
    int pos = 0, hopCount = 0, learnCount = 0; std::atomic<int> learnFrames { 0 }; bool profileReady = false, adaptive = true;
    float floorGain = 0.1f, over = 2.0f;
};
} // namespace cupido
