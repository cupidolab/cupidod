// =============================================================================
//  CUPIDO DSP — AFINA (v2)
//
//  • Detector: YIN em sinal decimado + proteção contra erro de oitava
//    (continuidade com a nota anterior + mediana das últimas estimativas).
//  • Shifter pitch-síncrono: a cabeça de leitura anda na velocidade da
//    correção e, quando precisa "pular", pula EXATAMENTE um período da voz,
//    com crossfade curto. Como o pulo cai na mesma fase da onda, não aparece
//    a voz-fantasma/oitava abaixo que o shifter granular comum cria.
//    Latência: ~4 ms + no máximo 1 período (≈ 2–8 ms numa voz típica).
// =============================================================================
#pragma once
#include "Core.h"

namespace cupido
{
// -----------------------------------------------------------------------------
//  Detector de pitch (YIN, de Cheveigné & Kawahara 2002) + anti-oitava
// -----------------------------------------------------------------------------
class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate; dec = sr > 64000 ? 4 : 2; dsr = sr / dec;
        W = 1 << 9; while ((double) W / dsr < 0.032) W <<= 1;       // janela >= 32 ms
        tauMax = std::min (W / 2, (int) (dsr / 70.0));               // até ~70 Hz
        tauMin = std::max (2, (int) (dsr / 1050.0));                 // até ~1050 Hz
        ring.assign ((size_t) W * 2, 0.0f); d.assign ((size_t) tauMax + 2, 0.0f);
        aa1.set (BqType::LowPass, sr, dsr * 0.42, 0.54); aa2.set (BqType::LowPass, sr, dsr * 0.42, 1.31);
        hp.set (BqType::HighPass, sr, 60.0, 0.707);
        hopDec = std::max (32, (int) (dsr * 0.004)); reset();
    }
    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f); wp = 0; decCount = 0; hopCount = 0;
        freq = 0; conf = 0; prevF = 0; histN = 0; aa1.reset(); aa2.reset(); hp.reset();
    }

    bool push (float x)
    {
        const float y = aa2.process (aa1.process (hp.process (x)));
        if (++decCount < dec) return false;
        decCount = 0;
        ring[(size_t) wp] = y; ring[(size_t) (wp + W)] = y; wp = (wp + 1) % W;
        if (++hopCount < hopDec) return false;
        hopCount = 0; analyse(); return true;
    }
    float frequency() const  { return freq; }
    float confidence() const { return conf; }
    float periodSamples() const { return freq > 0 ? (float) (sr / freq) : 0.0f; }   // em amostras da taxa cheia

private:
    float cmnd (int tau) const { return d[(size_t) tau]; }
    float refine (int best) const
    {
        float t = (float) best;
        if (best > 1 && best < tauMax - 1)
        {
            const float a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
            const float den = a - 2 * b + c; if (std::abs (den) > 1e-9f) t += 0.5f * (a - c) / den;
        }
        return t;
    }
    int localMinNear (int tau) const
    {   // procura o mínimo local em ±2 amostras
        int b = std::min (std::max (tau, tauMin), tauMax - 1);
        for (int k = std::max (tauMin, tau - 2); k <= std::min (tauMax - 1, tau + 2); ++k) if (d[(size_t) k] < d[(size_t) b]) b = k;
        return b;
    }

    void analyse()
    {
        const float* x = ring.data() + wp;
        double e = 0; for (int i = 0; i < W; ++i) e += (double) x[i] * x[i];
        const float lvlDb = 10.0f * (float) std::log10 (e / W + 1e-12);
        if (lvlDb < -52.0f) { unvoiced(); return; }

        const int N = W - tauMax;
        d[0] = 1; double run = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0;
            for (int j = 0; j < N; ++j) { const float df = x[j] - x[j + tau]; s += (double) df * df; }
            run += s; d[(size_t) tau] = (float) (run > 0 ? s * tau / run : 1.0);
        }
        // 1º mergulho abaixo do limiar (padrão YIN)
        constexpr float thr = 0.12f; int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (d[(size_t) tau] < thr) { while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau; best = tau; break; }
        if (best < 0)
        {
            float mn = 1e9f; for (int tau = tauMin; tau < tauMax; ++tau) if (d[(size_t) tau] < mn) { mn = d[(size_t) tau]; best = tau; }
            if (mn > 0.3f) { unvoiced(); return; }
        }
        // ---- anti-oitava: compara com tau/2 e tau*2 e com a nota anterior ----
        int cand[3] = { best, localMinNear (best / 2), best * 2 <= tauMax - 1 ? localMinNear (best * 2) : best };
        // se tau/2 também é um bom mínimo, a fundamental real é a mais aguda (erro clássico de oitava abaixo)
        if (cand[1] >= tauMin && d[(size_t) cand[1]] < d[(size_t) best] + 0.06f && d[(size_t) cand[1]] < 0.25f) best = cand[1];
        if (prevF > 0)
        {
            float bestScore = 1e9f; int pick = best;
            for (int c : cand)
            {
                if (c < tauMin || c >= tauMax) continue;
                const float f = (float) (dsr / refine (c));
                const float jump = std::abs (12.0f * std::log2 (f / prevF));       // semitons de salto
                const float score = d[(size_t) c] * 4.0f + jump * 0.08f;
                if (score < bestScore) { bestScore = score; pick = c; }
            }
            best = pick;
        }
        const float f = (float) (dsr / refine (best));
        conf = 1.0f - clampf (d[(size_t) best], 0.0f, 1.0f);
        // mediana das últimas 5 estimativas (em semitons) — mata picos isolados
        hist[histN % 5] = 12.0f * std::log2 (f / 440.0f); ++histN;
        const int n = std::min (histN, 5); std::array<float, 5> tmp {}; for (int k = 0; k < n; ++k) tmp[(size_t) k] = hist[k]; for (int a = 1; a < n; ++a) for (int b = a; b > 0 && tmp[(size_t) b - 1] > tmp[(size_t) b]; --b) std::swap (tmp[(size_t) b - 1], tmp[(size_t) b]);
        freq = 440.0f * std::pow (2.0f, tmp[(size_t) (n / 2)] / 12.0f);
        prevF = freq; silentHops = 0;
    }
    void unvoiced() { freq = 0; conf = 0; histN = 0; if (++silentHops > 25) prevF = 0; }

    double sr = 48000, dsr = 24000; int dec = 2, W = 1024, tauMax = 340, tauMin = 20, hopDec = 96;
    std::vector<float> ring, d; int wp = 0, decCount = 0, hopCount = 0, histN = 0, silentHops = 0;
    float freq = 0, conf = 0, prevF = 0, hist[5] {}; Biquad aa1, aa2, hp;
};

