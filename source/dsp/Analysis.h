// =============================================================================
//  CUPIDO DSP — Análise: medidor LUFS (ITU-R BS.1770 / EBU R128), captura de
//  áudio para o "ouvido" da IA, e análise offline de beat/voz:
//  BPM (fluxo espectral + autocorrelação), tom (cromagrama + perfis
//  Krumhansl-Schmuckler), balanço espectral, nível, crest, piso de ruído e sibilância.
// =============================================================================
#pragma once
#include "Core.h"
#include <string>

namespace cupido
{
// -----------------------------------------------------------------------------
//  LUFS: K-weighting (shelf +4 dB em ~1.7 kHz + passa-altas RLB ~38 Hz)
// -----------------------------------------------------------------------------
class LoudnessMeter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // Coeficientes derivados da forma analógica (como libebur128), válidos p/ qualquer sr
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (3.14159265358979323846 * f0 / sr), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            for (auto& s : pre) s.c = { (float) ((Vh + Vb * K / Q + K * K) / a0), (float) (2.0 * (K * K - Vh) / a0), (float) ((Vh - Vb * K / Q + K * K) / a0),
                                        (float) (2.0 * (K * K - 1.0) / a0), (float) ((1.0 - K / Q + K * K) / a0) };
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan (3.14159265358979323846 * f0 / sr);
            const double a0 = 1.0 + K / Q + K * K;
            for (auto& s : rlb) s.c = { 1.0f, -2.0f, 1.0f, (float) (2.0 * (K * K - 1.0) / a0), (float) ((1.0 - K / Q + K * K) / a0) };
        }
        blockLen = (int) (0.1 * sr); reset();
    }
    void reset()
    {
        for (auto& s : pre) s.reset();
        for (auto& s : rlb) s.reset();
        acc = 0; accN = 0; ring.fill (0); ringPos = 0; blocks.clear(); blocks.reserve (36000);
        momentary = shortTerm = integrated = -70.0f;
    }
    void process (const float* L, const float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float l = rlb[0].process (pre[0].process (L[i]));
            const float r = R ? rlb[1].process (pre[1].process (R[i])) : l;
            acc += (double) l * l + (R ? (double) r * r : 0.0);
            if (++accN >= blockLen)
            {
                const float ms = (float) (acc / accN); acc = 0; accN = 0;
                ring[(size_t) ringPos] = ms; ringPos = (ringPos + 1) % 30;
                double m4 = 0; for (int k = 0; k < 4; ++k) m4 += ring[(size_t) ((ringPos - 1 - k + 30) % 30)];
                double s30 = 0; for (float v : ring) s30 += v;
                momentary = toLufs (m4 / 4.0); shortTerm = toLufs (s30 / 30.0);
                if (blocks.size() < blocks.capacity()) blocks.push_back ((float) (m4 / 4.0));
                if (++sinceGate >= 5) { sinceGate = 0; computeIntegrated(); }
            }
        }
    }
    float momentary = -70, shortTerm = -70, integrated = -70;
private:
    static float toLufs (double ms) { return ms > 1e-12 ? (float) (-0.691 + 10.0 * std::log10 (ms)) : -70.0f; }
    void computeIntegrated()
    {
        double sum = 0; int cnt = 0;
        for (float b : blocks) if (toLufs (b) > -70.0f) { sum += b; ++cnt; }
        if (cnt == 0) { integrated = -70; return; }
        const float rel = toLufs (sum / cnt) - 10.0f; sum = 0; cnt = 0;
        for (float b : blocks) if (toLufs (b) > rel) { sum += b; ++cnt; }
        integrated = cnt ? toLufs (sum / cnt) : -70.0f;
    }
    double sr = 48000, acc = 0; int accN = 0, blockLen = 4800, ringPos = 0, sinceGate = 0;
    Biquad pre[2], rlb[2]; std::array<float, 30> ring {}; std::vector<float> blocks;
};

