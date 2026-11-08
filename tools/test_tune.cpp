// Teste do AFINA com voz sintética realista (pulsos glotais + formantes + vibrato + deslizes).
//   g++ -O2 -std=c++17 tools/test_tune.cpp -o test_tune && ./test_tune [saida.wav]
#define _USE_MATH_DEFINES
#include "../source/dsp/Pitch.h"
#include "../source/VoiceUtils.h"
#include <cstdio>
#include <fstream>

using namespace cupido;
static const double SR = 48000.0;

// Voz: trem de pulsos glotais (Rosenberg) filtrado por formantes do "a" + respiração
static std::vector<float> formantVoice (const std::vector<double>& midiCurve, unsigned seed = 3)
{
    std::vector<float> out (midiCurve.size());
    Biquad f1, f2, f3, f4;
    f1.set (BqType::BandPass, SR, 730, 6); f2.set (BqType::BandPass, SR, 1090, 8); f3.set (BqType::BandPass, SR, 2440, 10); f4.set (BqType::BandPass, SR, 3400, 12);
    double ph = 0; Rng rng (seed);
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double f0 = 440.0 * std::pow (2.0, (midiCurve[i] - 69.0) / 12.0);
        ph += f0 / SR; if (ph >= 1) ph -= 1;
        // pulso de Rosenberg: abre 40%, fecha 16%
        double g = ph < 0.4 ? 0.5 * (1 - std::cos (M_PI * ph / 0.4)) : ph < 0.56 ? std::cos (M_PI * (ph - 0.4) / 0.32) : 0.0;
        const float src = (float) (g - 0.3) + 0.01f * rng.next();
        float y = 1.0f * f1.process (src) + 0.7f * f2.process (src) + 0.35f * f3.process (src) + 0.15f * f4.process (src);
        out[i] = y * 0.9f;
    }
    float pk = 0; for (float v : out) pk = std::max (pk, std::abs (v)); for (float& v : out) v *= 0.5f / pk;
    return out;
}

struct Stats { double medianErr = 0; double octaveErrPct = 0; double subharmonicDb = 0; };

// mede pitch da saída a cada hop e compara com o alvo esperado
static Stats measure (const std::vector<float>& y, const std::vector<double>& targetMidi, size_t from)
{
    PitchDetector d; d.prepare (SR); std::vector<double> errs; int oct = 0, tot = 0;
    for (size_t i = 0; i < y.size(); ++i)
        if (d.push (y[i]) && i > from && d.frequency() > 0)
        {
            const double m = 69.0 + 12.0 * std::log2 (d.frequency() / 440.0), t = targetMidi[std::min (i, targetMidi.size() - 1)];
            const double e = m - t; ++tot; if (std::abs (e) > 6) ++oct; errs.push_back (std::abs (e));
        }
    Stats s; if (errs.empty()) return s;
    std::sort (errs.begin(), errs.end()); s.medianErr = errs[errs.size() / 2]; s.octaveErrPct = 100.0 * oct / std::max (1, tot);
    return s;
}

// energia em f0/2 relativa a f0 (fantasma de oitava abaixo) num trecho estável
static double subharmonicDb (const std::vector<float>& y, size_t start, size_t len, double f0)
{
    auto goertzel = [&] (double f) { double s1 = 0, s2 = 0, c = 2 * std::cos (2 * M_PI * f / SR);
        for (size_t i = start; i < start + len; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * M_PI * (i - start) / len); const double s0 = y[i] * w + c * s1 - s2; s2 = s1; s1 = s0; }
        return s1 * s1 + s2 * s2 - c * s1 * s2; };
    return 10 * std::log10 ((goertzel (f0 * 0.5) + goertzel (f0 * 1.5)) / 2 / (goertzel (f0) + 1e-20) + 1e-20);
}

int main (int argc, char** argv)
{
    int fails = 0;
    // frase: notas de Lá menor desafinadas, com vibrato e deslizes, voz masculina (A2..E3) e feminina (A3..E4)
    for (int voiceOct = 0; voiceOct < 2; ++voiceOct)
    {
        const double base = voiceOct == 0 ? 45.0 : 57.0;   // A2 ou A3
        const double notes[] = { 0, 3, 5, 7, 5, 3, 0, -2 };     // A C D E D C A G (todas em Lá menor)
        const double detune[] = { 0.35, -0.3, 0.42, -0.25, 0.3, -0.4, 0.2, 0.38 };
        std::vector<double> curve, target;
        for (int k = 0; k < 8; ++k)
        {
            const size_t len = (size_t) (SR * 0.6);
            for (size_t i = 0; i < len; ++i)
            {
                const double glide = i < SR * 0.05 && k > 0 ? (notes[k - 1] + detune[k - 1] - notes[k] - detune[k]) * (1 - i / (SR * 0.05)) : 0;
                const double vib = 0.25 * std::sin (2 * M_PI * 5.2 * (curve.size()) / SR) * std::min (1.0, i / (SR * 0.25));
                curve.push_back (base + notes[k] + detune[k] + glide + vib);
                target.push_back (base + notes[k]);
            }
        }
        auto x = formantVoice (curve);
        for (int mode = 0; mode < 2; ++mode)
        {
            PitchCorrector pc; pc.prepare (SR, mode == 1);
            pc.setParams (9, 2, 0.0f, 0.0f, 100.0f, 0.0f);        // Lá menor, robótico
            auto L = x, R = x;
            for (size_t s = 0; s < L.size(); s += 256) pc.process (L.data() + s, R.data() + s, (int) std::min<size_t> (256, L.size() - s), true);
            // compensa a latência ao medir
            std::vector<double> tShift (target.size()); for (size_t i = 0; i < target.size(); ++i) tShift[i] = target[i >= (size_t) pc.latency() ? i - pc.latency() : 0];
            // ignora os 120 ms após cada troca de nota (transição)
            std::vector<float> masked = L; std::vector<double> tm = tShift;
            auto st = measure (L, tShift, (size_t) (SR * 0.3));
            const double f0 = 440.0 * std::pow (2.0, (base + 7 - 69.0) / 12.0);
            const size_t seg = (size_t) (SR * 0.6 * 3 + SR * 0.25) + pc.latency();
            const double sub = subharmonicDb (L, seg, (size_t) (SR * 0.3), f0);
            const double subIn = subharmonicDb (x, seg - pc.latency(), (size_t) (SR * 0.3), 440.0 * std::pow (2.0, (base + 7 - 0.25 - 69.0) / 12.0));
            const bool ok = st.medianErr < 0.2 && st.octaveErrPct < 1.0 && sub < -40.0;   // -40 dB = inaudível
            std::printf ("  %s voz %s, modo %s: erro mediano %.3f st, erros de oitava %.2f%%, fantasma f0/2 %.1f dB (entrada %.1f), latência %.1f ms\n",
                         ok ? "OK  " : "FAIL", voiceOct ? "aguda " : "grave ", mode ? "ao vivo" : "HQ     ", st.medianErr, st.octaveErrPct, sub, subIn, pc.latency() * 1000.0 / SR);
            if (! ok) ++fails;
            if (argc > 1 && voiceOct == 0 && mode == 0)
            {
                std::vector<float> both; both.insert (both.end(), x.begin(), x.end()); both.insert (both.end(), L.begin(), L.end());
                auto wav = encodeWav16 (resampleTo (both, SR, 48000.0), 48000);
                std::ofstream (argv[1], std::ios::binary).write ((const char*) wav.data(), (std::streamsize) wav.size());
            }
        }
    }
    std::printf ("%s\n", fails ? "AFINA: FALHOU" : "AFINA: TODOS OK");
    return fails;
}