// -----------------------------------------------------------------------------
//  Shifter pitch-síncrono (uma cabeça + crossfade nos pulos de 1 período)
// -----------------------------------------------------------------------------
class SyncShifter
{
public:
    void prepare (double sampleRate, float minDelayMs)
    {
        sr = sampleRate; minD = (float) (minDelayMs * 0.001 * sr);
        buf.prepare ((int) (0.06 * sr) + 64); reset();
    }
    void reset() { buf.reset(); d = minD + 8.0f; xf = 0; }
    float baseLatency() const { return minD; }

    /** ratio = fator de pitch; period = período atual em amostras (0 = desconhecido). */
    inline float process (float x, float ratio, float period)
    {
        buf.push (x);
        const float P = period > 16.0f ? std::min (period, (float) (0.025 * sr)) : (float) (0.008 * sr);
        d += 1.0f - ratio;
        if (xf > 0) dOld += 1.0f - ratio;
        if (xf <= 0)
        {
            const float xlen = clampf (0.5f * P, 24.0f, (float) (0.004 * sr));
            if (d < minD)               { dOld = d; d += P; xf = xlen; xfLen = xlen; }     // subindo: volta 1 período
            else if (d > minD + P + 4)  { dOld = d; d -= P; xf = xlen; xfLen = xlen; }     // descendo: avança 1 período
        }
        float y = buf.read (std::max (2.0f, d));
        if (xf > 0)
        {
            const float t = 1.0f - xf / xfLen;                      // 0 -> 1
            const float gIn = std::sin (t * kPi * 0.5f), gOut = std::cos (t * kPi * 0.5f);
            y = y * gIn + buf.read (std::max (2.0f, dOld)) * gOut;
            xf -= 1.0f;
        }
        return y;
    }
private:
    double sr = 48000; DelayLine buf; float minD = 190, d = 200, dOld = 200, xf = 0, xfLen = 1;
};

// Mantido para o doubler (pequenos detunes soam bem com granular de janela longa)
class GranularShifter
{
public:
    void prepare (double sampleRate, float windowMs, float minDelayMs = 1.0f)
    {
        sr = sampleRate; win = std::max (64.0f, (float) (windowMs * 0.001 * sr));
        minD = (float) (minDelayMs * 0.001 * sr) + 2.0f; dl.prepare ((int) (win + minD + 16)); reset();
    }
    void reset() { dl.reset(); phase = 0; }
    float latency() const { return minD + win * 0.5f; }
    inline float process (float x, float ratio)
    {
        dl.push (x);
        phase += (1.0f - ratio) / win; phase -= std::floor (phase);
        const float p2 = phase + 0.5f - std::floor (phase + 0.5f);
        const float g1 = std::sin (kPi * phase), g2 = std::sin (kPi * p2);
        return dl.read (minD + phase * win) * g1 * g1 + dl.read (minD + p2 * win) * g2 * g2;
    }
    float win = 1200, minD = 50;
private:
    double sr = 48000; DelayLine dl; float phase = 0;
};