// -----------------------------------------------------------------------------
//  Captura em anel (o áudio escreve; a UI copia um snapshot quando analisa)
// -----------------------------------------------------------------------------
class CaptureRing
{
public:
    void prepare (int capacity) { data.assign ((size_t) capacity, 0.0f); w.store (0); filled.store (0); }
    void push (float x)
    {
        const int i = w.load (std::memory_order_relaxed);
        data[(size_t) i] = x; w.store ((i + 1) % (int) data.size(), std::memory_order_release);
        const int f = filled.load (std::memory_order_relaxed); if (f < (int) data.size()) filled.store (f + 1, std::memory_order_relaxed);
    }
    /** Copia as últimas `count` amostras (mais antigas primeiro). */
    std::vector<float> snapshot (int count) const
    {
        const int cap = (int) data.size(); count = std::min (count, filled.load());
        std::vector<float> out ((size_t) std::max (0, count));
        const int end = w.load (std::memory_order_acquire);
        for (int i = 0; i < count; ++i) out[(size_t) i] = data[(size_t) ((end - count + i + cap * 2) % cap)];
        return out;
    }
    int available() const { return filled.load(); }
    void clear() { filled.store (0); }
private:
    std::vector<float> data; std::atomic<int> w { 0 }, filled { 0 };
};

/** Decimador simples (anti-aliasing biquad duplo) para a captura longa. */
struct Decimator
{
    int factor = 4, count = 0; Biquad a, b;
    void prepare (double sr, double targetRate)
    {
        factor = std::max (1, (int) std::lround (sr / targetRate));
        const double fc = sr / factor * 0.45; a.set (BqType::LowPass, sr, fc, 0.54); b.set (BqType::LowPass, sr, fc, 1.31); count = 0;
    }
    bool push (float x, float& out) { const float y = b.process (a.process (x)); if (++count >= factor) { count = 0; out = y; return true; } return false; }
};

// -----------------------------------------------------------------------------
//  Resultado da análise (vira JSON para a UI / IA)
// -----------------------------------------------------------------------------
struct AnalysisResult
{
    bool  valid = false;
    float seconds = 0;
    float bpm = 0, bpmConfidence = 0;
    int   key = -1; bool minor = false; float keyConfidence = 0;
    float rmsDb = -100, peakDb = -100, crestDb = 0, noiseFloorDb = -100, sibilanceDb = -100;
    float bands[6] {};       // % de energia: sub, grave, médio-grave, médio, presença, ar
    float bandsDb[6] {};
    float spectrum[32] {};   // dB em 32 bandas log (20 Hz – 20 kHz)
};

inline const char* keyName (int k) { static const char* n[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }; return k >= 0 && k < 12 ? n[k] : "?"; }

/** Energia por banda via FFT média (sinal em taxa cheia). */
inline void analyseSpectrumBalance (const std::vector<float>& x, double sr, AnalysisResult& r)
{
    constexpr int order = 12, N = 1 << order;
    if ((int) x.size() < N) return;
    FFT fft; fft.prepare (order); std::vector<float> win; makeHann (win, N);
    std::vector<std::complex<float>> buf ((size_t) N); std::vector<double> pw (N / 2 + 1, 0.0);
    int frames = 0;
    for (size_t start = 0; start + N <= x.size(); start += N / 2, ++frames)
    {
        for (int i = 0; i < N; ++i) buf[(size_t) i] = { x[start + (size_t) i] * win[(size_t) i], 0.0f };
        fft.forward (buf.data());
        for (int k = 0; k <= N / 2; ++k) pw[(size_t) k] += std::norm (buf[(size_t) k]);
    }
    static const double edges[7] = { 20, 60, 250, 800, 2500, 6000, 20000 };
    double e[6] {}, tot = 0, sib = 0, s32[32] {};
    for (int k = 1; k <= N / 2; ++k)
    {
        const double f = k * sr / N;
        if (f >= 20 && f < 20000) s32[std::min (31, (int) (32.0 * std::log (f / 20.0) / std::log (1000.0)))] += pw[(size_t) k];
        for (int b = 0; b < 6; ++b) if (f >= edges[b] && f < edges[b + 1]) { e[b] += pw[(size_t) k]; tot += pw[(size_t) k]; }
        if (f >= 5000 && f < 10000) sib += pw[(size_t) k];
    }
    for (int b = 0; b < 6; ++b) { r.bands[b] = tot > 0 ? (float) (100.0 * e[b] / tot) : 0.0f; r.bandsDb[b] = (float) (10.0 * std::log10 (e[b] / std::max (1, frames) + 1e-20)) - 66.0f; }
    r.sibilanceDb = tot > 0 ? (float) (10.0 * std::log10 (sib / tot + 1e-12)) : -100.0f;
    for (int b = 0; b < 32; ++b) r.spectrum[b] = (float) (10.0 * std::log10 (s32[b] / std::max (1, frames) + 1e-20)) - 66.0f;
}

