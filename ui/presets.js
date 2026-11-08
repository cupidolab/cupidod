// =============================================================================
//  CUPIDO — presets de fábrica
//  Cada preset parte dos valores padrão e muda só o que precisa.
//  O tom do autotune (tune_key) NÃO é salvo nos presets de estilo: ele vem do
//  beat (botão "Ouvir o beat") ou do usuário, porque muda de música pra música.
//  As referências a artistas descrevem características sonoras (vibe), não cópias.
// =============================================================================
(function () {
  // base comum para vocal de trap/rap moderno
  const VOX = {
    gate_on: 1, gate_thresh: -52, gate_range: 14,
    eq_on: 1, eq_hpf: 95, eq_m1_freq: 320, eq_m1_gain: -3, eq_m1_q: 1.2, eq_m2_freq: 3200, eq_m2_gain: 2, eq_m2_q: 0.9, eq_high_freq: 11000, eq_high_gain: 2,
    ds_on: 1, ds_freq: 6800, ds_thresh: -30, ds_range: 8,
    comp_on: 1, comp_thresh: -22, comp_ratio: 6, comp_attack: 3, comp_release: 70, comp_knee: 6, comp_makeup: 5, comp_char: 1,
    opto_on: 1, opto_peak: 30, opto_gain: 1,
    air_on: 1, air_mid: 18, air_high: 30,
    lim_on: 1, lim_ceiling: -1, lim_gain: 2
  };
  const P = (name, cat, desc, tags, values, base = VOX) => ({ name, cat, desc, tags, values: Object.assign({}, base, values) });

  window.CUPIDO_PRESETS = [
    P("Init", "Padrão", "Tudo zerado: EQ, de-esser, compressor e ar leves.", ["init", "padrao", "zerar", "reset"], {}, {}),

    // ---------------- Trap BR ----------------
    P("Flecha de Ouro", "Trap BR", "Trap melódico brilhante, autotune rápido no tom, delay 1/4 pontuado com ducking e plate médio. Vibe 30PRAUM / Matuê.",
      ["matue", "matuê", "tue", "30praum", "melodico", "melódico", "brilhante"],
      { tune_on: 1, tune_scale: 2, tune_speed: 12, tune_humanize: 5, eq_m2_gain: 3, eq_high_gain: 3, air_high: 42, air_mid: 22,
        sat_on: 1, sat_type: 0, sat_drive: 30, sat_b4_amt: 3, sat_b5_amt: 4, sat_mix: 55,
        dbl_on: 1, dbl_mix: 18, dbl_detune: 7, dbl_delay: 14,
        dly_on: 1, dly_sync: 1, dly_note: 9, dly_feedback: 28, dly_mode: 1, dly_lowcut: 380, dly_highcut: 7500, dly_duck: 55, dly_mix: 20,
        rev_on: 1, rev_type: 1, rev_size: 55, rev_decay: 1.9, rev_predelay: 32, rev_damp: 40, rev_lowcut: 300, rev_duck: 30, rev_mix: 14 }),
    P("Concreto", "Trap BR", "Autotune duro, timbre agudo e saturado, bem comprimido, slap + 1/8 e dobras abertas. Vibe Teto.",
      ["teto", "duro", "hard", "agressivo", "saturado", "agudo"],
      { tune_on: 1, tune_scale: 2, tune_speed: 3, tune_humanize: 0, comp_thresh: -26, comp_ratio: 8, comp_attack: 1.5, comp_makeup: 7,
        eq_hpf: 120, eq_m2_freq: 3800, eq_m2_gain: 4, eq_high_gain: 4, air_high: 50, air_mid: 30,
        sat_on: 1, sat_type: 2, sat_drive: 55, sat_b4_amt: 5, sat_b5_amt: 6, sat_mix: 60,
        dbl_on: 1, dbl_voices: 1, dbl_mix: 30, dbl_width: 100, dbl_detune: 11,
        dly_on: 1, dly_sync: 1, dly_note: 5, dly_feedback: 22, dly_mode: 1, dly_lowcut: 420, dly_duck: 60, dly_mix: 16,
        rev_on: 1, rev_type: 4, rev_size: 40, rev_decay: 0.9, rev_predelay: 12, rev_mix: 10, lim_gain: 4 }),
    P("Recife Neon", "Trap BR", "Trap melódico escuro e longo: delay 1/2 com mais repetições, hall grande, saturação de fita. Vibe Yunk Vino.",
      ["yunk", "vino", "yunk vino", "recife", "escuro", "atmosferico", "longo"],
      { tune_on: 1, tune_scale: 2, tune_speed: 18, tune_humanize: 10, eq_high_gain: 1, air_high: 25, eq_lpf: 16000,
        sat_on: 1, sat_type: 1, sat_drive: 45, sat_b2_amt: 3, sat_b4_amt: 2, sat_mix: 55,
        dbl_on: 1, dbl_mix: 16,
        dly_on: 1, dly_sync: 1, dly_note: 10, dly_feedback: 42, dly_mode: 1, dly_lowcut: 350, dly_highcut: 5200, dly_duck: 50, dly_mod: 25, dly_drive: 30, dly_mix: 24,
        rev_on: 1, rev_type: 2, rev_size: 80, rev_decay: 3.4, rev_predelay: 28, rev_damp: 60, rev_lowcut: 280, rev_highcut: 7000, rev_duck: 35, rev_mix: 22 }),
    P("Plug Lunar", "Trap BR", "Pluggnb sedoso: autotune suave, muito espaço e brilho macio.",
      ["plug", "pluggnb", "plugg", "suave", "sedoso", "espacial"],
      { tune_on: 1, tune_scale: 2, tune_speed: 35, tune_humanize: 25, comp_ratio: 4, comp_thresh: -20, air_high: 45, eq_m1_gain: -4,
        dbl_on: 1, dbl_mix: 22, dly_on: 1, dly_note: 9, dly_feedback: 35, dly_mix: 22, dly_duck: 45,
        rev_on: 1, rev_type: 2, rev_size: 85, rev_decay: 2.8, rev_predelay: 40, rev_mix: 24, rev_mod: 45 }),
    P("Drill Seco", "Trap BR", "Voz seca, na cara e agressiva, só um slap curtinho.",
      ["drill", "seco", "na cara", "agressivo"],
      { comp_thresh: -26, comp_ratio: 8, comp_attack: 2, comp_makeup: 7, eq_m2_gain: 4, eq_m2_freq: 3500, sat_on: 1, sat_type: 2, sat_drive: 40, sat_b3_amt: 4, sat_b4_amt: 3,
        dly_on: 1, dly_mode: 3, dly_sync: 0, dly_time: 95, dly_feedback: 8, dly_mix: 12, rev_on: 1, rev_type: 4, rev_decay: 0.5, rev_mix: 6, lim_gain: 4 }),
    P("Rage Distorcido", "Trap BR", "Fuzz forte, hard-tune, doubler largo e bem alto.",
      ["rage", "distorcido", "fuzz", "pesado", "opium"],
      { tune_on: 1, tune_scale: 2, tune_speed: 0, sat_on: 1, sat_type: 5, sat_drive: 75, sat_b2_amt: 5, sat_b3_amt: 7, sat_b4_amt: 6, sat_b5_amt: 3, sat_mix: 70, sat_out: -3,
        comp_thresh: -28, comp_ratio: 10, comp_makeup: 6, dbl_on: 1, dbl_voices: 1, dbl_mix: 35, dbl_width: 100, dbl_detune: 15,
        dly_on: 1, dly_note: 5, dly_feedback: 30, dly_mix: 18, rev_on: 1, rev_type: 2, rev_decay: 2.2, rev_mix: 15, lim_gain: 6 }),
    P("Ad-libs Telefone", "Trap BR", "Ad-libs com cara de rádio/telefone: banda estreita e saturado.",
      ["adlib", "ad-lib", "telefone", "radio", "rádio", "lofi"],
      { eq_hpf: 450, eq_lpf: 3800, eq_m2_freq: 1800, eq_m2_gain: 6, sat_on: 1, sat_type: 4, sat_drive: 60, sat_b3_amt: 8, sat_mix: 70, air_on: 0,
        dly_on: 1, dly_note: 5, dly_mode: 1, dly_feedback: 35, dly_mix: 25, rev_on: 0, comp_ratio: 10 }),
    P("Backing Largo", "Trap BR", "Para dobras e backing vocals: largo, mais grave cortado e atrás.",
      ["backing", "dobra", "back", "coro", "largo", "wide"],
      { eq_hpf: 180, eq_high_gain: 3, dbl_on: 1, dbl_voices: 1, dbl_mix: 45, dbl_width: 100, img_width: 150,
        rev_on: 1, rev_type: 1, rev_mix: 22, out_gain: -3 }),

    // ---------------- Funk ----------------
    P("Funk Mandelão", "Funk", "Voz estourada na frente, muito brilho e pressão.",
      ["funk", "mandelao", "mandelão", "baile", "pressao"],
      { comp_thresh: -26, comp_ratio: 8, comp_makeup: 7, eq_m2_freq: 2800, eq_m2_gain: 5, eq_high_gain: 4, air_high: 40, sat_on: 1, sat_type: 2, sat_drive: 45, sat_b3_amt: 4,
        rev_on: 1, rev_type: 4, rev_mix: 8, lim_gain: 6 }),
    P("Funk Melody", "Funk", "Funk melódico: autotune médio, brilho e reverb curto.",
      ["funk melody", "melody", "funk melodico"],
      { tune_on: 1, tune_scale: 1, tune_speed: 20, air_high: 35, dly_on: 1, dly_note: 6, dly_mix: 14, rev_on: 1, rev_type: 1, rev_mix: 14 }),

    // ---------------- Rap ----------------
    P("Boom Bap Cru", "Rap", "Rap clássico: compressão de 1176, médio presente, sala curta.",
      ["boom bap", "rap", "classico", "clássico", "cru"],
      { comp_ratio: 5, comp_attack: 6, eq_m1_gain: -2, eq_m2_gain: 3, sat_on: 1, sat_type: 1, sat_drive: 35, sat_b3_amt: 3, rev_on: 1, rev_type: 0, rev_decay: 0.7, rev_mix: 9 }),
    P("Podcast Limpo", "Rap", "Fala limpa: redução de ruído, de-esser e nível constante.",
      ["podcast", "fala", "locucao", "locução", "limpo", "voz falada"],
      { nr_on: 1, nr_amount: 55, gate_thresh: -48, comp_ratio: 4, comp_thresh: -24, opto_peak: 40, eq_hpf: 80, eq_low_gain: 2, eq_low_freq: 160, air_high: 15, lufs_target: -16 }),

    // ---------------- Pop / R&B ----------------
    P("Seda R&B", "Pop/R&B", "R&B suave: opto bem presente, autotune natural, hall elegante.",
      ["rnb", "r&b", "suave", "seda", "romantico", "romântico"],
      { tune_on: 1, tune_scale: 1, tune_speed: 45, tune_humanize: 35, comp_ratio: 3, comp_char: 0, opto_peak: 45, air_high: 35, eq_m1_gain: -2,
        dly_on: 1, dly_note: 8, dly_feedback: 22, dly_mix: 12, rev_on: 1, rev_type: 2, rev_decay: 2.4, rev_mix: 18 }),
    P("Pop Rádio", "Pop/R&B", "Pop polido para rádio/streaming.",
      ["pop", "radio", "rádio", "polido", "comercial"],
      { tune_on: 1, tune_scale: 1, tune_speed: 25, comp_ratio: 4, comp_char: 2, air_high: 38, air_mid: 25, dbl_on: 1, dbl_mix: 14,
        dly_on: 1, dly_note: 6, dly_mix: 10, rev_on: 1, rev_type: 1, rev_mix: 13 }),
    P("Balada Íntima", "Pop/R&B", "Voz próxima e quente, pouca afinação, reverb de câmara.",
      ["balada", "intimo", "íntimo", "acustico", "acústico", "quente"],
      { comp_ratio: 3, comp_thresh: -20, comp_char: 0, opto_peak: 40, eq_low_gain: 2, sat_on: 1, sat_type: 0, sat_drive: 20, sat_b2_amt: 2,
        rev_on: 1, rev_type: 3, rev_decay: 1.6, rev_mix: 15 }),

    // ---------------- Efeitos ----------------
    P("Robozinho", "Efeitos", "Hard-tune total, velocidade 0: o efeito robótico clássico.",
      ["robo", "robô", "robotico", "robótico", "t-pain", "tpain", "hard tune"],
      { tune_on: 1, tune_scale: 2, tune_speed: 0, tune_humanize: 0, tune_amount: 100 }),
    P("Fantasma", "Efeitos", "Voz distante num reverb enorme.",
      ["fantasma", "distante", "etereo", "etéreo", "gigante", "espaco"],
      { rev_on: 1, rev_type: 2, rev_size: 100, rev_decay: 8, rev_predelay: 60, rev_mix: 45, rev_mod: 60, dly_on: 1, dly_note: 10, dly_feedback: 50, dly_mix: 25, eq_hpf: 200 }),
    P("Eco Infinito", "Efeitos", "Delay ping-pong quase infinito com fita.",
      ["eco", "infinito", "dub", "ping pong"],
      { dly_on: 1, dly_note: 9, dly_feedback: 82, dly_mode: 1, dly_drive: 50, dly_mod: 40, dly_mix: 32, dly_duck: 20 }),
    P("Oitava Acima", "Efeitos", "Transposição +12 para efeitos de voz aguda.",
      ["oitava", "chipmunk", "agudo", "fino"],
      { tune_on: 1, tune_scale: 0, tune_transpose: 12, tune_speed: 20 }),
    P("Oitava Abaixo", "Efeitos", "Transposição -12 para voz grave/demoníaca.",
      ["grave", "demonio", "demônio", "oitava abaixo", "monstro"],
      { tune_on: 1, tune_scale: 0, tune_transpose: -12, tune_speed: 20, sat_on: 1, sat_type: 5, sat_drive: 40, sat_b2_amt: 4 }),

    // ---------------- Master ----------------
    P("Master Trap -8", "Master", "Para o canal master: glue, tilt leve e limiter alto (~ -8 LUFS).",
      ["master", "masterizacao", "masterização", "alto", "trap master"],
      { glue_on: 1, glue_thresh: -14, glue_ratio: 2, glue_release: 300, tilt: 0.5, img_monobass: 1, img_width: 110, lim_on: 1, lim_gain: 7, lim_ceiling: -1, lufs_target: -8 }, {}),
    P("Master Streaming -14", "Master", "Master dinâmico para plataformas (~ -14 LUFS).",
      ["streaming", "spotify", "-14", "dinamico", "dinâmico"],
      { glue_on: 1, glue_thresh: -10, glue_ratio: 1.6, lim_on: 1, lim_gain: 2, lim_ceiling: -1, img_monobass: 1, lufs_target: -14 }, {}),
    P("Master Quente", "Master", "Master com cor analógica: fita nas bandas e glue.",
      ["master quente", "analogico", "analógico", "vintage"],
      { glue_on: 1, glue_thresh: -12, glue_ratio: 2, sat_on: 1, sat_type: 1, sat_drive: 30, sat_b1_amt: 2, sat_b5_amt: 2, sat_mix: 40, lim_on: 1, lim_gain: 4, lufs_target: -10 }, {})
  ];
})();
