// =============================================================================
//  CUPIDO DSP — AFINA: detecção de pitch (YIN), pitch shifter granular de
//  2 cabeças com crossfade de potência constante, e o corretor (autotune)
//  com tom/escala, velocidade, humanizar e transposição.
// =============================================================================
#pragma once
#include "Core.h"

namespace cupido
{
// -----------------------------------------------------------------------------
//  YIN (de Cheveigné & Kawahara, 2002) sobre sinal decimado por 2
// -----------------------------------------------------------------------------
class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate; dec = sr > 64000 ? 4 : 2; dsr = sr / dec;
        W = 1 << 10; while ((double) W / dsr < 0.040) W <<= 1;      // janela >= 40 ms
        tauMax = std::min (W / 2, (int) (dsr / 65.0));               // até ~65 Hz
        tauMin = std::max (2, (int) (dsr / 1100.0));                 // até ~1100 Hz
        ring.assign ((size_t) W * 2, 0.0f); frame.assign ((size_t) W, 0.0f); d.assign ((size_t) tauMax + 2, 0.0f);
        aa1.set (BqType::LowPass, sr, dsr * 0.42, 0.54); aa2.set (BqType::LowPass, sr, dsr * 0.42, 1.31);
        hopDec = std::max (64, (int) (dsr * 0.0055)); reset();
    }
    void reset() { std::fill (ring.begin(), ring.end(), 0.0f); wp = 0; decCount = 0; hopCount = 0; freq = 0; conf = 0; aa1.reset(); aa2.reset(); }

    /** Alimenta uma amostra (mono). Retorna true quando uma nova estimativa saiu. */
    bool push (float x)
    {
        const float y = aa2.process (aa1.process (x));
        if (++decCount < dec) return false;
        decCount = 0;
        ring[(size_t) wp] = y; ring[(size_t) (wp + W)] = y; wp = (wp + 1) % W;
        if (++hopCount < hopDec) return false;
        hopCount = 0; analyse(); return true;
    }
    float frequency() const  { return freq; }   // 0 = sem pitch (surdo/silêncio)
    float confidence() const { return conf; }
    float levelDb() const    { return lvlDb; }

private:
    void analyse()
    {
        const float* x = ring.data() + wp;           // janela contígua, mais antiga primeiro
        double e = 0; for (int i = 0; i < W; ++i) e += (double) x[i] * x[i];
        lvlDb = 10.0f * (float) std::log10 (e / W + 1e-12);
        if (lvlDb < -55.0f) { freq = 0; conf = 0; return; }
        const int N = W - tauMax;
        d[0] = 1; double run = 0; int best = -1;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0;
            for (int j = 0; j < N; ++j) { const float df = x[j] - x[j + tau]; s += (double) df * df; }
            run += s;
            d[(size_t) tau] = (float) (run > 0 ? s * tau / run : 1.0);
        }
        constexpr float thr = 0.15f;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (d[(size_t) tau] < thr)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau; break;
            }
        if (best < 0)
        {   // sem mergulho abaixo do limiar: pega o mínimo global se razoável
            float mn = 1e9f; for (int tau = tauMin; tau < tauMax; ++tau) if (d[(size_t) tau] < mn) { mn = d[(size_t) tau]; best = tau; }
            if (mn > 0.35f) { freq = 0; conf = 0; return; }
        }
        float t = (float) best;
        if (best > 1 && best < tauMax - 1)
        {
            const float a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
            const float den = a - 2 * b + c; if (std::abs (den) > 1e-9f) t += 0.5f * (a - c) / den;
        }
        freq = (float) (dsr / t); conf = 1.0f - clampf (d[(size_t) best], 0.0f, 1.0f);
    }

    double sr = 48000, dsr = 24000; int dec = 2, W = 1024, tauMax = 360, tauMin = 20, hopDec = 128;
    std::vector<float> ring, frame, d; int wp = 0, decCount = 0, hopCount = 0;
    float freq = 0, conf = 0, lvlDb = -100; Biquad aa1, aa2;
};

// -----------------------------------------------------------------------------
//  Pitch shifter granular (linha de atraso + 2 cabeças defasadas 180°).
//  ganhos sin² + cos² = 1 -> crossfade de potência/amplitude constante.
// -----------------------------------------------------------------------------
class GranularShifter
{
public:
    void prepare (double sampleRate, float windowMs, float minDelayMs = 1.0f)
    {
        sr = sampleRate; win = std::max (64.0f, (float) (windowMs * 0.001 * sr));
        minD = (float) (minDelayMs * 0.001 * sr) + 2.0f;
        dl.prepare ((int) (win + minD + 16)); reset();
    }
    void reset() { dl.reset(); phase = 0; }
    float latency() const { return minD + win * 0.5f; }

