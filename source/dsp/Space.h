// =============================================================================
//  CUPIDO DSP — Espaço: DOBRA (doubler de até 4 vozes com detune real e
//  variação de timing), ECO (delay sincronizado com ducking, fita e filtros)
//  e ESPAÇO (reverb FDN 8 linhas com difusão, modulação e ducking)
// =============================================================================
#pragma once
#include "Core.h"
#include "Pitch.h"

namespace cupido
{
// -----------------------------------------------------------------------------
//  Doubler: cada voz = pitch-shift fino (± cents) + atraso modulado + pan
// -----------------------------------------------------------------------------
class Doubler
{
public:
    static constexpr int kMaxVoices = 4;
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (int v = 0; v < kMaxVoices; ++v)
        {
            shift[v].prepare (sr, 45.0f + 7.0f * (float) v, 1.0f);
            dl[v].prepare ((int) (0.12 * sr));
            rnd[v].prepare (sr, 0.35f + 0.17f * (float) v, 1234u + 77u * (uint32_t) v);
            hp[v].set (BqType::HighPass, sr, 140, 0.707);
        }
        mixS.prepare (sr, 30); mixS.reset (0); reset();
    }
    void reset() { for (int v = 0; v < kMaxVoices; ++v) { shift[v].reset(); dl[v].reset(); hp[v].reset(); } }
    void setParams (int voicesIdx, float detuneCents, float delayMs, float widthPct, float modPct, float mixPct)
    {
        voices = voicesIdx == 0 ? 2 : 4; cents = detuneCents; baseDelay = (float) (delayMs * 0.001 * sr);
        width = widthPct * 0.01f; mod = modPct * 0.01f; mixS.set (mixPct * 0.01f);
    }
    void process (float* L, float* R, int n)
    {
        static const float detSign[kMaxVoices]  = { -1.0f, 1.0f, 0.62f, -0.55f };
        static const float delMul[kMaxVoices]   = { 1.0f, 1.37f, 0.71f, 1.85f };
        static const float panPos[kMaxVoices]   = { -1.0f, 1.0f, -0.55f, 0.55f };
        float ratio[kMaxVoices], gl[kMaxVoices], gr[kMaxVoices];
        for (int v = 0; v < kMaxVoices; ++v)
        {
            ratio[v] = std::pow (2.0f, cents * detSign[v] / 1200.0f);
            const float p = panPos[v] * width;                         // pan de potência constante
            const float ang = (p + 1.0f) * 0.25f * kPi;
            gl[v] = std::cos (ang); gr[v] = std::sin (ang);
        }
        const float norm = voices == 2 ? 0.75f : 0.55f;
        for (int i = 0; i < n; ++i)
        {
            const float mono = R ? 0.5f * (L[i] + R[i]) : L[i];
            const float mix = mixS.next();
            float ol = 0, orr = 0;
            for (int v = 0; v < voices; ++v)
            {
                const float s = shift[v].process (hp[v].process (mono), ratio[v]);
                dl[v].push (s);
                const float d = baseDelay * delMul[v] * (1.0f + 0.35f * mod * rnd[v].next());
                const float y = dl[v].read (std::max (2.0f, d));
                ol += y * gl[v]; orr += y * gr[v];
            }
            L[i] += ol * norm * mix * 1.4f;
            if (R) R[i] += orr * norm * mix * 1.4f;
        }
    }
private:
    double sr = 48000; int voices = 2; float cents = 9, baseDelay = 700, width = 0.85f, mod = 0.35f; Smoother mixS;
    GranularShifter shift[kMaxVoices]; DelayLine dl[kMaxVoices]; SmoothRandom rnd[kMaxVoices]; Biquad hp[kMaxVoices];
};

/** Duração (em beats) de cada nota do seletor do delay. */
inline float noteToBeats (int idx)
{
    static const float b[12] = { 0.125f, 0.25f * 2.0f / 3.0f, 0.25f, 0.375f, 0.5f * 2.0f / 3.0f, 0.5f, 0.75f,
                                 2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 4.0f };
    return b[std::min (std::max (idx, 0), 11)];
}

