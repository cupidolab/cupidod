// =============================================================================
//  CUPIDO DSP — Cadeia completa
//
//  Entrada -> LIMPA (gate + redução de ruído) -> AFINA (autotune) -> EQ ->
//  SIBILA (de-esser) -> PUNCH (FET/VCA) -> OPTO -> PRISMA (saturação) ->
//  AR (excitador) -> DOBRA (doubler) -> ECO (delay) -> ESPAÇO (reverb) ->
//  MASTER (glue, tilt, imagem, limiter) -> Saída
//
//  A ordem segue a prática de mixagem vocal: limpar antes de afinar (o detector
//  de pitch não "pega" ruído), corrigir tonalidade antes de colorir, de-esser
//  antes da compressão forte (S não dispara o compressor), efeitos de tempo no
//  fim e o limiter como último estágio.
// =============================================================================
#pragma once
#include "../Params.h"
#include "Core.h"
#include "Dynamics.h"
#include "Tone.h"
#include "Pitch.h"
#include "Space.h"
#include "Spectral.h"
#include "Analysis.h"

namespace cupido
{
/** Telemetria lida pela UI ~30x/s (escrita no áudio, lida em outra thread: atômicos). */
struct Meters
{
    std::atomic<float> inL { 0 }, inR { 0 }, outL { 0 }, outR { 0 };
    std::atomic<float> grGate { 0 }, grDeess { 0 }, grComp { 0 }, grOpto { 0 }, grGlue { 0 }, grLim { 0 };
    std::atomic<float> pitchHz { 0 }, pitchMidi { 0 }, targetMidi { 0 }, correction { 0 };
    std::atomic<float> lufsM { -70 }, lufsS { -70 }, lufsI { -70 };
    std::atomic<bool>  sidechainActive { false };
};

class VocalChain
{
public:
    static constexpr double kCaptureRate = 12000.0;   // captura longa decimada (BPM/tom)
    static constexpr int    kLongSeconds = 16;
    static constexpr int    kShortSamples = 1 << 16; // ~1.4 s em taxa cheia (espectro)

    void prepare (double sampleRate, int maxBlock, bool lowLatency_)
    {
        sr = sampleRate; lowLatency = lowLatency_;
        tmpL.assign ((size_t) maxBlock, 0.0f); tmpR.assign ((size_t) maxBlock, 0.0f);
        dryL.assign ((size_t) maxBlock, 0.0f); dryR.assign ((size_t) maxBlock, 0.0f);
        gate.prepare (sr); nr.prepare (sr); tune.prepare (sr, lowLatency); eq.prepare (sr); deess.prepare (sr);
        comp.prepare (sr); opto.prepare (sr); prisma.prepare (sr, maxBlock); air.prepare (sr); dbl.prepare (sr);
        for (auto& a : ag) a.prepare (sr, maxBlock);
        delay.prepare (sr); reverb.prepare (sr); glue.prepare (sr); tilt.prepare (sr); imager.prepare (sr);
        limiter.prepare (sr, lowLatency ? 16 : (int) (0.0015 * sr)); loud.prepare (sr);
        for (auto* m : { &pmInL, &pmInR, &pmOutL, &pmOutR }) m->prepare (sr);
        inS.prepare (sr, 20); outS.prepare (sr, 20); mixS.prepare (sr, 20);
        // latência total constante p/ o host compensar; o dry do "Mix Geral" é atrasado igual
        latencySamples = (lowLatency ? 0 : nr.latency()) + tune.latency() + limiter.latency();
        dryDelayL.prepare (latencySamples + 8); dryDelayR.prepare (latencySamples + 8);
        // captura p/ análise (voz = entrada principal; beat = sidechain)
        vocalLong.prepare ((int) (kCaptureRate * kLongSeconds)); beatLong.prepare ((int) (kCaptureRate * kLongSeconds));
        vocalShort.prepare (kShortSamples); beatShort.prepare (kShortSamples); outShort.prepare (1 << 12);
        decV.prepare (sr, kCaptureRate); decB.prepare (sr, kCaptureRate);
        inS.reset (1); outS.reset (1); mixS.reset (1);
    }