/** Nível RMS, pico, crest e piso de ruído (percentil 10 de janelas de 50 ms). */
inline void analyseLevels (const std::vector<float>& x, double sr, AnalysisResult& r)
{
    if (x.empty()) return;
    double s = 0; float pk = 0; for (float v : x) { s += (double) v * v; pk = std::max (pk, std::abs (v)); }
    r.rmsDb = 10.0f * (float) std::log10 (s / (double) x.size() + 1e-20); r.peakDb = gainToDb (pk); r.crestDb = r.peakDb - r.rmsDb;
    const int win = std::max (1, (int) (0.05 * sr)); std::vector<float> rms;
    for (size_t i = 0; i + (size_t) win <= x.size(); i += (size_t) win)
    { double e = 0; for (int j = 0; j < win; ++j) e += (double) x[i + (size_t) j] * x[i + (size_t) j]; rms.push_back (10.0f * (float) std::log10 (e / win + 1e-20)); }
    if (! rms.empty()) { std::sort (rms.begin(), rms.end()); r.noiseFloorDb = rms[rms.size() / 10]; }
    r.seconds = (float) (x.size() / sr);
}

/** BPM por fluxo espectral + autocorrelação com prior perceptual (centrado ~ 120-140). */
inline void analyseTempo (const std::vector<float>& x, double sr, AnalysisResult& r)
{
    constexpr int order = 10, N = 1 << order; const int hop = 128;
    if ((int) x.size() < N * 8) return;
    FFT fft; fft.prepare (order); std::vector<float> win; makeHann (win, N);
    std::vector<std::complex<float>> buf ((size_t) N); std::vector<float> prev (N / 2 + 1, 0.0f), onset;
    for (size_t start = 0; start + N <= x.size(); start += hop)
    {
        for (int i = 0; i < N; ++i) buf[(size_t) i] = { x[start + (size_t) i] * win[(size_t) i], 0.0f };
        fft.forward (buf.data()); float flux = 0;
        for (int k = 1; k <= N / 2; ++k) { const float m = std::log1p (100.0f * std::abs (buf[(size_t) k])); flux += std::max (0.0f, m - prev[(size_t) k]); prev[(size_t) k] = m; }
        onset.push_back (flux);
    }
    const double fps = sr / hop; const int M = (int) onset.size();
    // remove média local + retificação
    std::vector<float> o ((size_t) M); const int half = (int) (fps * 0.25);
    for (int i = 0; i < M; ++i) { double s = 0; int c = 0; for (int j = std::max (0, i - half); j < std::min (M, i + half); ++j) { s += onset[(size_t) j]; ++c; } o[(size_t) i] = std::max (0.0f, onset[(size_t) i] - (float) (s / c)); }
    const int lagMin = (int) (fps * 60.0 / 200.0), lagMax = std::min (M / 2, (int) (fps * 60.0 / 60.0));
    std::vector<double> ac ((size_t) lagMax + 2, 0.0);
    for (int lag = lagMin; lag <= lagMax; ++lag) { double s = 0; for (int i = 0; i + lag < M; ++i) s += (double) o[(size_t) i] * o[(size_t) (i + lag)]; ac[(size_t) lag] = s / (M - lag); }
    double best = -1; int bestLag = 0, secondBest = 0; double total = 0;
    for (int lag = lagMin; lag <= lagMax; ++lag)
    {
        const double bpm = 60.0 * fps / lag;
        const double prior = std::exp (-0.5 * std::pow (std::log2 (bpm / 125.0) / 0.9, 2.0));
        // reforço por harmônicos (lag*2) – estabiliza pulsos
        const double harm = lag * 2 <= lagMax ? 0.5 * ac[(size_t) lag * 2] : 0.0;
        const double sc = (ac[(size_t) lag] + harm) * prior; total += sc;
        if (sc > best) { secondBest = bestLag; best = sc; bestLag = lag; }
    }
    if (bestLag <= 0) return;
    double t = bestLag;
    if (bestLag > lagMin && bestLag < lagMax)
    { const double a = ac[(size_t) bestLag - 1], b = ac[(size_t) bestLag], c = ac[(size_t) bestLag + 1]; const double den = a - 2 * b + c; if (std::abs (den) > 1e-12) t += 0.5 * (a - c) / den; }
    r.bpm = (float) (60.0 * fps / t); r.bpmConfidence = (float) clampf ((float) (best * (lagMax - lagMin) / std::max (1e-12, total)) / 4.0f, 0.0f, 1.0f);
    (void) secondBest;
}

