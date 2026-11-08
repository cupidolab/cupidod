// Testes do motor CUPIDO (sem JUCE).  g++ -O2 -std=c++17 tools/test_dsp.cpp -o test_dsp && ./test_dsp
#define _USE_MATH_DEFINES
#include "../source/dsp/Chain.h"
#include <cstdio>
#include <chrono>
#include <random>

using namespace cupido;
static int failures = 0;
#define CHECK(cond, ...) do { if (cond) { std::printf ("  OK   "); } else { std::printf ("  FAIL "); ++failures; } std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)

static const double SR = 48000.0; static const int BLOCK = 512;

static ParamValues allOff()
{
    ParamValues p;
    for (P q : { P::GateOn, P::NrOn, P::TuneOn, P::EqOn, P::DsOn, P::CompOn, P::OptoOn, P::SatOn, P::AirOn, P::DblOn, P::DlyOn, P::RevOn, P::GlueOn, P::LimOn }) p[q] = 0;
    return p;
}
static void run (VocalChain& c, std::vector<float>& L, std::vector<float>& R, const float* sc = nullptr)
{
    for (size_t s = 0; s < L.size(); s += BLOCK)
    {
        const int n = (int) std::min ((size_t) BLOCK, L.size() - s);
        c.process (L.data() + s, R.data() + s, sc ? sc + s : nullptr, sc ? sc + s : nullptr, n);
    }
}
static float peak (const std::vector<float>& x, size_t from = 0) { float p = 0; for (size_t i = from; i < x.size(); ++i) p = std::max (p, std::abs (x[i])); return p; }
static bool finite (const std::vector<float>& x) { for (float v : x) if (! std::isfinite (v)) return false; return true; }
static float rmsDb (const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i]; return 10.0f * (float) std::log10 (s / (b - a) + 1e-20); }

static float measurePitch (const std::vector<float>& x, size_t from)
{
    PitchDetector d; d.prepare (SR); std::vector<float> m;
    for (size_t i = from; i < x.size(); ++i) if (d.push (x[i]) && d.frequency() > 0) m.push_back (69.0f + 12.0f * std::log2 (d.frequency() / 440.0f));
    if (m.empty()) return 0.0f; std::sort (m.begin(), m.end()); return m[m.size() / 2];   // mediana
}

static std::vector<float> voice (double f0, double secs, double vibratoCents = 0)
{
    std::vector<float> x ((size_t) (SR * secs)); double ph = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double f = f0 * std::pow (2.0, vibratoCents / 1200.0 * std::sin (2 * M_PI * 5.5 * i / SR));
        ph += 2 * M_PI * f / SR; double s = 0;
        for (int h = 1; h <= 8; ++h) s += std::sin (h * ph) / (h * 1.3);
        x[i] = (float) (0.25 * s);
    }
    return x;
}

