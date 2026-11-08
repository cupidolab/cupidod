// Gera beats sintéticos de estilos diferentes e imprime o "DNA" (features) que o CUPIDO extrai.
#define _USE_MATH_DEFINES
#include "../source/dsp/Analysis.h"
#include <cstdio>
using namespace cupido;
static const double SR = 44100;

struct Beat { const char* name; double bpm; bool tripletHats; bool long808; bool offbeatSkank; bool fourFloor; double hatDiv; };

static std::vector<float> make (const Beat& b, double secs = 30)
{
    const size_t N = (size_t) (SR * secs); std::vector<float> x (N, 0.0f);
    const double beat = 60.0 / b.bpm * SR; Rng rng (11);
    auto addKick = [&] (double at) { for (size_t i = 0; i < (size_t) (SR * 0.25) && at + i < N; ++i) { const double t = i / SR; x[(size_t) at + i] += (float) (0.8 * std::exp (-t * 14) * std::sin (2 * M_PI * (45 + 110 * std::exp (-t * 35)) * t)); } };
    auto add808 = [&] (double at, double len, double f0) { for (size_t i = 0; i < (size_t) len && at + i < N; ++i) { const double t = i / SR; x[(size_t) at + i] += (float) (0.5 * std::exp (-t * 1.2) * std::sin (2 * M_PI * f0 * t)); } };
    auto addHat = [&] (double at) { for (size_t i = 0; i < (size_t) (SR * 0.04) && at + i < N; ++i) { const double t = i / SR; x[(size_t) at + i] += (float) (0.12 * std::exp (-t * 120) * rng.next()); } };
    auto addSnare = [&] (double at) { for (size_t i = 0; i < (size_t) (SR * 0.2) && at + i < N; ++i) { const double t = i / SR; x[(size_t) at + i] += (float) (0.35 * std::exp (-t * 22) * (rng.next() * 0.7 + 0.3 * std::sin (2 * M_PI * 190 * t))); } };
    auto addSkank = [&] (double at) { for (size_t i = 0; i < (size_t) (SR * 0.12) && at + i < N; ++i) { const double t = i / SR; double s = 0; for (double f : { 523.0, 659.0, 784.0 }) s += std::sin (2 * M_PI * f * t); x[(size_t) at + i] += (float) (0.12 * std::exp (-t * 25) * s); } };
    const int beats = (int) (N / beat);
    for (int k = 0; k < beats; ++k)
    {
        const double at = k * beat;
        if (b.fourFloor || k % 4 == 0 || (k % 4 == 2 && b.bpm < 100) || (!b.fourFloor && k % 8 == 5)) addKick (at);
        if (b.long808 && k % 4 == 0) add808 (at, beat * 3.5, 49);
        if ((b.bpm > 110 ? k % 4 == 2 : k % 2 == 1)) addSnare (at);
        if (b.offbeatSkank) addSkank (at + beat * 0.5);
        const double div = b.tripletHats ? (k % 2 ? 3.0 : 2.0) : b.hatDiv;
        for (int h = 0; h < (int) div; ++h) addHat (at + beat * h / div);
    }
    // pad
    for (size_t i = 0; i < N; ++i) x[i] += 0.03f * (float) (std::sin (2 * M_PI * 220 * i / SR) + std::sin (2 * M_PI * 261.6 * i / SR) + std::sin (2 * M_PI * 329.6 * i / SR));
    return x;
}

int main()
{
    const Beat beats[] = {
        { "trap 140",      140, true,  true,  false, false, 4 },
        { "drill 142",     142, true,  true,  false, false, 3 },
        { "funk 130",      130, false, false, false, true,  4 },
        { "reggae 75",      75, false, false, true,  false, 2 },
        { "rnb 70",         70, false, false, false, false, 2 },
        { "pop 118",       118, false, false, false, true,  2 },
    };
    for (auto& b : beats)
    {
        auto x = make (b); auto f = analyseTrack (x, {}, SR);
        std::printf ("%-10s bpm %6.1f key %s%s lufs %5.1f crest %4.1f sub %.2f hats/s %5.1f kicks/s %4.1f trip %.2f/%.2f offbeat %.2f/%.2f cent %5.0f\n",
                     b.name, f.bpm, keyName (f.key), f.minor ? "m" : "", f.lufs, f.crestDb, f.subSustain, f.hatRate, f.kickRate, f.tripletRatio, f.tripletRatioHalf, f.offbeatRatio, f.offbeatRatioHalf, f.centroidHz);
    }
}