// -----------------------------------------------------------------------------
//  Corretor de afinação
// -----------------------------------------------------------------------------
class PitchCorrector
{
public:
    void prepare (double sampleRate, bool lowLatency)
    {
        sr = sampleRate; det.prepare (sr);
        const float minMs = lowLatency ? 2.5f : 4.0f;
        for (auto& s : sh) s.prepare (sr, minMs);
        lat = (int) std::lround (minMs * 0.001 * sr + 0.003 * sr);   // base + meio período típico
        for (auto& c : compDelay) c.prepare (lat + 8);
        corr.prepare (sr, 20.0f); corr.reset (0); periodS.prepare (sr, 8.0f); periodS.reset (0);
        slowPitch = 0; currentNote = -1; std::fill (std::begin (histogram), std::end (histogram), 0.0f); reset();
    }
    void reset() { for (auto& s : sh) s.reset(); det.reset(); for (auto& c : compDelay) c.reset(); }
    int latency() const { return lat; }

    void setParams (int key, int scale, float speedMs, float humanizePct, float amountPct, float transposeSt)
    {
        buildScale (key, scale);
        humanize = humanizePct * 0.01f; amount = amountPct * 0.01f; transpose = transposeSt;
        corr.prepare (sr, std::max (0.3f, speedMs));
    }

    void process (float* L, float* R, int n, bool enabled)
    {
        for (int i = 0; i < n; ++i)
        {
            const float mono = R ? 0.5f * (L[i] + R[i]) : L[i];
            if (det.push (mono)) updateTarget();
            const float semis = corr.next();
            const float ratio = std::pow (2.0f, semis / 12.0f);
            const float P = periodS.next();
            if (enabled)
            {
                L[i] = sh[0].process (L[i], ratio, P);
                if (R) R[i] = sh[1].process (R[i], ratio, P);
            }
            else
            {   // mantém a mesma latência quando desligado (alinhamento com o DAW)
                compDelay[0].push (L[i]); L[i] = compDelay[0].readInt (lat);
                if (R) { compDelay[1].push (R[i]); R[i] = compDelay[1].readInt (lat); }
            }
        }
    }

    float detectedHz() const   { return lastHz; }
    float detectedMidi() const { return lastMidi; }
    float targetMidi() const   { return lastTarget; }
    float correctionSt() const { return corr.cur; }
    const float* pitchClassHistogram() const { return histogram; }

private:
    void buildScale (int key, int scale)
    {
        static const int scales[8][12] = {
            {1,1,1,1,1,1,1,1,1,1,1,1}, {1,0,1,0,1,1,0,1,0,1,0,1}, {1,0,1,1,0,1,0,1,1,0,1,0}, {1,0,1,1,0,1,0,1,1,0,0,1},
            {1,0,0,1,0,1,0,1,0,0,1,0}, {1,0,1,0,1,0,0,1,0,1,0,0}, {1,0,1,1,0,1,0,1,0,1,1,0}, {1,1,0,1,0,1,0,1,1,0,1,0} };
        scale = std::min (std::max (scale, 0), 7);
        for (int pc = 0; pc < 12; ++pc) inScale[pc] = scales[scale][(pc - key + 12) % 12] != 0;
    }
    float nearestInScale (float midi) const
    {
        const int base = (int) std::floor (midi); float best = midi, bestDist = 1e9f;
        for (int k = base - 6; k <= base + 7; ++k)
            if (inScale[((k % 12) + 12) % 12]) { const float dd = std::abs ((float) k - midi); if (dd < bestDist) { bestDist = dd; best = (float) k; } }
        return best;
    }
    void updateTarget()
    {
        const float f = det.frequency();
        if (f <= 0.0f || det.confidence() < 0.55f)
        {
            corr.set (transpose); lastHz = 0; currentNote = -1; return;   // surdo: só transposição
        }
        periodS.set (det.periodSamples());
        if (periodS.cur <= 0) periodS.reset (det.periodSamples());
        const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
        lastHz = f; lastMidi = midi;
        if (slowPitch == 0) slowPitch = midi;
        slowPitch += (midi - slowPitch) * 0.06f;
        float target = nearestInScale (midi);
        if (currentNote >= 0 && (float) currentNote != target && std::abs ((float) currentNote - midi) < 1.2f)
        {
            const float mid = 0.5f * ((float) currentNote + target);
            const bool crossed = target > (float) currentNote ? midi > mid + 0.12f : midi < mid - 0.12f;
            if (! crossed && inScale[((currentNote % 12) + 12) % 12]) target = (float) currentNote;
        }
        currentNote = (int) target; lastTarget = target;
        histogram[((currentNote % 12) + 12) % 12] += det.confidence();
        const float expression = (midi - slowPitch) * humanize;
        float correction = (target - midi + expression) * amount;
        correction = clampf (correction, -1.0f, 1.0f);   // nunca corrige mais que 1 semitom (evita saltos de oitava)
        corr.set (correction + transpose);
    }

    double sr = 48000; int lat = 300; PitchDetector det; SyncShifter sh[2]; DelayLine compDelay[2]; Smoother corr, periodS;
    bool inScale[12] {}; float humanize = 0, amount = 1, transpose = 0;
    float slowPitch = 0, lastHz = 0, lastMidi = 0, lastTarget = 0; int currentNote = -1; float histogram[12] {};
};
} // namespace cupido
