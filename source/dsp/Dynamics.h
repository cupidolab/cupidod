// =============================================================================
//  CUPIDO DSP — Dinâmica: Gate, Compressor (Limpo/FET/VCA), Opto, De-Esser,
//  Glue (bus), Limiter com lookahead
// =============================================================================
#pragma once
#include "Core.h"

namespace cupido
{
/** Curva estática de compressão com joelho suave (dB in -> dB de redução, >= 0). */
inline float compGainReductionDb (float inDb, float thresh, float ratio, float knee)
{
    const float over = inDb - thresh;
    const float slope = 1.0f - 1.0f / std::max (1.0f, ratio);
    if (knee > 0.0f && 2.0f * std::abs (over) <= knee)
    {
        const float x = over + knee * 0.5f;
        return slope * x * x / (2.0f * knee);
    }
    return over > 0.0f ? slope * over : 0.0f;
}

// -----------------------------------------------------------------------------
//  Noise Gate / Expander com histerese e hold
// -----------------------------------------------------------------------------
class Gate
{
public:
    void prepare (double sampleRate) { sr = sampleRate; env = 0; gain = 1; hold = 0; envCoefR = msToCoef (40.0f, sr); }
    void reset() { env = 0; gain = 1; hold = 0; open = false; }

    void setParams (float threshDb, float rangeDb, float attackMs, float releaseMs)
    {
        thresh = threshDb; floorGain = dbToGain (-rangeDb);
        atk = msToCoef (attackMs, sr); rel = msToCoef (releaseMs, sr);
        holdSamples = (int) (0.03 * sr);
    }

    /** Processa estéreo in-place. Retorna redução média em dB (para o meter). */
    float process (float* L, float* R, int n)
    {
        float grSum = 0;
        for (int i = 0; i < n; ++i)
        {
            const float a = std::max (std::abs (L[i]), R ? std::abs (R[i]) : 0.0f);
            env = a > env ? a : env * envCoefR;
            const float envDb = gainToDb (env);
            if (! open && envDb > thresh)            { open = true;  hold = holdSamples; }
            else if (open && envDb < thresh - 4.0f)  { if (--hold <= 0) open = false; }
            else if (open)                            hold = holdSamples;
            const float target = open ? 1.0f : floorGain;
            const float c = target > gain ? atk : rel;
            gain = target + (gain - target) * c;
            L[i] *= gain; if (R) R[i] *= gain;
            grSum += gain;
        }
        return -gainToDb (grSum / (float) std::max (1, n));
    }

private:
    double sr = 48000; float env = 0, gain = 1, thresh = -50, floorGain = 0.1f, atk = 0, rel = 0, envCoefR = 0;
    int hold = 0, holdSamples = 1440; bool open = false;
};

// -----------------------------------------------------------------------------
//  Compressor feed-forward: Limpo / FET (estilo 1176) / VCA (estilo dbx/SSL)
// -----------------------------------------------------------------------------
class Compressor
{
public:
    enum Character { Clean = 0, FET = 1, VCA = 2 };

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { grDb = 0; rmsState = 0; fastRel = 0; }

    void setParams (float threshDb, float ratio_, float attackMs, float releaseMs, float kneeDb,
                    float makeupDb, int character, float mixPct)
    {
        thresh = threshDb; ratio = ratio_; knee = kneeDb; makeup = dbToGain (makeupDb);
        ch = (Character) character; mix = mixPct * 0.01f;
        atk = msToCoef (attackMs, sr); rel = msToCoef (releaseMs, sr);
        relFast = msToCoef (std::max (10.0f, releaseMs * 0.25f), sr);
        rmsCoef = msToCoef (ch == VCA ? 8.0f : 1.5f, sr);
    }