/** Tom por cromagrama (55–2000 Hz) e correlação com perfis de Krumhansl. */
inline void analyseKey (const std::vector<float>& x, double sr, AnalysisResult& r, const float* extraChroma = nullptr)
{
    double chroma[12] {};
    constexpr int order = 13, N = 1 << order;
    if ((int) x.size() >= N)
    {
        FFT fft; fft.prepare (order); std::vector<float> win; makeHann (win, N);
        std::vector<std::complex<float>> buf ((size_t) N);
        for (size_t start = 0; start + N <= x.size(); start += N / 2)
        {
            for (int i = 0; i < N; ++i) buf[(size_t) i] = { x[start + (size_t) i] * win[(size_t) i], 0.0f };
            fft.forward (buf.data());
            for (int k = 1; k < N / 2; ++k)
            {
                const double f = k * sr / N; if (f < 55 || f > 2000) continue;
                const double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
                const int pc = ((int) std::lround (midi) % 12 + 12) % 12;
                const double dev = std::abs (midi - std::round (midi));
                chroma[pc] += std::log1p (10.0 * std::abs (buf[(size_t) k])) * (1.0 - dev);
            }
        }
    }
    if (extraChroma) for (int i = 0; i < 12; ++i) chroma[i] += extraChroma[i];
    double sum = 0; for (double c : chroma) sum += c; if (sum <= 0) return;
    static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double mnr[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    auto corr = [&] (const double* prof, int key)
    {
        double mx = 0, my = 0; for (int i = 0; i < 12; ++i) { mx += chroma[i]; my += prof[i]; } mx /= 12; my /= 12;
        double sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i) { const double a = chroma[(i + key) % 12] - mx, b = prof[i] - my; sxy += a * b; sxx += a * a; syy += b * b; }
        return sxy / std::sqrt (sxx * syy + 1e-20);
    };
    double best = -2, second = -2; int bk = 0; bool bm = false;
    for (int k = 0; k < 12; ++k)
        for (int m = 0; m < 2; ++m)
        {
            const double c = corr (m ? mnr : maj, k);
            if (c > best) { second = best; best = c; bk = k; bm = m == 1; } else if (c > second) second = c;
        }
    r.key = bk; r.minor = bm; r.keyConfidence = (float) clampf ((float) ((best - second) * 5.0 + best * 0.5), 0.0f, 1.0f);
}

