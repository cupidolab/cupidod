// =============================================================================
//  CUPIDO — Tabela única de parâmetros
//  Toda a lista de parâmetros vive aqui (X-macro). O processador JUCE, o motor
//  DSP e a interface (via getSpec) leem desta tabela. Para adicionar um
//  parâmetro: adicione uma linha aqui e rode tools/gen_params (atualiza ui/params.js).
//
//  Campos: ENUM, id, rótulo, módulo, min, max, padrão, passo, skew, unidade, opções
//    passo: 0 = contínuo, 1 = inteiro/escolha/liga-desliga
//    skew : 0 = linear, 1 = logarítmico (frequência/tempo)
//    unidade "bool" = liga/desliga, "choice" = lista (opções separadas por |)
// =============================================================================
#pragma once

#define CUPIDO_PARAMS(X) \
  /* ---------- Entrada / Saída / Global ---------- */ \
  X(InGain,        "in_gain",        "Entrada",          "io",     -24,   24,    0,     0, 0, "dB",   "") \
  X(OutGain,       "out_gain",       "Saída",            "io",     -24,   24,    0,     0, 0, "dB",   "") \
  X(Mix,           "mix",            "Mix Geral",        "io",       0,  100,  100,     0, 0, "%",    "") \
  X(LatencyMode,   "latency_mode",   "Modo",             "io",       0,    1,    0,     1, 0, "choice","Mixagem HQ|Gravação (baixa latência)") \
  /* ---------- LIMPA: Gate ---------- */ \
  X(GateOn,        "gate_on",        "Gate",             "gate",     0,    1,    0,     1, 0, "bool", "") \
  X(GateThresh,    "gate_thresh",    "Threshold",        "gate",   -80,    0,  -50,     0, 0, "dB",   "") \
  X(GateRange,     "gate_range",     "Redução",          "gate",     0,   60,   18,     0, 0, "dB",   "") \
  X(GateAttack,    "gate_attack",    "Ataque",           "gate",   0.1,   50,    2,     0, 1, "ms",   "") \
  X(GateRelease,   "gate_release",   "Release",          "gate",    10, 1000,  120,     0, 1, "ms",   "") \
  /* ---------- LIMPA: Redução de ruído espectral ---------- */ \
  X(NrOn,          "nr_on",          "Redução de Ruído", "nr",       0,    1,    0,     1, 0, "bool", "") \
  X(NrAmount,      "nr_amount",      "Quantidade",       "nr",       0,  100,   50,     0, 0, "%",    "") \
  X(NrSensitivity, "nr_sens",        "Sensibilidade",    "nr",       0,  100,   50,     0, 0, "%",    "") \
  X(NrAdaptive,    "nr_adaptive",    "Adaptativo",       "nr",       0,    1,    1,     1, 0, "bool", "") \
  /* ---------- AFINA: Autotune ---------- */ \
  X(TuneOn,        "tune_on",        "Afinação",         "tune",     0,    1,    0,     1, 0, "bool", "") \
  X(TuneKey,       "tune_key",       "Tom",              "tune",     0,   11,    0,     1, 0, "choice","C|C#|D|D#|E|F|F#|G|G#|A|A#|B") \
  X(TuneScale,     "tune_scale",     "Escala",           "tune",     0,    7,    0,     1, 0, "choice","Cromática|Maior|Menor|Menor Harmônica|Pentatônica Menor|Pentatônica Maior|Dórico|Frígio") \
  X(TuneSpeed,     "tune_speed",     "Velocidade",       "tune",     0,  200,   20,     0, 0, "ms",   "") \
  X(TuneHumanize,  "tune_humanize",  "Humanizar",        "tune",     0,  100,    0,     0, 0, "%",    "") \
  X(TuneAmount,    "tune_amount",    "Intensidade",      "tune",     0,  100,  100,     0, 0, "%",    "") \
  X(TuneTranspose, "tune_transpose", "Transpor",         "tune",   -12,   12,    0,     1, 0, "st",   "") \
  /* ---------- EQ ---------- */ \
  X(EqOn,          "eq_on",          "EQ",               "eq",       0,    1,    1,     1, 0, "bool", "") \
  X(EqHpf,         "eq_hpf",         "Passa-Altas",      "eq",      20,  500,   80,     0, 1, "Hz",   "") \
  X(EqLowFreq,     "eq_low_freq",    "Grave Freq",       "eq",      40,  500,  150,     0, 1, "Hz",   "") \
  X(EqLowGain,     "eq_low_gain",    "Grave",            "eq",     -18,   18,    0,     0, 0, "dB",   "") \
  X(EqMid1Freq,    "eq_m1_freq",     "Médio 1 Freq",     "eq",     100, 2500,  350,     0, 1, "Hz",   "") \
  X(EqMid1Gain,    "eq_m1_gain",     "Médio 1",          "eq",     -18,   18,    0,     0, 0, "dB",   "") \
  X(EqMid1Q,       "eq_m1_q",        "Médio 1 Q",        "eq",     0.3,    8,    1,     0, 1, "",     "") \
  X(EqMid2Freq,    "eq_m2_freq",     "Médio 2 Freq",     "eq",     600, 9000, 3000,     0, 1, "Hz",   "") \
  X(EqMid2Gain,    "eq_m2_gain",     "Médio 2",          "eq",     -18,   18,    0,     0, 0, "dB",   "") \
  X(EqMid2Q,       "eq_m2_q",        "Médio 2 Q",        "eq",     0.3,    8,    1,     0, 1, "",     "") \
  X(EqHighFreq,    "eq_high_freq",   "Agudo Freq",       "eq",    3000,18000,10000,     0, 1, "Hz",   "") \
  X(EqHighGain,    "eq_high_gain",   "Agudo",            "eq",     -18,   18,    0,     0, 0, "dB",   "") \
  X(EqLpf,         "eq_lpf",         "Passa-Baixas",     "eq",    2000,22000,22000,     0, 1, "Hz",   "") \
  /* ---------- SIBILA: De-esser ---------- */ \
  X(DsOn,          "ds_on",          "De-Esser",         "deess",    0,    1,    1,     1, 0, "bool", "") \
  X(DsFreq,        "ds_freq",        "Frequência",       "deess", 2500,14000, 6500,     0, 1, "Hz",   "") \
  X(DsThresh,      "ds_thresh",      "Threshold",        "deess",  -60,    0,  -28,     0, 0, "dB",   "") \
  X(DsRange,       "ds_range",       "Redução Máx",      "deess",    0,   24,    8,     0, 0, "dB",   "") \
  X(DsMode,        "ds_mode",        "Modo",             "deess",    0,    1,    0,     1, 0, "choice","Split (só agudos)|Wide (sinal todo)") \
  X(DsListen,      "ds_listen",      "Ouvir S",          "deess",    0,    1,    0,     1, 0, "bool", "") \
  /* ---------- PUNCH: Compressor FET/VCA ---------- */ \
  X(CompOn,        "comp_on",        "Compressor",       "comp",     0,    1,    1,     1, 0, "bool", "") \
  X(CompThresh,    "comp_thresh",    "Threshold",        "comp",   -60,    0,  -18,     0, 0, "dB",   "") \
  X(CompRatio,     "comp_ratio",     "Ratio",            "comp",     1,   20,    4,     0, 1, ":1",   "") \
  X(CompAttack,    "comp_attack",    "Ataque",           "comp",  0.05,  100,    5,     0, 1, "ms",   "") \
  X(CompRelease,   "comp_release",   "Release",          "comp",    10, 1500,   80,     0, 1, "ms",   "") \
  X(CompKnee,      "comp_knee",      "Joelho",           "comp",     0,   18,    6,     0, 0, "dB",   "") \
  X(CompMakeup,    "comp_makeup",    "Makeup",           "comp",   -12,   24,    3,     0, 0, "dB",   "") \
  X(CompChar,      "comp_char",      "Caráter",          "comp",     0,    2,    1,     1, 0, "choice","Limpo|FET (1176)|VCA") \
  X(CompMix,       "comp_mix",       "Paralelo",         "comp",     0,  100,  100,     0, 0, "%",    "") \
  /* ---------- OPTO: Compressor óptico (LA-2A) ---------- */ \
  X(OptoOn,        "opto_on",        "Opto",             "opto",     0,    1,    0,     1, 0, "bool", "") \
  X(OptoPeak,      "opto_peak",      "Peak Reduction",   "opto",     0,  100,   35,     0, 0, "%",    "") \
  X(OptoGain,      "opto_gain",      "Ganho",            "opto",   -12,   24,    2,     0, 0, "dB",   "") \
  X(OptoMix,       "opto_mix",       "Paralelo",         "opto",     0,  100,  100,     0, 0, "%",    "") \
  /* ---------- PRISMA: Saturação multibanda (estilo enhancer paralelo) ---------- */ \
  X(SatOn,         "sat_on",         "Prisma",           "sat",      0,    1,    0,     1, 0, "bool", "") \
  X(SatType,       "sat_type",       "Tipo",             "sat",      0,    5,    0,     1, 0, "choice","Válvula|Fita|Transistor|Quente|Digital|Fuzz") \
  X(SatDrive,      "sat_drive",      "Drive",            "sat",      0,  100,   35,     0, 0, "%",    "") \
  X(SatB1Freq,     "sat_b1_freq",    "Banda 1 Freq",     "sat",     40,  300,   90,     0, 1, "Hz",   "") \
  X(SatB1Amt,      "sat_b1_amt",     "Banda 1",          "sat",      0,   12,    0,     0, 0, "dB",   "") \
  X(SatB2Freq,     "sat_b2_freq",    "Banda 2 Freq",     "sat",    150, 1200,  400,     0, 1, "Hz",   "") \
  X(SatB2Amt,      "sat_b2_amt",     "Banda 2",          "sat",      0,   12,    0,     0, 0, "dB",   "") \
  X(SatB3Freq,     "sat_b3_freq",    "Banda 3 Freq",     "sat",    600, 4000, 1500,     0, 1, "Hz",   "") \
  X(SatB3Amt,      "sat_b3_amt",     "Banda 3",          "sat",      0,   12,    0,     0, 0, "dB",   "") \
  X(SatB4Freq,     "sat_b4_freq",    "Banda 4 Freq",     "sat",   2000, 9000, 4500,     0, 1, "Hz",   "") \
  X(SatB4Amt,      "sat_b4_amt",     "Banda 4",          "sat",      0,   12,    0,     0, 0, "dB",   "") \
  X(SatB5Freq,     "sat_b5_freq",    "Banda 5 Freq",     "sat",   5000,18000,10000,     0, 1, "Hz",   "") \
  X(SatB5Amt,      "sat_b5_amt",     "Banda 5",          "sat",      0,   12,    0,     0, 0, "dB",   "") \
  X(SatMix,        "sat_mix",        "Mix",              "sat",      0,  100,   60,     0, 0, "%",    "") \
  X(SatOut,        "sat_out",        "Saída",            "sat",    -12,   12,    0,     0, 0, "dB",   "") \
  /* ---------- AR: Excitador dinâmico (estilo Dolby-A trick) ---------- */ \
  X(AirOn,         "air_on",         "Ar",               "air",      0,    1,    1,     1, 0, "bool", "") \
  X(AirMid,        "air_mid",        "Mid Air",          "air",      0,  100,   15,     0, 0, "%",    "") \
  X(AirHigh,       "air_high",       "High Air",         "air",      0,  100,   25,     0, 0, "%",    "") \
  X(AirTrim,       "air_trim",       "Trim",             "air",    -12,    6,    0,     0, 0, "dB",   "") \
  /* ---------- DOBRA: Doubler / estéreo ---------- */ \
  X(DblOn,         "dbl_on",         "Doubler",          "dbl",      0,    1,    0,     1, 0, "bool", "") \
  X(DblVoices,     "dbl_voices",     "Vozes",            "dbl",      0,    1,    0,     1, 0, "choice","2 Vozes|4 Vozes") \
  X(DblDetune,     "dbl_detune",     "Detune",           "dbl",      0,   50,   9,      0, 0, "cents","") \
  X(DblDelay,      "dbl_delay",      "Atraso",           "dbl",      3,   60,   16,     0, 1, "ms",   "") \
  X(DblWidth,      "dbl_width",      "Abertura",         "dbl",      0,  100,   85,     0, 0, "%",    "") \
  X(DblMod,        "dbl_mod",        "Variação",         "dbl",      0,  100,   35,     0, 0, "%",    "") \
  X(DblMix,        "dbl_mix",        "Mix",              "dbl",      0,  100,   35,     0, 0, "%",    "") \
  /* ---------- ECO: Delay ---------- */ \
  X(DlyOn,         "dly_on",         "Delay",            "dly",      0,    1,    0,     1, 0, "bool", "") \
  X(DlySync,       "dly_sync",       "Sync BPM",         "dly",      0,    1,    1,     1, 0, "bool", "") \
  X(DlyNote,       "dly_note",       "Nota",             "dly",      0,   11,    6,     1, 0, "choice","1/32|1/16T|1/16|1/16.|1/8T|1/8|1/8.|1/4T|1/4|1/4.|1/2|1/1") \
  X(DlyTime,       "dly_time",       "Tempo",            "dly",      1, 2000,  375,     0, 1, "ms",   "") \
  X(DlyFeedback,   "dly_feedback",   "Repetições",       "dly",      0,   95,   30,     0, 0, "%",    "") \
  X(DlyMode,       "dly_mode",       "Modo",             "dly",      0,    3,    1,     1, 0, "choice","Estéreo|Ping-Pong|Mono|Slap") \
  X(DlyLowCut,     "dly_lowcut",     "Corte Grave",      "dly",     20, 2000,  350,     0, 1, "Hz",   "") \
  X(DlyHighCut,    "dly_highcut",    "Corte Agudo",      "dly",   1000,20000, 7000,     0, 1, "Hz",   "") \
  X(DlyDuck,       "dly_duck",       "Ducking",          "dly",      0,  100,   35,     0, 0, "%",    "") \
  X(DlyMod,        "dly_mod",        "Wow/Flutter",      "dly",      0,  100,   10,     0, 0, "%",    "") \
  X(DlyDrive,      "dly_drive",      "Fita",             "dly",      0,  100,   10,     0, 0, "%",    "") \
  X(DlyWidth,      "dly_width",      "Largura",          "dly",      0,  100,  100,     0, 0, "%",    "") \
  X(DlyMix,        "dly_mix",        "Mix",              "dly",      0,  100,   18,     0, 0, "%",    "") \
  /* ---------- ESPAÇO: Reverb ---------- */ \
  X(RevOn,         "rev_on",         "Reverb",           "rev",      0,    1,    0,     1, 0, "bool", "") \
  X(RevType,       "rev_type",       "Tipo",             "rev",      0,    4,    1,     1, 0, "choice","Sala|Plate|Hall|Câmara|Ambiente") \
  X(RevSize,       "rev_size",       "Tamanho",          "rev",      0,  100,   55,     0, 0, "%",    "") \
  X(RevDecay,      "rev_decay",      "Decay",            "rev",    0.1,   15,  1.8,     0, 1, "s",    "") \
  X(RevPredelay,   "rev_predelay",   "Pré-delay",        "rev",      0,  250,   30,     0, 0, "ms",   "") \
  X(RevDamp,       "rev_damp",       "Amortecer",        "rev",      0,  100,   45,     0, 0, "%",    "") \
  X(RevLowCut,     "rev_lowcut",     "Corte Grave",      "rev",     20, 1000,  250,     0, 1, "Hz",   "") \
  X(RevHighCut,    "rev_highcut",    "Corte Agudo",      "rev",   1000,20000, 9000,     0, 1, "Hz",   "") \
  X(RevMod,        "rev_mod",        "Modulação",        "rev",      0,  100,   25,     0, 0, "%",    "") \
  X(RevWidth,      "rev_width",      "Largura",          "rev",      0,  100,  100,     0, 0, "%",    "") \
  X(RevDuck,       "rev_duck",       "Ducking",          "rev",      0,  100,   15,     0, 0, "%",    "") \
  X(RevMix,        "rev_mix",        "Mix",              "rev",      0,  100,   14,     0, 0, "%",    "") \
  /* ---------- MASTER: Glue, tilt, imagem, limiter ---------- */ \
  X(GlueOn,        "glue_on",        "Glue Comp",        "master",   0,    1,    0,     1, 0, "bool", "") \
  X(GlueThresh,    "glue_thresh",    "Threshold",        "master", -40,    0,  -12,     0, 0, "dB",   "") \
  X(GlueRatio,     "glue_ratio",     "Ratio",            "master",   1,   10,    2,     0, 1, ":1",   "") \
  X(GlueRelease,   "glue_release",   "Release",          "master",  50, 1200,  300,     0, 1, "ms",   "") \
  X(Tilt,          "tilt",           "Tilt EQ",          "master",  -6,    6,    0,     0, 0, "dB",   "") \
  X(ImgWidth,      "img_width",      "Largura Estéreo",  "master",   0,  200,  100,     0, 0, "%",    "") \
  X(ImgMonoBass,   "img_monobass",   "Grave Mono",       "master",   0,    1,    0,     1, 0, "bool", "") \
  X(LimOn,         "lim_on",         "Limiter",          "master",   0,    1,    1,     1, 0, "bool", "") \
  X(LimGain,       "lim_gain",       "Ganho",            "master",   0,   24,    0,     0, 0, "dB",   "") \
  X(LimCeiling,    "lim_ceiling",    "Teto",             "master", -12,    0,   -1,     0, 0, "dB",   "") \
  X(LimRelease,    "lim_release",    "Release",          "master",   1,  500,   60,     0, 1, "ms",   "") \
  X(LufsTarget,    "lufs_target",    "Alvo LUFS",        "master", -24,   -5,  -10,     0, 0, "LUFS", "")