    float process (float* L, float* R, int n)
    {
        float maxGr = 0;
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R ? R[i] : l;
            // Detector: FET/Limpo = pico, VCA = RMS curto
            float det;
            if (ch == VCA) { rmsState = (l * l + r * r) * 0.5f + (rmsState - (l * l + r * r) * 0.5f) * rmsCoef; det = std::sqrt (rmsState) * 1.414f; }
            else           det = std::max (std::abs (l), std::abs (r));

            const float target = compGainReductionDb (gainToDb (det), thresh, ratio, knee);
            if (target > grDb) grDb = target + (grDb - target) * atk;
            else
            {
                // FET: release dependente do programa (transientes curtos recuperam mais rápido)
                const float c = (ch == FET) ? lerp (relFast, rel, clampf (grDb / 12.0f, 0.0f, 1.0f)) : rel;
                grDb = target + (grDb - target) * c;
            }
            const float g = dbToGain (-grDb) * makeup;
            float wl = l * g, wr = r * g;
            if (ch == FET)
            {   // leve coloração do FET: assimetria sutil proporcional à compressão
                const float drive = 0.1f * (0.08f + 0.02f * grDb);
                wl += drive * (std::tanh (wl * 2.0f) * 0.5f - wl * 0.5f + 0.15f * wl * std::abs (wl));
                wr += drive * (std::tanh (wr * 2.0f) * 0.5f - wr * 0.5f + 0.15f * wr * std::abs (wr));
            }
            L[i] = l + (wl - l) * mix;
            if (R) R[i] = r + (wr - r) * mix;
            maxGr = std::max (maxGr, grDb);
        }
        return maxGr;
    }

private:
    double sr = 48000; float thresh = -18, ratio = 4, knee = 6, makeup = 1, mix = 1;
    float atk = 0, rel = 0, relFast = 0, rmsCoef = 0, rmsState = 0, grDb = 0, fastRel = 0; Character ch = FET;
};

// -----------------------------------------------------------------------------
//  Opto (estilo LA-2A): ataque ~10ms, release de dois estágios com "memória"
// -----------------------------------------------------------------------------
class OptoCompressor
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate; atk = msToCoef (10.0f, sr); relFast = msToCoef (60.0f, sr); relSlow = msToCoef (1500.0f, sr);
        memCoefUp = msToCoef (800.0f, sr); memCoefDown = msToCoef (3000.0f, sr); reset();
        lowShelfL.set (BqType::LowShelf, sr, 120, 0.7, 0.6); lowShelfR.c = lowShelfL.c;
    }
    void reset() { grDb = 0; memory = 0; env = 0; lowShelfL.reset(); lowShelfR.reset(); }
    void setParams (float peakPct, float gainDb, float mixPct)
    {
        thresh = -6.0f - peakPct * 0.38f; gain = dbToGain (gainDb); mix = mixPct * 0.01f;
    }
    float process (float* L, float* R, int n)
    {
        float maxGr = 0;
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R ? R[i] : l;
            const float a = std::max (std::abs (l), std::abs (r));
            env = a > env ? a : env * 0.9995f;  // detector suave (célula óptica)
            const float target = compGainReductionDb (gainToDb (env), thresh, 3.5f, 10.0f);
            if (target > grDb) grDb = target + (grDb - target) * atk;
            else { const float c = lerp (relFast, relSlow, memory); grDb = target + (grDb - target) * c; }
            const float m = clampf (grDb / 10.0f, 0.0f, 1.0f);
            memory = m > memory ? m + (memory - m) * memCoefUp : m + (memory - m) * memCoefDown;
            const float g = dbToGain (-grDb) * gain;
            // toque valvulado: saturação mínima + corpo no grave
            float wl = std::tanh (lowShelfL.process (l * g) * 0.9f) / 0.9f;
            float wr = std::tanh (lowShelfR.process (r * g) * 0.9f) / 0.9f;
            L[i] = l + (wl - l) * mix; if (R) R[i] = r + (wr - r) * mix;
            maxGr = std::max (maxGr, grDb);
        }
        return maxGr;
    }
private:
    double sr = 48000; float atk = 0, relFast = 0, relSlow = 0, memCoefUp = 0, memCoefDown = 0;
    float grDb = 0, memory = 0, env = 0, thresh = -20, gain = 1, mix = 1; Biquad lowShelfL, lowShelfR;
};