/** Pacote completo: chamado numa thread de fundo. */
inline AnalysisResult analyseAll (const std::vector<float>& longDecimated, double decRate,
                                  const std::vector<float>& shortFull, double fullRate,
                                  const float* extraChroma = nullptr)
{
    AnalysisResult r;
    if (longDecimated.size() < (size_t) (decRate * 2.0)) return r;
    analyseLevels (longDecimated, decRate, r);
    analyseTempo (longDecimated, decRate, r);
    analyseKey (longDecimated, decRate, r, extraChroma);
    AnalysisResult tmp; analyseLevels (shortFull, fullRate, tmp); r.peakDb = std::max (r.peakDb, tmp.peakDb);
    analyseSpectrumBalance (shortFull, fullRate, r);
    r.valid = true; return r;
}


// =============================================================================
//  ANÁLISE DE FAIXA INTEIRA (arquivo anexado: beat, voz ou referência)
//  Extrai o "DNA" do som: BPM, tom, loudness, dinâmica, espectro em 32 bandas
//  e marcas de estilo (808 sustentado, densidade de hi-hat, chimbal em tercina,
//  contratempo do reggae, kick, brilho).
// =============================================================================
struct TrackFeatures
{
    bool  valid = false;
    float seconds = 0, bpm = 0, bpmConfidence = 0;
    int   key = -1; bool minor = false; float keyConfidence = 0;
    float lufs = -70, rmsDb = -100, peakDb = -100, crestDb = 0, noiseFloorDb = -100, sibilanceDb = -100;
    float bands[6] {};                 // % de energia: sub, grave, médio-grave, médio, presença, ar
    float spectrum[32] {};             // dB médio em 32 bandas log (20 Hz – 20 kHz)
    float centroidHz = 0;              // brilho
    float subSustain = 0;              // 0..1 fração do tempo com sub (30–70 Hz) presente (808 longo)
    float hatRate = 0, kickRate = 0;   // eventos/segundo em 6–16 kHz e 40–120 Hz
    float tripletRatio = 0;            // 0..1 quanto os hi-hats caem em grade de tercina
    float offbeatRatio = 0;            // >1 = médios acentuam contratempo (skank do reggae)
    float offbeatRatioHalf = 0, tripletRatioHalf = 0;   // os mesmos medidos na metade do BPM (resolve 75 x 150)
    float stereoWidth = 0;             // 0 mono .. 1 bem largo (se estéreo)
};

inline float bandCenter32 (int b) { return (float) (20.0 * std::pow (1000.0, (b + 0.5) / 32.0)); }