// -----------------------------------------------------------------------------
//  Delay estéreo / ping-pong / mono / slap
// -----------------------------------------------------------------------------
class Delay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate; for (auto& d : dl) d.prepare ((int) (4.2 * sr));
        timeS.prepare (sr, 180); timeS.reset ((float) (0.375 * sr)); mixS.prepare (sr, 30); mixS.reset (0);
        wow.prepare (sr, 0.6f, 99u); flutter.prepare (sr, 6.5f, 7u);
        envAtk = msToCoef (5, sr); envRel = msToCoef (280, sr); duckS.prepare (sr, 60); duckS.reset (1);
        last.lc = -1; reset();
    }
    void reset() { for (auto& d : dl) d.reset(); for (auto& b : lp) b.reset(); for (auto& b : hp) b.reset(); fb[0] = fb[1] = 0; env = 0; }

    struct Settings { bool sync; int note; float timeMs, feedback, lc, hc, duck, mod, drive, width, mix; int mode; double bpm; };

    void setParams (const Settings& s)
    {
        mode = s.mode;
        float seconds = s.sync ? noteToBeats (s.note) * 60.0f / (float) std::max (30.0, s.bpm) : s.timeMs * 0.001f;
        if (mode == 3 && s.sync) seconds = std::min (seconds, 0.14f); // slap: sempre curtinho
        timeS.set ((float) clampf (seconds, 0.001f, 4.0f) * (float) sr);
        feedback = s.feedback * 0.01f; if (mode == 3) feedback = std::min (feedback, 0.15f);
        duck = s.duck * 0.01f; modAmt = s.mod * 0.01f; drive = s.drive * 0.01f; width = s.width * 0.01f; mixS.set (s.mix * 0.01f);
        if (s.lc != last.lc || s.hc != last.hc)
            for (int c = 0; c < 2; ++c) { hp[c].set (BqType::HighPass, sr, s.lc, 0.6); lp[c].set (BqType::LowPass, sr, std::min ((double) s.hc, sr * 0.45), 0.6); }
        last = s;
    }

    void process (float* L, float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R ? R[i] : l;
            const float a = std::max (std::abs (l), std::abs (r));
            env = a > env ? a + (env - a) * envAtk : a + (env - a) * envRel;
            const float envN = clampf ((gainToDb (env) + 45.0f) / 30.0f, 0.0f, 1.0f);
            duckS.set (1.0f - duck * envN * 0.9f);
            const float dg = duckS.next(), mix = mixS.next();

            const float m = modAmt * (0.004f * wow.next() + 0.0006f * flutter.next()) * (float) sr;
            const float t = timeS.next();
            const float tl = std::max (2.0f, t + m), tr = std::max (2.0f, (mode == 0 ? t * 1.015f : t) + m);
            float yl = dl[0].read (tl), yr = dl[1].read (tr);

            // caminho de realimentação: filtros + saturação de fita
            auto colour = [this] (float x, int c)
            {
                x = lp[c].process (hp[c].process (x));
                return drive > 0.001f ? std::tanh (x * (1.0f + 3.0f * drive)) / (1.0f + 3.0f * drive) * (1.0f + drive) : x;
            };
            const float cl = colour (yl, 0), cr = colour (yr, 1);
            const float inMono = 0.5f * (l + r);
            switch (mode)
            {
                case 1:  dl[0].push (inMono + cr * feedback); dl[1].push (cl * feedback); break;  // ping-pong
                case 2:
                case 3:  dl[0].push (inMono + cl * feedback); dl[1].push (inMono + cl * feedback); break; // mono / slap
                default: dl[0].push (l + cl * feedback); dl[1].push (r + cr * feedback); break;   // estéreo
            }
            float wl = cl, wr = (mode >= 2) ? cl : cr;
            const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
            wl = mid + side; wr = mid - side;
            L[i] = l + wl * mix * dg; if (R) R[i] = r + wr * mix * dg;
        }
    }
private:
    double sr = 48000; DelayLine dl[2]; Biquad lp[2], hp[2]; Smoother timeS, mixS, duckS; SmoothRandom wow, flutter;
    float fb[2] {}, feedback = 0.3f, duck = 0.3f, modAmt = 0.1f, drive = 0.1f, width = 1, env = 0, envAtk = 0, envRel = 0; int mode = 1;
    Settings last {};
};

// -----------------------------------------------------------------------------
//  Reverb FDN 8x8 (Hadamard) com pré-delay, difusores all-pass e modulação
// -----------------------------------------------------------------------------
class Reverb
{
public:
    static constexpr int N = 8;
    void prepare (double sampleRate)
    {
        sr = sampleRate; pre.prepare ((int) (0.36 * sr));
        for (int i = 0; i < N; ++i) { lines[i].prepare ((int) (0.25 * sr)); lfo[i] = (float) i * 0.7f; damp[i].reset(); }
        for (auto& a : ap) a.dl.prepare ((int) (0.03 * sr));
        mixS.prepare (sr, 30); mixS.reset (0); envAtk = msToCoef (8, sr); envRel = msToCoef (350, sr); duckS.prepare (sr, 80); duckS.reset (1);
        lastType = -1; reset();
    }
    void reset() { pre.reset(); for (auto& l : lines) l.reset(); for (auto& a : ap) a.dl.reset(); for (auto& d : damp) d.reset(); for (auto& b : lc) b.reset(); for (auto& b : hc) b.reset(); env = 0; }

    struct Settings { int type; float size, decay, predelay, damp, lowcut, highcut, mod, width, duck, mix, diff = 70, er = 35, bass = 1; };