    inline float process (float x, float ratio)
    {
        dl.push (x);
        phase += (1.0f - ratio) / win;
        phase -= std::floor (phase);
        const float p2 = phase + 0.5f - std::floor (phase + 0.5f);
        const float g1 = std::sin (kPi * phase), g2 = std::sin (kPi * p2);
        return dl.read (minD + phase * win) * g1 * g1 + dl.read (minD + p2 * win) * g2 * g2;
    }
    /** Lê sem empurrar (para usar várias cabeças no mesmo buffer — doubler). */
    float win = 1200, minD = 50;
private:
    double sr = 48000; DelayLine dl; float phase = 0;
};

// -----------------------------------------------------------------------------
//  Corretor de afinação (autotune)
// -----------------------------------------------------------------------------
class PitchCorrector
{
public:
    void prepare (double sampleRate, bool lowLatency)
    {
        sr = sampleRate; det.prepare (sr);
        const float w = lowLatency ? 12.0f : 24.0f;
        for (auto& s : sh) s.prepare (sr, w, 1.0f);
        compDelay[0].prepare ((int) (sh[0].latency() + 8)); compDelay[1].prepare ((int) (sh[0].latency() + 8));
        corr.prepare (sr, 20.0f); corr.reset (0); slowPitch = 0; currentNote = -1; reset();
        std::fill (std::begin (histogram), std::end (histogram), 0.0f);
    }
    void reset() { for (auto& s : sh) s.reset(); det.reset(); compDelay[0].reset(); compDelay[1].reset(); }

    int latency() const { return (int) std::lround (sh[0].latency()); }

    void setParams (int key, int scale, float speedMs, float humanizePct, float amountPct, float transposeSt)
    {
        buildScale (key, scale);
        speed = speedMs; humanize = humanizePct * 0.01f; amount = amountPct * 0.01f; transpose = transposeSt;
        corr.prepare (sr, std::max (0.3f, speedMs));
    }

    /** enabled=false: só atrasa (mantém a latência constante) mas ainda analisa o pitch. */
    void process (float* L, float* R, int n, bool enabled)
    {
        for (int i = 0; i < n; ++i)
        {
            const float mono = R ? 0.5f * (L[i] + R[i]) : L[i];
            if (det.push (mono)) updateTarget();
            const float semis = corr.next();
            const float ratio = std::pow (2.0f, semis / 12.0f);
            if (enabled)
            {
                L[i] = sh[0].process (L[i], ratio);
                if (R) R[i] = sh[1].process (R[i], ratio);
            }
            else
            {
                sh[0].process (L[i], 1.0f); if (R) sh[1].process (R[i], 1.0f);   // mantém buffers vivos
                compDelay[0].push (L[i]); L[i] = compDelay[0].read (sh[0].latency());
                if (R) { compDelay[1].push (R[i]); R[i] = compDelay[1].read (sh[0].latency()); }
            }
        }
    }

    // ---- telemetria para a UI/IA ----
    float detectedHz() const   { return lastHz; }
    float detectedMidi() const { return lastMidi; }
    float targetMidi() const   { return lastTarget; }
    float correctionSt() const { return corr.cur; }
    const float* pitchClassHistogram() const { return histogram; } // 12 bins (para detectar o tom da voz)
    void decayHistogram (float k) { for (float& h : histogram) h *= k; }

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
        const int base = (int) std::floor (midi);
        float best = midi, bestDist = 1e9f;
        for (int k = base - 6; k <= base + 7; ++k)
            if (inScale[((k % 12) + 12) % 12]) { const float dd = std::abs ((float) k - midi); if (dd < bestDist) { bestDist = dd; best = (float) k; } }
        return best;
    }

    void updateTarget()
    {
        const float f = det.frequency();
        if (f <= 0.0f || det.confidence() < 0.6f)
        {   // trecho surdo: solta a correção (só transposição)
            corr.set (transpose); lastHz = 0; currentNote = -1; return;
        }
        const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
        lastHz = f; lastMidi = midi;
        if (slowPitch == 0) slowPitch = midi;
        slowPitch += (midi - slowPitch) * 0.08f;  // pitch "lento" (contorno sem vibrato)

        // histerese: só troca de nota se passou 0.15 st da metade do caminho
        float target = nearestInScale (midi);
        if (currentNote >= 0 && (float) currentNote != target)
        {
            const float mid = 0.5f * ((float) currentNote + target);
            const bool crossed = target > (float) currentNote ? midi > mid + 0.15f : midi < mid - 0.15f;
            if (! crossed && inScale[((currentNote % 12) + 12) % 12]) target = (float) currentNote;
        }
        currentNote = (int) target; lastTarget = target;
        histogram[((currentNote % 12) + 12) % 12] += det.confidence();

        // humanizar: preserva parte do vibrato/expressão (desvio em relação ao contorno)
        const float expression = (midi - slowPitch) * humanize;
        const float correction = (target - midi + expression) * amount;
        corr.set (correction + transpose);
    }

    double sr = 48000; PitchDetector det; GranularShifter sh[2]; DelayLine compDelay[2]; Smoother corr;
    bool inScale[12] {}; float speed = 20, humanize = 0, amount = 1, transpose = 0;
    float slowPitch = 0, lastHz = 0, lastMidi = 0, lastTarget = 0; int currentNote = -1; float histogram[12] {};
};
} // namespace cupido