/** L e R em taxa cheia (R pode ser vazio). */
inline TrackFeatures analyseTrack (const std::vector<float>& Lin, const std::vector<float>& Rin, double sr)
{
    TrackFeatures f;
    if (Lin.size() < (size_t) (sr * 3)) return f;
    // mono e largura estéreo
    std::vector<float> mono (Lin.size());
    double sm = 0, ss = 0;
    for (size_t i = 0; i < Lin.size(); ++i)
    {
        const float r = Rin.empty() ? Lin[i] : Rin[i];
        mono[i] = 0.5f * (Lin[i] + r); const float side = 0.5f * (Lin[i] - r);
        sm += (double) mono[i] * mono[i]; ss += (double) side * side;
    }
    f.stereoWidth = (float) clampf ((float) std::sqrt (ss / (sm + 1e-20)) * 1.6f, 0.0f, 1.0f);
    f.seconds = (float) (mono.size() / sr);

    // LUFS integrado (aproximação por K-weighting de 1 canal duplicado)
    { LoudnessMeter lm; lm.prepare (sr); const size_t blk = 4096;
      for (size_t s = 0; s < Lin.size(); s += blk) { const int n = (int) std::min (blk, Lin.size() - s); lm.process (Lin.data() + s, Rin.empty() ? Lin.data() + s : Rin.data() + s, n); }
      f.lufs = lm.integrated; }

    // trecho analisado: até 75 s a partir de 15% da faixa (pula intro)
    const size_t maxLen = (size_t) (sr * 75), start = mono.size() > maxLen ? (mono.size() - maxLen) / 4 : 0;
    std::vector<float> seg (mono.begin() + (long) start, mono.begin() + (long) std::min (mono.size(), start + maxLen));

    AnalysisResult r; analyseLevels (seg, sr, r); analyseSpectrumBalance (seg, sr, r);
    f.rmsDb = r.rmsDb; f.peakDb = r.peakDb; f.crestDb = r.crestDb; f.noiseFloorDb = r.noiseFloorDb; f.sibilanceDb = r.sibilanceDb;
    for (int b = 0; b < 6; ++b) f.bands[b] = r.bands[b];

    // tempo e tom em 12 kHz
    { Decimator dz; dz.prepare (sr, 12000.0); std::vector<float> d; d.reserve (seg.size() / dz.factor + 1); float o;
      for (float v : seg) if (dz.push (v, o)) d.push_back (o);
      const double dr = sr / dz.factor; AnalysisResult t; analyseTempo (d, dr, t); analyseKey (d, dr, t);
      f.bpm = t.bpm; f.bpmConfidence = t.bpmConfidence; f.key = t.key; f.minor = t.minor; f.keyConfidence = t.keyConfidence; }

    // STFT para espectro 32 bandas, centróide, sub sustentado e onsets por banda
    constexpr int order = 11, N = 1 << order; const int hop = N / 2;
    FFT fft; fft.prepare (order); std::vector<float> win; makeHann (win, N);
    std::vector<std::complex<float>> buf ((size_t) N);
    std::vector<double> spec32 (32, 0.0); double cenNum = 0, cenDen = 0;
    std::vector<float> subE, hiFlux, loFlux, midFlux; float prevHi = 0, prevLo = 0, prevMid = 0;
    for (size_t s0 = 0; s0 + N <= seg.size(); s0 += hop)
    {
        for (int i = 0; i < N; ++i) buf[(size_t) i] = { seg[s0 + (size_t) i] * win[(size_t) i], 0.0f };
        fft.forward (buf.data());
        double sub = 0, hi = 0, lo = 0, mid = 0;
        for (int k = 1; k < N / 2; ++k)
        {
            const double fr = k * sr / N, p = std::norm (buf[(size_t) k]);
            if (fr >= 20 && fr < 20000) { const int b = std::min (31, (int) (32.0 * std::log (fr / 20.0) / std::log (1000.0))); spec32[(size_t) b] += p; }
            cenNum += fr * p; cenDen += p;
            if (fr >= 30 && fr < 70) sub += p;
            if (fr >= 6000 && fr < 16000) hi += p;
            if (fr >= 40 && fr < 120) lo += p;
            if (fr >= 300 && fr < 2000) mid += p;
        }
        subE.push_back ((float) sub);
        const float lh = (float) std::log1p (hi * 1e3), ll = (float) std::log1p (lo * 1e3), lmid = (float) std::log1p (mid * 1e3);
        hiFlux.push_back (std::max (0.0f, lh - prevHi)); loFlux.push_back (std::max (0.0f, ll - prevLo)); midFlux.push_back (std::max (0.0f, lmid - prevMid));
        prevHi = lh; prevLo = ll; prevMid = lmid;
    }
    const size_t frames = subE.size(); if (frames < 8) return f;
    for (int b = 0; b < 32; ++b) f.spectrum[b] = (float) (10.0 * std::log10 (spec32[(size_t) b] / frames + 1e-20)) - 60.0f;
    f.centroidHz = (float) (cenNum / (cenDen + 1e-20));
    { std::vector<float> s2 = subE; std::sort (s2.begin(), s2.end()); const float ref = s2[s2.size() * 95 / 100] + 1e-20f;
      int on = 0; for (float e : subE) if (e > ref * 0.1f) ++on; f.subSustain = (float) on / (float) frames; }

    const double fps = sr / hop;
    auto peaks = [&] (const std::vector<float>& fl, std::vector<double>& times)
    {
        const size_t M = fl.size(); const int w = (int) (fps * 0.4);
        for (size_t i = 1; i + 1 < M; ++i)
        {
            double m = 0; int c = 0; for (long j = (long) i - w; j <= (long) i + w; ++j) if (j >= 0 && j < (long) M) { m += fl[(size_t) j]; ++c; }
            m /= std::max (1, c);
            if (fl[i] > fl[i - 1] && fl[i] >= fl[i + 1] && fl[i] > m * 1.8 + 0.05) times.push_back (i / fps);
        }
    };
    std::vector<double> hiT, loT, midT; peaks (hiFlux, hiT); peaks (loFlux, loT); peaks (midFlux, midT);
    const double dur = frames / fps;
    f.hatRate = (float) (hiT.size() / dur); f.kickRate = (float) (loT.size() / dur);
    auto rhythm = [&] (double bpm, float& tripletOut, float& offbeatOut)
    {
        const double beat = 60.0 / bpm; int trip = 0, straight = 0;
        for (size_t i = 1; i < hiT.size(); ++i)
        {
            const double ioi = hiT[i] - hiT[i - 1];
            for (double div : { 3.0, 6.0 }) if (std::abs (ioi - beat / div) < beat / div * 0.12) ++trip;
            for (double div : { 2.0, 4.0 }) if (std::abs (ioi - beat / div) < beat / div * 0.12) ++straight;
        }
        tripletOut = (float) trip / (float) std::max (1, trip + straight);
        if (loT.empty()) return;
        // fase de referência = fase mais comum dos kicks (robusto a kicks fora do tempo forte)
        int hist[20] {}; for (double t : loT) { double ph = std::fmod (t / beat, 1.0); hist[std::min (19, (int) (ph * 20))]++; }
        int bi = 0; for (int i = 1; i < 20; ++i) if (hist[i] > hist[bi]) bi = i;
        const double ref = (bi + 0.5) / 20.0; double on = 0, off = 0;
        for (double t : midT)
        {
            double ph = std::fmod (t / beat - ref + 10.0, 1.0);
            if (ph < 0.12 || ph > 0.88) on += 1; else if (std::abs (ph - 0.5) < 0.12) off += 1;
        }
        offbeatOut = (float) ((off + 1) / (on + 1));
    };
    if (f.bpm > 0) { rhythm (f.bpm, f.tripletRatio, f.offbeatRatio); rhythm (f.bpm * 0.5, f.tripletRatioHalf, f.offbeatRatioHalf); }
    f.valid = true;
    return f;
}