    int  getLatency() const { return latencySamples; }
    bool isLowLatency() const { return lowLatency; }

    void reset()
    {
        gate.reset(); nr.reset(); tune.reset(); eq.reset(); deess.reset(); comp.reset(); opto.reset(); prisma.reset();
        air.reset(); dbl.reset(); delay.reset(); reverb.reset(); glue.reset(); limiter.reset(); loud.reset();
        dryDelayL.reset(); dryDelayR.reset();
    }

    /** Atualiza todos os módulos a partir dos valores dos parâmetros (1x por bloco). */
    void setParams (const ParamValues& p, double hostBpm)
    {
        pv = p; bpm = hostBpm > 0 ? hostBpm : (analysedBpm > 0 ? analysedBpm : 120.0);
        inS.set (dbToGain (p[P::InGain])); outS.set (dbToGain (p[P::OutGain])); mixS.set (p[P::Mix] * 0.01f);
        gate.setParams (p[P::GateThresh], p[P::GateRange], p[P::GateAttack], p[P::GateRelease]);
        nr.setParams (p[P::NrAmount], p[P::NrSensitivity], p.on (P::NrAdaptive));
        tune.setParams (p.idx (P::TuneKey), p.idx (P::TuneScale), p[P::TuneSpeed], p[P::TuneHumanize], p[P::TuneAmount], p[P::TuneTranspose]);
        eq.setParams ({ p[P::EqHpf], p[P::EqLowFreq], p[P::EqLowGain], p[P::EqMid1Freq], p[P::EqMid1Gain], p[P::EqMid1Q],
                        p[P::EqMid2Freq], p[P::EqMid2Gain], p[P::EqMid2Q], p[P::EqHighFreq], p[P::EqHighGain], p[P::EqLpf] });
        deess.setParams (p[P::DsFreq], p[P::DsThresh], p[P::DsRange], p.idx (P::DsMode), p.on (P::DsListen));
        comp.setParams (p[P::CompThresh], p[P::CompRatio], p[P::CompAttack], p[P::CompRelease], p[P::CompKnee], p[P::CompMakeup], p.idx (P::CompChar), p[P::CompMix]);
        opto.setParams (p[P::OptoPeak], p[P::OptoGain], p[P::OptoMix]);
        const float f[5] = { p[P::SatB1Freq], p[P::SatB2Freq], p[P::SatB3Freq], p[P::SatB4Freq], p[P::SatB5Freq] };
        const float a[5] = { p[P::SatB1Amt], p[P::SatB2Amt], p[P::SatB3Amt], p[P::SatB4Amt], p[P::SatB5Amt] };
        prisma.setParams (p.idx (P::SatType), p[P::SatDrive], f, a, p[P::SatMix], p[P::SatOut]);
        air.setParams (p[P::AirMid], p[P::AirHigh], p[P::AirTrim]);
        dbl.setParams (p.idx (P::DblVoices), p[P::DblDetune], p[P::DblDelay], p[P::DblWidth], p[P::DblMod], p[P::DblMix]);
        delay.setParams ({ p.on (P::DlySync), p.idx (P::DlyNote), p[P::DlyTime], p[P::DlyFeedback], p[P::DlyLowCut], p[P::DlyHighCut],
                           p[P::DlyDuck], p[P::DlyMod], p[P::DlyDrive], p[P::DlyWidth], p[P::DlyMix], p.idx (P::DlyMode), bpm });
        reverb.setParams ({ p.idx (P::RevType), p[P::RevSize], p[P::RevDecay], p[P::RevPredelay], p[P::RevDamp], p[P::RevLowCut],
                            p[P::RevHighCut], p[P::RevMod], p[P::RevWidth], p[P::RevDuck], p[P::RevMix],
                            p[P::RevDiff], p[P::RevEarly], p[P::RevBass] });
        glue.setParams (p[P::GlueThresh], p[P::GlueRatio], p[P::GlueRelease]);
        tilt.setParams (p[P::Tilt]); imager.setParams (p[P::ImgWidth], p.on (P::ImgMonoBass));
        limiter.setParams (p[P::LimGain], p[P::LimCeiling], p[P::LimRelease]);
    }