namespace cupido
{
    enum class P : int
    {
       #define CUPIDO_ENUM(e, id, label, mod, mn, mx, df, st, sk, unit, ch) e,
        CUPIDO_PARAMS(CUPIDO_ENUM)
       #undef CUPIDO_ENUM
        Count
    };

    constexpr int kNumParams = (int) P::Count;

    struct ParamInfo
    {
        const char* id; const char* label; const char* module;
        float min, max, def, step; int skew; const char* unit; const char* choices;
    };

    inline const ParamInfo* paramTable()
    {
        static const ParamInfo table[] = {
           #define CUPIDO_INFO(e, id, label, mod, mn, mx, df, st, sk, unit, ch) { id, label, mod, (float) mn, (float) mx, (float) df, (float) st, sk, unit, ch },
            CUPIDO_PARAMS(CUPIDO_INFO)
           #undef CUPIDO_INFO
        };
        return table;
    }

    inline const ParamInfo& info (P p) { return paramTable()[(int) p]; }

    /** Valores "crus" (unidades reais) de todos os parâmetros, lidos uma vez por bloco. */
    struct ParamValues
    {
        float v[kNumParams];
        ParamValues() { for (int i = 0; i < kNumParams; ++i) v[i] = paramTable()[i].def; }
        float operator[] (P p) const { return v[(int) p]; }
        float& operator[] (P p)      { return v[(int) p]; }
        bool  on (P p) const         { return v[(int) p] > 0.5f; }
        int   idx (P p) const        { return (int) (v[(int) p] + 0.5f); }
    };
}