int main()
{
    std::printf ("CUPIDO DSP — %d parâmetros\n", kNumParams);

    // 1) Latência e transparência com tudo desligado
    {
        VocalChain c; c.prepare (SR, BLOCK, false); auto p = allOff(); c.setParams (p, 120);
        std::vector<float> L (SR, 0.0f), R (SR, 0.0f); L[1000] = R[1000] = 1.0f; run (c, L, R);
        int at = -1; float mx = 0; for (int i = 0; i < (int) L.size(); ++i) if (std::abs (L[i]) > mx) { mx = std::abs (L[i]); at = i; }
        std::printf ("[latência] reportada=%d amostras (%.1f ms)\n", c.getLatency(), c.getLatency() * 1000.0 / SR);
        CHECK (std::abs (at - (1000 + c.getLatency())) <= 2, "impulso saiu em %d (esperado %d), pico %.3f", at, 1000 + c.getLatency(), mx);
        VocalChain c2; c2.prepare (SR, BLOCK, true); std::printf ("[latência] modo gravação=%d amostras (%.1f ms)\n", c2.getLatency(), c2.getLatency() * 1000.0 / SR);
    }

    // 2) Autotune: A3 desafinado (+40 cents) deve ir para 57 (A3) em Lá menor, velocidade 0
    {
        VocalChain c; c.prepare (SR, BLOCK, false); auto p = allOff();
        p[P::TuneOn] = 1; p[P::TuneKey] = 9; p[P::TuneScale] = 2; p[P::TuneSpeed] = 0; c.setParams (p, 120);
        auto L = voice (220.0 * std::pow (2.0, 40.0 / 1200.0), 3.0, 0); auto R = L;
        const float inMidi = measurePitch (L, (size_t) SR); run (c, L, R); const float outMidi = measurePitch (L, (size_t) SR);
        CHECK (std::abs (outMidi - 57.0f) < 0.12f, "afinação: entrada %.2f -> saída %.2f (alvo 57.00)", inMidi, outMidi);
        // nota fora da escala: G#3 (56) em Lá menor natural -> deve ir para 55 ou 57
        auto L2 = voice (207.65 * std::pow (2.0, 10.0 / 1200.0), 3.0); auto R2 = L2; run (c, L2, R2);
        const float o2 = measurePitch (L2, (size_t) SR);
        CHECK (std::abs (o2 - 57.0f) < 0.15f || std::abs (o2 - 55.0f) < 0.15f, "nota fora da escala 56.1 -> %.2f", o2);
        // transpor +12
        p[P::TuneScale] = 0; p[P::TuneTranspose] = 12; c.setParams (p, 120);
        auto L3 = voice (220.0, 3.0); auto R3 = L3; run (c, L3, R3); const float o3 = measurePitch (L3, (size_t) SR);
        CHECK (std::abs (o3 - 69.0f) < 0.2f, "transpor +12: 57 -> %.2f", o3);
    }

    // 3) Limiter: senoide +12 dB, teto -1 dBFS
    {
        VocalChain c; c.prepare (SR, BLOCK, false); auto p = allOff(); p[P::LimOn] = 1; p[P::LimGain] = 12; p[P::LimCeiling] = -1; c.setParams (p, 120);
        std::vector<float> L ((size_t) SR * 2), R;
        for (size_t i = 0; i < L.size(); ++i) L[i] = 0.7f * (float) std::sin (2 * M_PI * 100 * i / SR) + 0.3f * (float) std::sin (2 * M_PI * 3100 * i / SR);
        R = L; run (c, L, R); const float pk = peak (L, 4800);
        CHECK (pk <= dbToGain (-1.0f) + 1e-4f, "pico de saída %.2f dBFS (teto -1)", gainToDb (pk));
    }

    // 4) Tudo ligado, ruído + voz, extremos: estabilidade numérica + desempenho
    {
        VocalChain c; c.prepare (SR, BLOCK, false); ParamValues p;
        for (P q : { P::GateOn, P::NrOn, P::TuneOn, P::EqOn, P::DsOn, P::CompOn, P::OptoOn, P::SatOn, P::AirOn, P::DblOn, P::DlyOn, P::RevOn, P::GlueOn, P::LimOn }) p[q] = 1;
        p[P::SatB5Amt] = 12; p[P::SatB2Amt] = 6; p[P::SatDrive] = 100; p[P::DlyFeedback] = 95; p[P::RevDecay] = 15; p[P::DblVoices] = 1; p[P::EqLowGain] = 18;
        p[P::SatType] = 5; c.setParams (p, 140);
        auto L = voice (196.0, 10.0, 40); std::mt19937 rng (1); std::normal_distribution<float> nd (0, 0.01f);
        for (auto& v : L) v += nd (rng);
        auto R = L; auto t0 = std::chrono::high_resolution_clock::now(); run (c, L, R); auto t1 = std::chrono::high_resolution_clock::now();
        const double secs = std::chrono::duration<double> (t1 - t0).count();
        CHECK (finite (L) && finite (R), "sem NaN/Inf com tudo no máximo");
        CHECK (peak (L) <= 1.0f, "pico %.2f dBFS (limiter segurou)", gainToDb (peak (L)));
        std::printf ("  CPU  10 s de áudio estéreo com TUDO ligado em %.2f s  -> %.1fx tempo real (%.1f%% de um núcleo)\n", secs, 10.0 / secs, secs * 10.0);
        // varre todos os tipos/escolhas
        bool ok = true;
        for (int t = 0; t < 6; ++t) for (int m = 0; m < 4; ++m) for (int rv = 0; rv < 5; ++rv)
        {
            p[P::SatType] = (float) t; p[P::DlyMode] = (float) m; p[P::RevType] = (float) rv; p[P::CompChar] = (float) (rv % 3); c.setParams (p, 90);
            std::vector<float> a (4096), b (4096); for (size_t i = 0; i < a.size(); ++i) a[i] = b[i] = nd (rng) * 30.0f;
            run (c, a, b); ok &= finite (a) && finite (b);
        }
        CHECK (ok, "todas as combinações de tipo (saturação x delay x reverb x compressor) estáveis");
    }

    // 5) Redução de ruído: voz + ruído branco; aprende perfil no ruído puro
    {
        VocalChain c; c.prepare (SR, BLOCK, false); auto p = allOff(); p[P::NrOn] = 1; p[P::NrAmount] = 80; p[P::NrSensitivity] = 50; p[P::NrAdaptive] = 0; c.setParams (p, 120);
        std::mt19937 rng (7); std::normal_distribution<float> nd (0, 0.02f);
        std::vector<float> noiseL ((size_t) (SR * 3)); for (auto& v : noiseL) v = nd (rng); auto noiseR = noiseL;
        c.learnNoise (2.0f); run (c, noiseL, noiseR);
        std::vector<float> L ((size_t) (SR * 3)); for (auto& v : L) v = nd (rng); auto R = L;
        const float before = rmsDb (L, (size_t) SR, L.size()); run (c, L, R); const float after = rmsDb (L, (size_t) SR, L.size());
        CHECK (c.hasNoiseProfile() && after < before - 12.0f, "ruído de fundo: %.1f dB -> %.1f dB (perfil aprendido)", before, after);
        auto v = voice (220, 3); std::vector<float> vn = v; for (auto& s : vn) s += nd (rng); auto vR = vn;
        const float vb = rmsDb (vn, (size_t) SR, vn.size()); run (c, vn, vR); const float va = rmsDb (vn, (size_t) SR, vn.size());
        CHECK (std::abs (va - vb) < 1.5f, "voz preservada: %.1f dB -> %.1f dB", vb, va);
    }

    // 6) Análise de beat: kick+hat a 140 BPM + acordes de Lá menor no sidechain
    {
        VocalChain c; c.prepare (SR, BLOCK, false); auto p = allOff(); c.setParams (p, 0);
        const double bpm = 140.0, secs = 15; const size_t N = (size_t) (SR * secs);
        std::vector<float> beat (N, 0.0f); const double beatLen = 60.0 / bpm * SR;
        const double chordsHz[4][3] = { { 220.0, 261.63, 329.63 }, { 174.61, 220.0, 261.63 }, { 261.63, 329.63, 392.0 }, { 196.0, 246.94, 293.66 } }; // Am F C G
        for (size_t i = 0; i < N; ++i)
        {
            const double posBeats = i / beatLen; const double tIn = (posBeats - std::floor (posBeats)) * beatLen / SR;
            double kick = std::exp (-tIn * 18) * std::sin (2 * M_PI * (50 + 120 * std::exp (-tIn * 30)) * tIn);
            const double hatT = std::fmod (posBeats * 2, 1.0) * beatLen / 2 / SR;
            double hat = std::exp (-hatT * 80) * (((i * 2654435761u) >> 7) % 2000 / 1000.0 - 1.0) * 0.15;
            const int ch = (int) (posBeats / 8) % 4; double pad = 0;
            for (double f : chordsHz[ch]) pad += std::sin (2 * M_PI * f * i / SR) * 0.08 + std::sin (2 * M_PI * f * 2 * i / SR) * 0.03;
            beat[i] = (float) (0.7 * kick + hat + pad);
        }
        std::vector<float> L (N, 0.0f), R (N, 0.0f); run (c, L, R, beat.data());
        auto r = c.analyse (true);
        const bool bpmOk = std::abs (r.bpm - 140) < 2 || std::abs (r.bpm - 70) < 1.5;
        CHECK (r.valid && bpmOk, "BPM detectado %.1f (real 140) conf %.2f", r.bpm, r.bpmConfidence);
        CHECK (r.key == 9 && r.minor, "tom detectado %s %s (real A menor) conf %.2f", keyName (r.key), r.minor ? "menor" : "maior", r.keyConfidence);
        std::printf ("  info balanço: sub %.0f%% grave %.0f%% m-grave %.0f%% médio %.0f%% presença %.0f%% ar %.0f%%  rms %.1f dB crest %.1f\n",
                     r.bands[0], r.bands[1], r.bands[2], r.bands[3], r.bands[4], r.bands[5], r.rmsDb, r.crestDb);
    }

    // 7) De-esser reduz 7 kHz e mantém 300 Hz
    {
        VocalChain c; c.prepare (SR, BLOCK, false); auto p = allOff(); p[P::DsOn] = 1; p[P::DsThresh] = -40; p[P::DsRange] = 12; p[P::DsFreq] = 7000; c.setParams (p, 120);
        std::vector<float> L ((size_t) SR); for (size_t i = 0; i < L.size(); ++i) L[i] = 0.3f * (float) std::sin (2 * M_PI * 7000 * i / SR); auto R = L;
        const float b = rmsDb (L, 9600, L.size()); run (c, L, R); const float a = rmsDb (L, 9600, L.size());
        std::vector<float> L2 ((size_t) SR); for (size_t i = 0; i < L2.size(); ++i) L2[i] = 0.3f * (float) std::sin (2 * M_PI * 300 * i / SR); auto R2 = L2;
        const float b2 = rmsDb (L2, 9600, L2.size()); run (c, L2, R2); const float a2 = rmsDb (L2, 9600, L2.size());
        CHECK (a < b - 6 && std::abs (a2 - b2) < 0.5f, "de-esser: 7 kHz %.1f->%.1f dB, 300 Hz %.1f->%.1f dB", b, a, b2, a2);
    }

    // 8) Prisma banda 5 adiciona agudos (harmônicos) e Ar levanta topo
    {
        VocalChain c; c.prepare (SR, BLOCK, false); auto p = allOff(); p[P::SatOn] = 1; p[P::SatB5Amt] = 9; p[P::SatDrive] = 60; c.setParams (p, 120);
        auto L = voice (220, 2); for (size_t i = 0; i < L.size(); ++i) L[i] += 0.01f * (float) std::sin (2 * M_PI * 7040 * i / SR); auto R = L; std::vector<float> spec; computeDisplaySpectrum (L, SR, 64, spec); const float hiB = spec[58];
        run (c, L, R); std::vector<float> spec2; computeDisplaySpectrum (L, SR, 64, spec2);
        CHECK (spec2[58] > hiB + 3, "Prisma agudos: banda ~10k %.1f -> %.1f dB", hiB, spec2[58]);
    }

    std::printf ("\n%s (%d falha(s))\n", failures ? "ALGUNS TESTES FALHARAM" : "TODOS OS TESTES PASSARAM", failures);
    return failures ? 1 : 0;
}