// -----------------------------------------------------------------------------
//  De-Esser: sidechain passa-banda ajustável.
//   Split = EQ dinâmico (high-shelf cuja atenuação segue a redução) — totalmente
//           transparente quando não há "S"; Wide = reduz o sinal inteiro.
// -----------------------------------------------------------------------------
class DeEsser
{
public:
    void prepare (double sampleRate) { sr = sampleRate; atk = msToCoef (0.4f, sr); rel = msToCoef (70.0f, sr); reset(); }
    void reset() { for (auto* b : { &scL, &scR, &scL2, &scR2, &shL, &shR }) b->reset(); env = 0; grDb = 0; appliedDb = 0; counter = 0; }
    void setParams (float freq, float threshDb, float rangeDb, int mode_, bool listen_)
    {
        if (freq != lastFreq)
        {
            lastFreq = freq;
            auto bp = BiquadCoefs::make (BqType::BandPass, sr, freq, 1.2);
            auto hp2 = BiquadCoefs::make (BqType::HighPass, sr, freq * 0.75, 0.707);
            scL.set (bp); scR.set (bp); scL2.set (hp2); scR2.set (hp2); appliedDb = -1;
        }
        thresh = threshDb; range = rangeDb; mode = mode_; listen = listen_;
    }
    float process (float* L, float* R, int n)
    {
        float maxGr = 0;
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R ? R[i] : l;
            const float sl = scL.process (scL2.process (l)), sr_ = scR.process (scR2.process (r));
            const float a = std::max (std::abs (sl), std::abs (sr_));
            env = a > env ? a + (env - a) * atk : a + (env - a) * rel;
            const float target = std::min (range, compGainReductionDb (gainToDb (env), thresh, 6.0f, 4.0f));
            grDb = target > grDb ? target + (grDb - target) * atk : target + (grDb - target) * rel;
            if (listen) { L[i] = sl; if (R) R[i] = sr_; }
            else if (mode == 0)
            {
                if (++counter >= 16 || appliedDb < 0) { counter = 0; updateShelf(); }
                L[i] = shL.process (l); if (R) R[i] = shR.process (r);
            }
            else { const float g = dbToGain (-grDb); L[i] = l * g; if (R) R[i] = r * g; }
            maxGr = std::max (maxGr, grDb);
        }
        return maxGr;
    }
private:
    void updateShelf()
    {
        if (std::abs (grDb - appliedDb) < 0.05f) return;
        appliedDb = grDb;
        auto c = BiquadCoefs::make (BqType::HighShelf, sr, lastFreq * 0.8, 0.8, -grDb);
        shL.set (c); shR.set (c);
    }
    double sr = 48000; Biquad scL, scR, scL2, scR2, shL, shR;
    float atk = 0, rel = 0, env = 0, grDb = 0, appliedDb = -1, thresh = -28, range = 8, lastFreq = -1; int mode = 0, counter = 0; bool listen = false;
};

// -----------------------------------------------------------------------------
//  Glue (bus compressor estilo SSL): RMS, ataque 10ms, auto-makeup parcial
// -----------------------------------------------------------------------------
class GlueCompressor
{
public:
    void prepare (double sampleRate) { sr = sampleRate; atk = msToCoef (10.0f, sr); rmsCoef = msToCoef (15.0f, sr); grDb = 0; rms = 0; }
    void reset() { grDb = 0; rms = 0; }
    void setParams (float threshDb, float ratio_, float releaseMs)
    { thresh = threshDb; ratio = ratio_; rel = msToCoef (releaseMs, sr);
      makeup = dbToGain (0.35f * compGainReductionDb (-6.0f, thresh, ratio, 6.0f)); }
    float process (float* L, float* R, int n)
    {
        float maxGr = 0;
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R ? R[i] : l;
            const float p = (l * l + r * r) * 0.5f; rms = p + (rms - p) * rmsCoef;
            const float target = compGainReductionDb (gainToDb (std::sqrt (rms) * 1.414f), thresh, ratio, 6.0f);
            grDb = target > grDb ? target + (grDb - target) * atk : target + (grDb - target) * rel;
            const float g = dbToGain (-grDb) * makeup;
            L[i] = l * g; if (R) R[i] = r * g; maxGr = std::max (maxGr, grDb);
        }
        return maxGr;
    }