/** Espectro em bandas log (para o analisador da interface). */
inline void computeDisplaySpectrum (const std::vector<float>& x, double sr, int numBands, std::vector<float>& outDb)
{
    constexpr int order = 11, N = 1 << order; outDb.assign ((size_t) numBands, -100.0f);
    if ((int) x.size() < N) return;
    static thread_local FFT fft; static thread_local std::vector<float> win;
    if (fft.size() != N) { fft.prepare (order); makeHann (win, N); }
    std::vector<std::complex<float>> buf ((size_t) N);
    const size_t off = x.size() - N;
    for (int i = 0; i < N; ++i) buf[(size_t) i] = { x[off + (size_t) i] * win[(size_t) i], 0.0f };
    fft.forward (buf.data());
    const double fmin = 20, fmax = std::min (20000.0, sr * 0.5);
    for (int b = 0; b < numBands; ++b)
    {
        const double f0 = fmin * std::pow (fmax / fmin, (double) b / numBands), f1 = fmin * std::pow (fmax / fmin, (double) (b + 1) / numBands);
        int k0 = (int) (f0 * N / sr), k1 = std::max (k0 + 1, (int) (f1 * N / sr)); double mx = 0;
        for (int k = k0; k < k1 && k <= N / 2; ++k) mx = std::max (mx, (double) std::abs (buf[(size_t) k]));
        outDb[(size_t) b] = (float) (20.0 * std::log10 (mx * 4.0 / N + 1e-9));
    }
}
} // namespace cupido