    void setParams (const Settings& s)
    {
        static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 47.9f, 53.3f, 59.9f, 67.3f };
        static const float typeScale[5] = { 0.55f, 0.8f, 1.45f, 1.0f, 0.32f };  // sala, plate, hall, câmara, ambiente
        static const float typeDiff[5]  = { 0.62f, 0.75f, 0.7f, 0.68f, 0.55f };
        const int t = std::min (std::max (s.type, 0), 4);
        const float scale = typeScale[t] * (0.5f + s.size * 0.01f);
        for (int i = 0; i < N; ++i)
        {
            len[i] = baseMs[i] * 0.001f * (float) sr * scale;
            const float g = std::pow (10.0f, -3.0f * (len[i] / (float) sr) / std::max (0.1f, s.decay));
            fbGain[i] = g;
            fbGainLow[i] = std::pow (10.0f, -3.0f * (len[i] / (float) sr) / std::max (0.1f, s.decay * s.bass));
            xover[i].setCutoff (sr, 250.0);
            const float dampHz = lerp (16000.0f, 2500.0f, s.damp * 0.01f) * (t == 1 ? 1.25f : 1.0f);
            damp[i].setCutoff (sr, dampHz);
        }
        diff = clampf (typeDiff[t] * (s.diff / 70.0f), 0.05f, 0.85f);
        erGain = s.er * 0.01f;
        static const float apMs[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
        for (int k = 0; k < 4; ++k) ap[k].len = apMs[k] * 0.001f * (float) sr * (t == 4 ? 0.6f : 1.0f);
        preSamples = std::max (1.0f, s.predelay * 0.001f * (float) sr);
        erScale = scale;
        modDepth = s.mod * 0.01f * 0.0012f * (float) sr;
        if (s.lowcut != last.lowcut || s.highcut != last.highcut)
            for (int c = 0; c < 2; ++c) { lc[c].set (BqType::HighPass, sr, s.lowcut, 0.6); hc[c].set (BqType::LowPass, sr, std::min ((double) s.highcut, sr * 0.45), 0.6); }
        width = s.width * 0.01f; duck = s.duck * 0.01f; mixS.set (s.mix * 0.01f); last = s;
        lfoInc = kTwoPi * 0.35f / (float) sr;
    }

    void process (float* L, float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R ? R[i] : l;
            const float a = std::max (std::abs (l), std::abs (r));
            env = a > env ? a + (env - a) * envAtk : a + (env - a) * envRel;
            duckS.set (1.0f - duck * clampf ((gainToDb (env) + 45.0f) / 30.0f, 0.0f, 1.0f) * 0.9f);
            const float dg = duckS.next(), mix = mixS.next();

            pre.push (0.5f * (l + r));
            float x = pre.read (preSamples);
            for (auto& d : ap) x = d.process (x, diff);

            float y[N];
            for (int k = 0; k < N; ++k)
            {
                lfo[k] += lfoInc * (1.0f + 0.13f * (float) k); if (lfo[k] > kTwoPi) lfo[k] -= kTwoPi;
                y[k] = lines[k].read (len[k] + modDepth * std::sin (lfo[k]));
            }
            // Hadamard 8x8 rápido, normalizado
            float h[N]; std::copy (y, y + N, h);
            for (int s = 1; s < N; s <<= 1)
                for (int k = 0; k < N; k += s << 1)
                    for (int j = k; j < k + s; ++j) { const float u = h[j], v = h[j + s]; h[j] = u + v; h[j + s] = u - v; }
            const float nrm = 0.35355339f;
            for (int k = 0; k < N; ++k)
            {
                const float v = h[k] * nrm; const float lo = xover[k].lp (v);
                lines[k].push (lo * fbGainLow[k] + damp[k].lp ((v - lo) * fbGain[k]) + x * ((k & 1) ? -0.5f : 0.5f));
            }
            // primeiras reflexões: taps do pré-delay (padrão assimétrico L/R)
            static const float erMs[6] = { 7.3f, 11.9f, 17.1f, 23.3f, 29.7f, 37.1f };
            float erL = 0, erR = 0;
            for (int k = 0; k < 6; ++k)
            {
                const float tap = pre.read (preSamples + erMs[k] * 0.001f * (float) sr * erScale) * (1.0f - (float) k * 0.13f);
                if (k & 1) erR += tap; else erL += tap;
            }
            float wl = (y[0] - y[2] + y[4] - y[6]) * 0.5f + erL * erGain * 0.6f, wr = (y[1] - y[3] + y[5] - y[7]) * 0.5f + erR * erGain * 0.6f;
            wl = hc[0].process (lc[0].process (wl)); wr = hc[1].process (lc[1].process (wr));
            const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
            wl = mid + side; wr = mid - side;
            L[i] = l + wl * mix * dg; if (R) R[i] = r + wr * mix * dg;
        }
    }
private:
    struct AllPass
    {
        DelayLine dl; float len = 100;
        inline float process (float x, float g)
        { const float d = dl.read (len); const float v = x + g * d; dl.push (v); return d - g * v; }
    };
    double sr = 48000; DelayLine pre, lines[N]; AllPass ap[4]; OnePole damp[N]; Biquad lc[2], hc[2];
    float len[N] {}, fbGain[N] {}, fbGainLow[N] {}, erGain = 0.35f, erScale = 1.0f; OnePole xover[N]; float lfo[N] {}, lfoInc = 0, modDepth = 0, preSamples = 1, diff = 0.7f, width = 1, duck = 0, env = 0, envAtk = 0, envRel = 0;
    Smoother mixS, duckS; Settings last {}; int lastType = -1;
};
} // namespace cupido