private:
    double sr = 48000; float atk = 0, rel = 0, rmsCoef = 0, rms = 0, grDb = 0, thresh = -12, ratio = 2, makeup = 1;
};

// -----------------------------------------------------------------------------
//  Limiter brickwall com lookahead: filtro de mínimo + média móvel (garante
//  que o ganho já atingiu o necessário quando o pico chega) + clip de segurança
// -----------------------------------------------------------------------------
class Limiter
{
public:
    void prepare (double sampleRate, int lookaheadSamples)
    {
        sr = sampleRate; L = std::max (1, lookaheadSamples);
        dl.prepare (L + 4); dr.prepare (L + 4);
        minBuf.assign ((size_t) L, 1.0f); avgBuf.assign ((size_t) L, 1.0f);
        reset();
    }
    int  latency() const { return L - 1; }
    void reset()
    {
        dl.reset(); dr.reset(); std::fill (minBuf.begin(), minBuf.end(), 1.0f); std::fill (avgBuf.begin(), avgBuf.end(), 1.0f);
        dq.clear(); idx = 0; t = 0; avgSum = (double) L; relState = 1; grNow = 0;
    }
    void setParams (float inGainDb, float ceilingDb, float releaseMs)
    { inGain = dbToGain (inGainDb); ceiling = dbToGain (ceilingDb); relCoef = msToCoef (releaseMs, sr); }

    float process (float* Lc, float* Rc, int n)
    {
        float maxGr = 0;
        for (int i = 0; i < n; ++i)
        {
            const float l = Lc[i] * inGain, r = Rc ? Rc[i] * inGain : l;
            const float pk = std::max (std::abs (l), std::abs (r));
            const float req = pk > ceiling ? ceiling / pk : 1.0f;
            // mínimo deslizante (deque monotônico) sobre janela L
            while (! dq.empty() && dq.back().second >= req) dq.pop_back();
            dq.push_back ({ t, req });
            while (dq.front().first <= t - L) dq.pop_front();
            const float m = dq.front().second;
            // release suave (sempre <= m)
            relState = m < relState ? m : m + (relState - m) * relCoef;
            // média móvel de L amostras
            avgSum += (double) relState - (double) avgBuf[(size_t) idx];
            avgBuf[(size_t) idx] = relState; idx = (idx + 1) % L;
            const float g = (float) (avgSum / (double) L);
            dl.push (l); dr.push (r);
            float ol = dl.readInt (L) * g, orr = dr.readInt (L) * g;
            ol = clampf (ol, -ceiling, ceiling); orr = clampf (orr, -ceiling, ceiling);
            Lc[i] = ol; if (Rc) Rc[i] = orr;
            ++t; maxGr = std::max (maxGr, -gainToDb (g));
        }
        grNow = maxGr; return maxGr;
    }
private:
    struct Deque
    {   // deque de capacidade fixa (sem alocação no áudio)
        std::vector<std::pair<int64_t, float>> d; size_t head = 0, count = 0;
        void ensure (size_t cap) { if (d.size() < cap) d.resize (cap); }
        bool empty() const { return count == 0; }
        void clear() { head = count = 0; }
        std::pair<int64_t, float>& front() { return d[head]; }
        std::pair<int64_t, float>& back()  { return d[(head + count - 1) % d.size()]; }
        void push_back (std::pair<int64_t, float> v) { d[(head + count) % d.size()] = v; ++count; }
        void pop_back()  { --count; }
        void pop_front() { head = (head + 1) % d.size(); --count; }
    };
    double sr = 48000; int L = 64; DelayLine dl, dr; std::vector<float> minBuf, avgBuf;
    Deque dq; int idx = 0; int64_t t = 0; double avgSum = 64; float relState = 1, relCoef = 0.999f;
    float inGain = 1, ceiling = 0.89f, grNow = 0;
public:
    Limiter() { dq.ensure (1 << 15); }
};
} // namespace cupido