    /** L/R: canais principais (R pode ser nulo p/ mono). scL/scR: sidechain (beat) ou nulos. */
    void process (float* L, float* R, const float* scL, const float* scR, int n)
    {
        // ganho de entrada + captura + meters
        for (int i = 0; i < n; ++i)
        {
            const float g = inS.next(); L[i] *= g; if (R) R[i] *= g;
            pmInL.push (L[i]); pmInR.push (R ? R[i] : L[i]);
            const float mono = R ? 0.5f * (L[i] + R[i]) : L[i];
            float d; if (decV.push (mono, d)) vocalLong.push (d);
            vocalShort.push (mono);
            dryL[(size_t) i] = L[i]; dryR[(size_t) i] = R ? R[i] : L[i];
        }
        bool scActive = false;
        if (scL)
        {
            float scPeak = 0;
            for (int i = 0; i < n; ++i)
            {
                const float m = scR ? 0.5f * (scL[i] + scR[i]) : scL[i]; scPeak = std::max (scPeak, std::abs (m));
                float d; if (decB.push (m, d)) beatLong.push (d);
                beatShort.push (m);
            }
            scActive = scPeak > 1.0e-4f;
        }
        meters.sidechainActive.store (scActive);

        // ---- cadeia ----
        if (pv.on (P::GateOn)) meters.grGate.store (gate.process (L, R, n)); else meters.grGate.store (0);
        if (! lowLatency) nr.process (L, R, n, pv.on (P::NrOn));
        tune.process (L, R, n, pv.on (P::TuneOn));
        const bool autoG = pv.on (P::AutoGain);
        // cada módulo de cor/dinâmica tem casamento de volume (quando "Ganho Automático" está ligado)
        auto colour = [&] (int idx, bool on, auto&& fn) -> float
        {
            if (! on) return 0.0f;
            ag[idx].measureIn (L, R, n); const float r = fn(); ag[idx].apply (L, R, n, autoG); return r;
        };
        colour (0, pv.on (P::EqOn), [&] { eq.process (L, R, n); return 0.0f; });
        meters.grDeess.store (pv.on (P::DsOn) ? deess.process (L, R, n) : 0.0f);
        meters.grComp.store (colour (1, pv.on (P::CompOn), [&] { return comp.process (L, R, n); }));
        meters.grOpto.store (colour (2, pv.on (P::OptoOn), [&] { return opto.process (L, R, n); }));
        colour (3, pv.on (P::SatOn), [&] { prisma.process (L, R, n); return 0.0f; });
        colour (4, pv.on (P::AirOn), [&] { air.process (L, R, n); return 0.0f; });
        // a partir daqui o sinal precisa ser estéreo (doubler/efeitos abrem a imagem)
        float* RR = R ? R : tmpR.data();
        if (! R) std::copy (L, L + n, RR);
        if (pv.on (P::DblOn)) dbl.process (L, RR, n);
        if (pv.on (P::DlyOn)) delay.process (L, RR, n);
        if (pv.on (P::RevOn)) reverb.process (L, RR, n);
        meters.grGlue.store (pv.on (P::GlueOn) ? glue.process (L, RR, n) : 0.0f);
        tilt.process (L, RR, n); imager.process (L, RR, n);
        meters.grLim.store (pv.on (P::LimOn) ? limiter.process (L, RR, n) : passLimiterDelay (L, RR, n));

        // mix geral (dry alinhado pela latência) + saída
        for (int i = 0; i < n; ++i)
        {
            dryDelayL.push (dryL[(size_t) i]); dryDelayR.push (dryR[(size_t) i]);
            const float dl = latencySamples > 0 ? dryDelayL.readInt (latencySamples) : dryL[(size_t) i];
            const float dr = latencySamples > 0 ? dryDelayR.readInt (latencySamples) : dryR[(size_t) i];
            const float m = mixS.next(), g = outS.next();
            L[i] = (dl + (L[i] - dl) * m) * g; RR[i] = (dr + (RR[i] - dr) * m) * g;
            if (! std::isfinite (L[i]))  L[i] = 0;
            if (! std::isfinite (RR[i])) RR[i] = 0;
            pmOutL.push (L[i]); pmOutR.push (RR[i]); outShort.push (0.5f * (L[i] + RR[i]));
        }
        if (! R) for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + RR[i]);   // saída mono: soma
        loud.process (L, R ? RR : nullptr, n);

        meters.inL.store (pmInL.value); meters.inR.store (pmInR.value); meters.outL.store (pmOutL.value); meters.outR.store (pmOutR.value);
        meters.pitchHz.store (tune.detectedHz()); meters.pitchMidi.store (tune.detectedMidi());
        meters.targetMidi.store (tune.targetMidi()); meters.correction.store (tune.correctionSt());
        meters.lufsM.store (loud.momentary); meters.lufsS.store (loud.shortTerm); meters.lufsI.store (loud.integrated);
    }

    // ---- ações vindas da UI ----
    void learnNoise (float seconds = 2.0f) { nr.startLearning (seconds); }
    bool isLearningNoise() const { return nr.isLearning(); }
    bool hasNoiseProfile() const { return nr.hasProfile(); }
    void resetLoudness() { loud.reset(); }
    void setAnalysedBpm (double b) { analysedBpm = b; }

    /** Análise (chamar fora da thread de áudio). beat=true analisa o sidechain. */
    AnalysisResult analyse (bool beat) const
    {
        const auto& lr = beat ? beatLong : vocalLong; const auto& sr_ = beat ? beatShort : vocalShort;
        auto longSnap = lr.snapshot ((int) (kCaptureRate * kLongSeconds));
        auto shortSnap = sr_.snapshot (kShortSamples);
        float chroma[12]; const float* hist = tune.pitchClassHistogram(); float hs = 0;
        for (int i = 0; i < 12; ++i) { chroma[i] = hist[i]; hs += hist[i]; }
        if (hs > 0) for (float& c : chroma) c = c / hs * 50.0f;
        return analyseAll (longSnap, kCaptureRate, shortSnap, sr, (! beat && hs > 0) ? chroma : nullptr);
    }
    std::vector<float> outputSnapshot() const { return outShort.snapshot (1 << 12); }
    std::vector<float> beatSnapshot() const   { return beatShort.snapshot (1 << 12); }
    double sampleRate() const { return sr; }
    double currentBpm() const { return bpm; }

    Meters meters;

private:
    float passLimiterDelay (float* L, float* R, int n)
    {   // limiter desligado: mantém a latência (pass-through com mesmo atraso)
        limiter.setParams (0.0f, 60.0f, 50.0f); limiter.process (L, R, n);
        limiter.setParams (pv[P::LimGain], pv[P::LimCeiling], pv[P::LimRelease]);
        return 0.0f;
    }

    double sr = 48000, bpm = 120, analysedBpm = 0; bool lowLatency = false; int latencySamples = 0;
    ParamValues pv;
    Gate gate; NoiseReducer nr; PitchCorrector tune; Equalizer eq; DeEsser deess; Compressor comp; OptoCompressor opto;
    Prisma prisma; AirExciter air; Doubler dbl; Delay delay; Reverb reverb; GlueCompressor glue; Tilt tilt; Imager imager;
    Limiter limiter; LoudnessMeter loud; AutoGain ag[5]; PeakMeter pmInL, pmInR, pmOutL, pmOutR; Smoother inS, outS, mixS;
    DelayLine dryDelayL, dryDelayR; std::vector<float> tmpL, tmpR, dryL, dryR;
    CaptureRing vocalLong, beatLong, vocalShort, beatShort, outShort; Decimator decV, decB;
};
} // namespace cupido
