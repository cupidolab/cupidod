// =============================================================================
//  CUPIDO — Base de conhecimento do Cupidinho
//  • STYLES: receita de mixagem vocal por estilo (números tirados da pesquisa:
//    guias de engenheiros, manuais de plugins, artigos em PT e EN — ver
//    docs/BASE-DE-CONHECIMENTO.md com as fontes).
//  • ARTISTS: 100+ referências. Cada artista = estilo base + ajustes de vibe
//    (afinação, brilho, saturação, espaço, delay, reverb, largura, pressão).
//    São APROXIMAÇÕES de características sonoras, não cópias de ninguém.
//  • REVERB_PRESETS: salas prontas para o módulo ESPAÇO.
//  • classifyStyle(): adivinha o estilo do beat pelo "DNA" (BPM, 808, hi-hats...).
//  • diagnoseMix(): analisa voz x beat (embolando? escondida? ardida?) e propõe correções.
// =============================================================================
(function () {
  // ---------------------------------------------------------------------------
  //  Estilos — receitas base (valores reais dos parâmetros do CUPIDO)
  // ---------------------------------------------------------------------------
  const STYLES = {
    trap: {
      name: "Trap", lufs: -9,
      tips: "Compressão forte (6–10 dB, FET rápido), de-esser 5–7 kHz antes do brilho, presença 2–5 kHz, ar 10–16 kHz, autotune no tom (0–15 ms), delay 1/4 ou 1/8 pontuado com retorno filtrado (HP 400 Hz / LP 8 kHz), plate curto com pré-delay 15–30 ms.",
      v: { gate_on: 1, gate_thresh: -45, gate_range: 14, eq_on: 1, eq_hpf: 110, eq_m1_freq: 300, eq_m1_gain: -3.5, eq_m1_q: 1.5, eq_m2_freq: 3500, eq_m2_gain: 2.5, eq_m2_q: 1.0, eq_high_freq: 12000, eq_high_gain: 3,
           ds_on: 1, ds_freq: 6500, ds_thresh: -32, ds_range: 6, comp_on: 1, comp_char: 1, comp_ratio: 7, comp_attack: 4, comp_release: 60, comp_thresh: -26, comp_knee: 3, comp_makeup: 0,
           opto_on: 1, opto_peak: 25, tune_on: 1, tune_scale: 2, tune_speed: 12, tune_humanize: 0, sat_on: 1, sat_type: 1, sat_drive: 25, sat_b4_amt: 2, sat_b5_amt: 2, sat_mix: 50,
           air_on: 1, air_mid: 15, air_high: 30, dly_on: 1, dly_sync: 1, dly_note: 8, dly_feedback: 25, dly_mode: 1, dly_lowcut: 400, dly_highcut: 8000, dly_duck: 50, dly_mix: 15,
           rev_on: 1, rev_type: 1, rev_size: 45, rev_decay: 1.0, rev_predelay: 25, rev_lowcut: 400, rev_highcut: 9000, rev_duck: 30, rev_mix: 15, lim_on: 1, lim_ceiling: -1, lufs_target: -9 } },
    trapbr: {
      name: "Trap BR", lufs: -9,
      tips: "Base do trap com autotune melódico (10–18 ms) e voz bem na frente; delay 1/4 pontuado com ducking alto; dobra sutil; muito brilho controlado pelo de-esser.",
      base: "trap",
      v: { tune_speed: 14, dly_note: 9, dly_duck: 55, dly_mix: 18, dbl_on: 1, dbl_mix: 16, dbl_detune: 7, rev_decay: 1.4, rev_mix: 14, air_high: 36 } },
    drill: {
      name: "Drill", lufs: -8,
      tips: "Voz escura e clara: HPF 90 Hz, compressão em dois estágios (2:1 suave + picos rápidos), saturação de fita 5–10%, slap mono 80–120 ms filtrado (150 Hz–6 kHz), plate curtíssimo (0,6–1 s) quase só no hook.",
      v: { gate_on: 1, gate_thresh: -48, eq_on: 1, eq_hpf: 90, eq_m1_freq: 250, eq_m1_gain: -2, eq_m1_q: 1.2, eq_m2_freq: 3500, eq_m2_gain: 1, eq_high_freq: 11000, eq_high_gain: 1,
           ds_on: 1, ds_freq: 7000, ds_thresh: -30, ds_range: 6, comp_on: 1, comp_char: 1, comp_ratio: 4, comp_attack: 15, comp_release: 120, comp_thresh: -22,
           opto_on: 1, opto_peak: 30, tune_on: 1, tune_scale: 2, tune_speed: 20, tune_humanize: 10, sat_on: 1, sat_type: 1, sat_drive: 15, sat_b3_amt: 2, sat_mix: 30,
           air_on: 1, air_mid: 10, air_high: 12, dly_on: 1, dly_sync: 0, dly_mode: 3, dly_time: 100, dly_feedback: 10, dly_lowcut: 150, dly_highcut: 6000, dly_mix: 12, dly_duck: 30,
           rev_on: 1, rev_type: 1, rev_size: 35, rev_decay: 0.8, rev_predelay: 35, rev_lowcut: 300, rev_mix: 8, lim_on: 1, lufs_target: -8 } },
    plug: {
      name: "Plug / Pluggnb", lufs: -10,
      tips: "Afinação suave (retune médio), compressão 3:1–4:1 média, HPF 70–110 Hz, lama 200–400 Hz cortada só onde acumula, ar 10–12 kHz depois do de-esser, delay 1/4 ou 1/8 pontuado filtrado, reverb plate/hall largo que 'abre' em volta da voz com ducking.",
      v: { gate_on: 1, gate_thresh: -46, eq_on: 1, eq_hpf: 100, eq_m1_freq: 300, eq_m1_gain: -3, eq_m2_freq: 3200, eq_m2_gain: 1.5, eq_high_freq: 11000, eq_high_gain: 2.5,
           ds_on: 1, ds_freq: 6800, ds_thresh: -31, ds_range: 6, comp_on: 1, comp_char: 0, comp_ratio: 3.5, comp_attack: 10, comp_release: 100, comp_thresh: -22,
           opto_on: 1, opto_peak: 30, tune_on: 1, tune_scale: 2, tune_speed: 30, tune_humanize: 20, sat_on: 1, sat_type: 1, sat_drive: 15, sat_b5_amt: 2, sat_mix: 35,
           air_on: 1, air_mid: 12, air_high: 40, dbl_on: 1, dbl_mix: 18, dly_on: 1, dly_sync: 1, dly_note: 9, dly_feedback: 35, dly_mode: 1, dly_lowcut: 350, dly_highcut: 6500, dly_duck: 50, dly_mix: 22,
           rev_on: 1, rev_type: 2, rev_size: 80, rev_decay: 2.6, rev_predelay: 35, rev_lowcut: 300, rev_highcut: 8000, rev_mod: 40, rev_duck: 40, rev_mix: 22, lim_on: 1, lufs_target: -10 } },
    rnb: {
      name: "R&B", lufs: -11,
      tips: "HPF mais baixo (60–80 Hz), corte 200–350 Hz, calor +1–2 dB em 150–250 Hz, compressão suave 2:1–3:1 (ataque 15–30 ms, 3–5 dB), de-esser 5–8 kHz leve, saturação de fita 10–20%, plate/hall 1,8–3 s com pré-delay 30–60 ms, delay 1/8 com high-cut 4–6 kHz.",
      v: { gate_on: 1, gate_thresh: -42, eq_on: 1, eq_hpf: 75, eq_low_freq: 200, eq_low_gain: 1.5, eq_m1_freq: 280, eq_m1_gain: -2.5, eq_m2_freq: 3800, eq_m2_gain: 1.5, eq_high_freq: 13000, eq_high_gain: 2.5,
           ds_on: 1, ds_freq: 6500, ds_thresh: -30, ds_range: 4, comp_on: 1, comp_char: 0, comp_ratio: 2.5, comp_attack: 20, comp_release: 120, comp_thresh: -20, comp_knee: 9,
           opto_on: 1, opto_peak: 40, tune_on: 1, tune_scale: 2, tune_speed: 40, tune_humanize: 35, sat_on: 1, sat_type: 1, sat_drive: 15, sat_b2_amt: 1.5, sat_mix: 25,
           air_on: 1, air_mid: 12, air_high: 35, dbl_on: 1, dbl_mix: 14, dbl_detune: 6, dly_on: 1, dly_sync: 1, dly_note: 5, dly_feedback: 28, dly_mode: 0, dly_highcut: 5000, dly_lowcut: 300, dly_mix: 12,
           rev_on: 1, rev_type: 2, rev_size: 70, rev_decay: 2.4, rev_predelay: 45, rev_damp: 55, rev_lowcut: 250, rev_highcut: 7500, rev_mix: 20, lim_on: 1, lufs_target: -11 } },
    funk: {
      name: "Funk BR", lufs: -6,
      tips: "Voz do MC estourada e na cara: compressão VCA pesada, presença forte 2,5–3 kHz, saturação de transistor nos médios, reverb curtinho ou nenhum, limiter alto (funk costuma bater -7 a -5 LUFS).",
      v: { gate_on: 1, gate_thresh: -44, eq_on: 1, eq_hpf: 120, eq_m1_freq: 320, eq_m1_gain: -3, eq_m2_freq: 2800, eq_m2_gain: 4, eq_high_freq: 11000, eq_high_gain: 3,
           ds_on: 1, ds_freq: 6500, ds_thresh: -32, ds_range: 7, comp_on: 1, comp_char: 2, comp_ratio: 8, comp_attack: 2, comp_release: 50, comp_thresh: -26,
           opto_on: 0, tune_on: 1, tune_scale: 1, tune_speed: 10, sat_on: 1, sat_type: 2, sat_drive: 45, sat_b3_amt: 4, sat_b4_amt: 3, sat_mix: 55,
           air_on: 1, air_mid: 20, air_high: 35, dly_on: 1, dly_sync: 1, dly_note: 5, dly_feedback: 15, dly_mix: 8, dly_duck: 60,
           rev_on: 1, rev_type: 4, rev_size: 30, rev_decay: 0.6, rev_predelay: 10, rev_mix: 7, lim_on: 1, lufs_target: -6 } },
    funkmelody: {
      name: "Funk melody / Pop funk", lufs: -8, base: "funk",
      tips: "Funk com autotune melódico (15–25 ms), brilho e um plate curto; menos saturação que o mandelão.",
      v: { tune_speed: 20, tune_scale: 1, sat_drive: 25, sat_b3_amt: 2, comp_ratio: 5, rev_type: 1, rev_decay: 1.2, rev_mix: 12, dly_note: 6, dly_mix: 12, lufs_target: -8 } },
    pop: {
      name: "Pop", lufs: -9,
      tips: "Voz polida e brilhante: compressão em série (VCA + opto), presença +2 dB, ar com shelf 10–16 kHz, autotune transparente, dobras e um plate médio; delays curtos e throws pontuais.",
      v: { gate_on: 1, gate_thresh: -46, eq_on: 1, eq_hpf: 90, eq_m1_freq: 300, eq_m1_gain: -2, eq_m2_freq: 3000, eq_m2_gain: 2, eq_high_freq: 12000, eq_high_gain: 3,
           ds_on: 1, ds_freq: 7000, ds_thresh: -31, ds_range: 6, comp_on: 1, comp_char: 2, comp_ratio: 4, comp_attack: 6, comp_release: 80, comp_thresh: -22,
           opto_on: 1, opto_peak: 35, tune_on: 1, tune_scale: 1, tune_speed: 25, tune_humanize: 15, sat_on: 1, sat_type: 0, sat_drive: 20, sat_b4_amt: 1.5, sat_mix: 35,
           air_on: 1, air_mid: 20, air_high: 40, dbl_on: 1, dbl_mix: 14, dly_on: 1, dly_sync: 1, dly_note: 6, dly_feedback: 22, dly_mix: 10, dly_duck: 45,
           rev_on: 1, rev_type: 1, rev_size: 55, rev_decay: 1.6, rev_predelay: 30, rev_mix: 14, lim_on: 1, lufs_target: -9 } },
    reggae: {
      name: "Reggae / Dub", lufs: -11,
      tips: "Voz quente e encorpada (médios 2–3 kHz suaves, topo um pouco mais escuro), compressão FET média, saturação de fita, eco de fita escuro (1/4 ou 1/8 pontuado, repetições altas, retorno 300 Hz–4 kHz) e reverb de mola/câmara; throws de delay no fim das frases.",
      v: { gate_on: 1, gate_thresh: -44, eq_on: 1, eq_hpf: 90, eq_low_freq: 180, eq_low_gain: 1, eq_m1_freq: 350, eq_m1_gain: -2, eq_m2_freq: 2500, eq_m2_gain: 2, eq_high_freq: 10000, eq_high_gain: 0, eq_lpf: 15000,
           ds_on: 1, ds_freq: 6500, ds_thresh: -30, ds_range: 5, comp_on: 1, comp_char: 1, comp_ratio: 4, comp_attack: 10, comp_release: 100, comp_thresh: -22,
           opto_on: 1, opto_peak: 35, tune_on: 0, sat_on: 1, sat_type: 1, sat_drive: 30, sat_b2_amt: 2, sat_b3_amt: 1.5, sat_mix: 45,
           air_on: 1, air_mid: 8, air_high: 8, dly_on: 1, dly_sync: 1, dly_note: 9, dly_feedback: 42, dly_mode: 2, dly_lowcut: 300, dly_highcut: 4000, dly_drive: 40, dly_mod: 30, dly_duck: 35, dly_mix: 18,
           rev_on: 1, rev_type: 3, rev_size: 35, rev_decay: 1.6, rev_diff: 35, rev_er: 60, rev_damp: 60, rev_predelay: 15, rev_mix: 14, lim_on: 1, lufs_target: -11 } },
    rap: {
      name: "Rap / Boom bap", lufs: -9,
      tips: "Voz seca e presente: compressão 1176 (4:1–6:1), médio-agudo +3 dB, saturação de fita, sala curta e slap; foco em inteligibilidade.",
      v: { gate_on: 1, gate_thresh: -46, eq_on: 1, eq_hpf: 100, eq_m1_freq: 300, eq_m1_gain: -2.5, eq_m2_freq: 3000, eq_m2_gain: 3, eq_high_freq: 11000, eq_high_gain: 1.5,
           ds_on: 1, ds_freq: 6500, ds_thresh: -30, ds_range: 6, comp_on: 1, comp_char: 1, comp_ratio: 5, comp_attack: 6, comp_release: 70, comp_thresh: -24,
           opto_on: 1, opto_peak: 25, tune_on: 0, sat_on: 1, sat_type: 1, sat_drive: 30, sat_b3_amt: 2.5, sat_mix: 45, air_on: 1, air_mid: 12, air_high: 15,
           dly_on: 1, dly_sync: 0, dly_mode: 3, dly_time: 110, dly_feedback: 10, dly_mix: 10, rev_on: 1, rev_type: 0, rev_size: 30, rev_decay: 0.7, rev_mix: 8, lim_on: 1, lufs_target: -9 } },
    rage: {
      name: "Rage", lufs: -7,
      tips: "Hard-tune 0 ms, distorção forte (fuzz) nos médios, dobras bem abertas, delay e reverb grandes; tudo alto.",
      v: { gate_on: 1, eq_on: 1, eq_hpf: 130, eq_m1_freq: 300, eq_m1_gain: -3, eq_m2_freq: 3000, eq_m2_gain: 3, comp_on: 1, comp_char: 2, comp_ratio: 10, comp_attack: 2, comp_release: 50, comp_thresh: -28,
           ds_on: 1, ds_thresh: -32, tune_on: 1, tune_scale: 2, tune_speed: 0, tune_humanize: 0, sat_on: 1, sat_type: 5, sat_drive: 70, sat_b2_amt: 4, sat_b3_amt: 6, sat_b4_amt: 5, sat_mix: 65, sat_out: -2,
           air_on: 1, air_high: 25, dbl_on: 1, dbl_voices: 1, dbl_mix: 32, dbl_width: 100, dbl_detune: 14, dly_on: 1, dly_sync: 1, dly_note: 5, dly_feedback: 30, dly_mix: 18,
           rev_on: 1, rev_type: 2, rev_decay: 2.2, rev_mix: 16, lim_on: 1, lufs_target: -7 } },
    phonk: {
      name: "Brazilian Phonk", lufs: -6, base: "funk",
      tips: "Base do funk com voz mais suja e escura: saturação digital/fuzz, passa-baixas para ar 'lo-fi', delay 1/8 marcado.",
      v: { sat_type: 4, sat_drive: 60, sat_b3_amt: 5, eq_lpf: 9500, eq_hpf: 150, air_on: 0, dly_note: 5, dly_mix: 20, dly_feedback: 30, lufs_target: -6 } },
    sertanejo: {
      name: "Sertanejo / Piseiro", lufs: -9, base: "pop",
      tips: "Voz grande e brilhante na frente, afinação transparente, dobras e reverb médio (hall/plate).",
      v: { tune_speed: 30, tune_humanize: 25, rev_type: 2, rev_decay: 1.8, rev_mix: 16, dbl_mix: 18 } }
  };
  // resolve heranças (base)
  function styleValues (key) {
    const s = STYLES[key]; if (!s) return {};
    return Object.assign({}, s.base ? styleValues (s.base) : {}, s.v);
  }

  // ---------------------------------------------------------------------------
  //  Ajustes de vibe por artista (aplicados sobre o estilo)
  //   t: velocidade do autotune (ms) | b: brilho -2..+3 | s: saturação 0..3 (st: tipo)
  //   e: espaço -2..+3 (reverb/delay) | dn: nota do delay | rt: tipo de reverb
  //   w: largura 0..3 | c: pressão/compressão -1..+2 | dark: topo escuro | adl: muitos ad-libs
  // ---------------------------------------------------------------------------
  const A = (name, style, mods, vibe, aliases = []) => ({ name, style, mods, vibe, aliases });
  const ARTISTS = [
    // ---------------- Trap BR ----------------
    A("Matuê", "trapbr", { t: 12, b: 2, e: 1, dn: 9, w: 1 }, "melódico, brilhante e na cara; delay 1/4 pontuado", ["matue", "tue", "30praum"]),
    A("Teto", "trapbr", { t: 3, b: 3, s: 2, st: 2, e: 0, dn: 5, w: 2, c: 1 }, "autotune duro, agudo e saturado, dobras abertas", ["teto"]),
    A("Yunk Vino", "trapbr", { t: 16, b: 0, s: 2, st: 1, e: 3, dn: 10, rt: 2, dark: 1 }, "escuro e atmosférico, delay longo e hall grande", ["yunk vino", "vino", "yunk"]),
    A("Veigh", "trapbr", { t: 10, b: 2, e: 1, dn: 9, w: 1 }, "melódico limpo, voz suave e brilhante", ["veigh"]),
    A("Orochi", "trapbr", { t: 14, b: 1, s: 1, e: 0, c: 1 }, "rap-trap firme, voz cheia e na frente", ["orochi"]),
    A("Wiu", "trapbr", { t: 8, b: 2, e: 1, dn: 8, w: 1 }, "melódico jovem, afinação marcada", ["wiu"]),
    A("Kayblack", "trapbr", { t: 6, b: 1, s: 1, e: 1, dn: 9 }, "trap de favela melódico, autotune evidente", ["kayblack", "kay black"]),
    A("Chefin", "trapbr", { t: 5, b: 2, s: 1, e: 0, c: 1 }, "flow rápido, autotune rápido e voz seca", ["chefin"]),
    A("Brandão85", "trapbr", { t: 8, b: 1, e: 1, dn: 9 }, "melódico, timbre grave com brilho", ["brandao", "brandao85"]),
    A("Sidoka", "trapbr", { t: 4, b: 2, s: 1, e: 1, w: 2, adl: 1 }, "melodias tortas, muitos ad-libs e dobras", ["sidoka"]),
    A("MC Cabelinho", "trapbr", { t: 12, b: 1, e: 1, dn: 9 }, "trap/funk do RJ melódico", ["cabelinho", "mc cabelinho"]),
    A("Filipe Ret", "rap", { t: 25, b: 1, s: 1, e: 0, c: 0 }, "rap cantado, voz grave e natural", ["filipe ret", "ret"]),
    A("L7nnon", "trapbr", { t: 12, b: 1, e: 1 }, "trap melódico grave e limpo", ["l7nnon", "lennon"]),
    A("Poze do Rodo", "trapbr", { t: 6, b: 2, s: 1, e: 1, c: 1 }, "trap/funk com autotune forte", ["poze", "poze do rodo", "mc poze"]),
    A("Oruam", "trapbr", { t: 5, b: 2, s: 1, e: 1, dn: 8 }, "melódico rápido, voz aguda", ["oruam"]),
    A("TZ da Coronel", "trapbr", { t: 6, b: 1, s: 1, e: 1 }, "trap/funk do RJ, autotune marcado", ["tz da coronel", "tz"]),
    A("Ryu, the Runner", "trapbr", { t: 6, b: 2, e: 2, dn: 9, w: 1 }, "trap/pluggnb espacial", ["ryu", "ryu the runner"]),
    A("Borges", "trapbr", { t: 6, b: 2, s: 1, e: 1, c: 1 }, "trap agressivo, voz aguda", ["borges", "mc borges"]),
    A("Djonga", "rap", { t: 0, b: 1, s: 1, e: -1, c: 1, noTune: 1 }, "rap seco e direto, sem autotune", ["djonga"]),
    A("BK'", "rap", { b: 1, s: 1, e: 0, noTune: 1 }, "rap cheio, voz grave natural", ["bk", "bk'"]),
    A("Baco Exu do Blues", "rap", { b: 0, s: 1, e: 1, noTune: 1 }, "rap com alma, voz quente", ["baco", "baco exu do blues"]),
    A("Xamã", "trapbr", { t: 20, b: 1, e: 1, dn: 9 }, "melódico romântico", ["xama", "xamã"]),
    A("Kyan", "trapbr", { t: 10, b: 2, e: 1 }, "trap melódico limpo", ["kyan"]),
    A("Derek", "trapbr", { t: 6, b: 2, s: 1, e: 1, w: 1 }, "trap melódico da Recayd Mob", ["derek"]),
    A("Dfideliz", "trapbr", { t: 8, b: 1, s: 1, e: 1 }, "trap grave", ["dfideliz"]),
    A("Jé Santiago", "trapbr", { t: 6, b: 2, e: 2, dn: 9 }, "melódico espacial", ["je santiago", "jé santiago"]),
    A("Raffa Moreira", "trapbr", { t: 4, b: 2, s: 2, st: 4, e: 1 }, "trap underground cru", ["raffa moreira", "raffa"]),
    A("Sain", "trapbr", { t: 10, b: 1, e: 1 }, "trap melódico suave", ["sain"]),
    A("Major RD", "drill", { t: 12, b: 1, s: 1, e: 0 }, "drill/trap BR grave", ["major rd", "major"]),
    A("MC IG", "funk", { t: 8, b: 2, s: 1, e: 0 }, "funk/trap paulista", ["mc ig", "ig"]),
    A("Mano Brown", "rap", { b: 0, s: 1, e: 0, noTune: 1, dark: 1 }, "rap clássico grave", ["mano brown", "racionais"]),
    A("Emicida", "rap", { b: 1, s: 0, e: 0, noTune: 1 }, "rap articulado e limpo", ["emicida"]),
    A("Criolo", "rap", { b: 1, s: 1, e: 1, noTune: 1 }, "rap com timbre aberto", ["criolo"]),
    A("Kawe", "trapbr", { t: 6, b: 2, s: 1, e: 1 }, "trap melódico", ["kawe"]),
    A("Jovem Dex", "trapbr", { t: 5, b: 2, s: 1, e: 1, dn: 8 }, "trap paulista melódico", ["jovem dex", "dex"]),
    A("Mano B", "trapbr", { t: 6, b: 2, e: 1 }, "trap nordestino", ["mano b"]),
    // ---------------- Funk BR ----------------
    A("MC Ryan SP", "funk", { t: 8, b: 2, s: 2, c: 2 }, "funk paulista estourado", ["ryan sp", "mc ryan"]),
    A("MC Paiva", "funk", { t: 8, b: 2, s: 2, c: 1 }, "funk paulista", ["paiva", "mc paiva"]),
    A("MC Hariel", "funkmelody", { t: 14, b: 1, s: 1 }, "funk consciente melódico", ["hariel", "mc hariel"]),
    A("MC Livinho", "funkmelody", { t: 16, b: 2, e: 1 }, "funk romântico melódico", ["livinho", "mc livinho"]),
    A("MC Kevin", "funkmelody", { t: 12, b: 2 }, "funk melódico", ["mc kevin"]),
    A("Kevin o Chris", "funk", { t: 10, b: 2, s: 1, e: 1 }, "funk 150 do RJ", ["kevin o chris"]),
    A("MC Don Juan", "funkmelody", { t: 14, b: 2 }, "funk romântico", ["don juan", "mc don juan"]),
    A("MC Daniel", "funkmelody", { t: 14, b: 2, e: 1 }, "funk melódico", ["mc daniel"]),
    A("MC Kekel", "funkmelody", { t: 14, b: 2 }, "funk melody", ["kekel"]),
    A("MC Pedrinho", "funk", { t: 8, b: 2, s: 2 }, "funk ostentação", ["pedrinho"]),
    A("MC Cabelinho (funk)", "funk", { t: 10, b: 1 }, "funk carioca", ["cabelinho funk"]),
    A("Dennis DJ", "funk", { t: 8, b: 2, s: 2, c: 2 }, "funk de pista alto", ["dennis", "dennis dj"]),
    A("Pedro Sampaio", "funkmelody", { t: 18, b: 2, e: 1, w: 1 }, "pop funk de pista", ["pedro sampaio"]),
    A("MC Carol", "funk", { t: 10, b: 2, s: 2, c: 2 }, "funk carioca forte", ["mc carol"]),
    A("Ludmilla", "pop", { t: 20, b: 2, e: 1, w: 1 }, "pop/funk/R&B potente", ["ludmilla"]),
    A("Anitta", "pop", { t: 18, b: 3, e: 1, w: 1 }, "pop/funk brilhante", ["anitta"]),
    A("Luísa Sonza", "pop", { t: 20, b: 2, e: 1, w: 1 }, "pop moderno", ["luisa sonza", "sonza"]),
    A("Pabllo Vittar", "pop", { t: 15, b: 3, s: 1, e: 1, w: 1 }, "pop dançante brilhante", ["pabllo", "pabllo vittar"]),
    A("Gloria Groove", "pop", { t: 18, b: 2, e: 1 }, "pop/rap versátil", ["gloria groove"]),
    A("IZA", "rnb", { t: 30, b: 2, e: 1 }, "R&B/pop potente", ["iza"]),
    A("Marina Sena", "pop", { t: 25, b: 2, e: 1, w: 1 }, "pop leve e aéreo", ["marina sena"]),
    // ---------------- R&B BR ----------------
    A("Liniker", "rnb", { t: 45, b: 1, s: 1, e: 1 }, "soul/R&B natural e quente", ["liniker"]),
    A("Tasha & Tracie", "rnb", { t: 20, b: 2, e: 0, c: 1 }, "rap/R&B afiado", ["tasha", "tracie", "tasha e tracie"]),
    A("Ebony", "rnb", { t: 18, b: 2, e: 1 }, "rap/R&B melódico", ["ebony"]),
    A("Budah", "rnb", { t: 28, b: 1, e: 2 }, "R&B suave e espacial", ["budah"]),
    A("Agnes Nunes", "rnb", { t: 40, b: 1, e: 1 }, "voz doce e natural", ["agnes nunes", "agnes"]),
    A("Vitão", "pop", { t: 22, b: 2, e: 1 }, "pop/R&B jovem", ["vitao", "vitão"]),
    A("Duda Beat", "pop", { t: 25, b: 1, e: 2, dark: 1 }, "pop alternativo com reverb", ["duda beat"]),
    A("Luccas Carlos", "rnb", { t: 22, b: 2, e: 1 }, "R&B melódico", ["luccas carlos"]),
    A("Day Limns", "rnb", { t: 25, b: 1, e: 2 }, "R&B alternativo", ["day limns", "day"]),
    // ---------------- Reggae ----------------
    A("Natiruts", "reggae", { b: 1, e: 1, noTune: 1 }, "reggae pop solar", ["natiruts"]),
    A("Armandinho", "reggae", { b: 1, e: 0, noTune: 1 }, "reggae acústico leve", ["armandinho"]),
    A("Ponto de Equilíbrio", "reggae", { b: 0, e: 2, noTune: 1 }, "roots com dub", ["ponto de equilibrio"]),
    A("Maneva", "reggae", { b: 1, e: 1, noTune: 1 }, "reggae pop", ["maneva"]),
    A("Planta e Raiz", "reggae", { b: 1, e: 1, noTune: 1 }, "reggae roots/pop", ["planta e raiz"]),
    A("Cidade Negra", "reggae", { b: 1, e: 1, noTune: 1 }, "reggae clássico BR", ["cidade negra"]),
    A("Bob Marley", "reggae", { b: -1, s: 1, e: 1, noTune: 1, dark: 1 }, "roots clássico, voz quente com eco de fita", ["bob marley", "marley"]),
    A("Damian Marley", "reggae", { b: 1, s: 1, e: 1, c: 1, noTune: 1 }, "dancehall/roots moderno", ["damian marley"]),
    A("Chronixx", "reggae", { b: 1, e: 1, noTune: 1 }, "roots moderno", ["chronixx"]),
    A("Protoje", "reggae", { b: 1, e: 1, noTune: 1 }, "reggae revival", ["protoje"]),
    A("Alborosie", "reggae", { b: 0, s: 2, e: 2, noTune: 1, dark: 1 }, "roots/dub analógico", ["alborosie"]),
    A("Sean Paul", "reggae", { b: 2, s: 1, c: 1, e: 0, t: 25 }, "dancehall de pista", ["sean paul"]),
    // ---------------- Trap / Hip-hop gringo ----------------
    A("Travis Scott", "trap", { t: 15, b: 1, s: 1, e: 3, dn: 6, rt: 2, w: 1 }, "autotune com glide, delay ping-pong 1/8 pontuado, reverb grande", ["travis", "travis scott", "la flame"]),
    A("Future", "trap", { t: 18, b: 0, s: 2, st: 1, e: 3, dn: 8, rt: 2, dark: 1 }, "melódico escuro, fita e reverb grande", ["future"]),
    A("Young Thug", "trap", { t: 8, b: 2, e: 1, w: 1, adl: 1 }, "melodias elásticas e muitos ad-libs", ["young thug", "thug"]),
    A("Lil Baby", "trap", { t: 12, b: 1, e: 0, c: 1 }, "flow seco e na cara", ["lil baby"]),
    A("Gunna", "trap", { t: 25, b: 2, e: 1, dn: 9 }, "melódico suave e 'dripping'", ["gunna"]),
    A("Playboi Carti", "rage", { t: 4, b: 2, s: 2, e: 1, w: 2, adl: 1 }, "rage/baby voice, distorção e dobras", ["carti", "playboi carti"]),
    A("Lil Uzi Vert", "trap", { t: 3, b: 3, s: 1, e: 1, w: 1, adl: 1 }, "autotune duro e brilhante", ["uzi", "lil uzi", "lil uzi vert"]),
    A("Juice WRLD", "trap", { t: 12, b: 2, s: 1, e: 2, dn: 8 }, "emo trap melódico com reverb", ["juice wrld", "juice"]),
    A("The Kid LAROI", "pop", { t: 5, b: 2, e: 2, w: 1 }, "pop/trap melódico", ["laroi", "kid laroi"]),
    A("Post Malone", "pop", { t: 18, b: 2, e: 2, w: 1 }, "pop/trap melódico com reverb brilhante", ["post malone", "posty"]),
    A("Drake", "rnb", { t: 25, b: 1, e: 0, dark: 1, c: 1 }, "voz próxima, escura e seca", ["drake", "drizzy"]),
    A("21 Savage", "trap", { t: 0, b: 0, s: 1, e: 0, dark: 1, noTune: 1 }, "monótono, seco e escuro", ["21 savage", "21"]),
    A("Don Toliver", "trap", { t: 10, b: 2, e: 3, dn: 9, rt: 2, w: 1 }, "melódico espacial", ["don toliver"]),
    A("Trippie Redd", "trap", { t: 6, b: 2, s: 2, e: 2, rt: 2 }, "emo/rage com hall grande", ["trippie", "trippie redd"]),
    A("Lil Durk", "trap", { t: 4, b: 1, s: 1, e: 1 }, "melódico de Chicago, autotune marcado", ["lil durk", "durk"]),
    A("Polo G", "trap", { t: 10, b: 1, e: 1 }, "melódico emotivo", ["polo g"]),
    A("NBA YoungBoy", "trap", { t: 22, b: 1, s: 1, e: 1 }, "melódico cru", ["youngboy", "nba youngboy"]),
    A("Pop Smoke", "drill", { t: 25, b: -1, s: 2, st: 1, e: 0, dark: 1 }, "drill de Nova York, voz grave e rouca", ["pop smoke"]),
    A("Central Cee", "drill", { t: 18, b: 1, e: -1, c: 0 }, "drill UK claro e rápido", ["central cee", "cench"]),
    A("Fivio Foreign", "drill", { t: 25, b: 1, s: 1, c: 1 }, "drill agressivo", ["fivio", "fivio foreign"]),
    A("Chief Keef", "trap", { t: 4, b: 1, s: 2, st: 4, e: 1 }, "drill/trap cru com autotune", ["chief keef", "keef"]),
    A("Ken Carson", "rage", { t: 3, b: 2, s: 3, e: 1, w: 2 }, "rage distorcido", ["ken carson"]),
    A("Destroy Lonely", "rage", { t: 5, b: 1, s: 2, e: 2, dark: 1 }, "rage/plug escuro", ["destroy lonely"]),
    A("Yeat", "rage", { t: 3, b: 2, s: 2, e: 1, w: 2 }, "rage com voz processada", ["yeat"]),
    A("Kanye West", "rap", { t: 20, b: 1, s: 1, e: 1 }, "rap/experimental", ["kanye", "ye", "kanye west"]),
    A("Kendrick Lamar", "rap", { b: 1, s: 1, e: 0, noTune: 1 }, "rap preciso e expressivo", ["kendrick", "kendrick lamar"]),
    A("J. Cole", "rap", { b: 1, s: 1, e: 0, noTune: 1 }, "rap quente e limpo", ["j cole", "cole"]),
    A("Metro Boomin (estética)", "trap", { t: 12, b: 0, e: 3, rt: 2, dark: 1 }, "trap cinematográfico escuro", ["metro boomin", "metro"]),
    A("Lil Tecca", "plug", { t: 15, b: 2, e: 1 }, "melódico leve", ["tecca", "lil tecca"]),
    A("Summrs", "plug", { t: 10, b: 2, e: 2, w: 1 }, "pluggnb", ["summrs"]),
    A("Autumn!", "plug", { t: 12, b: 2, e: 2 }, "pluggnb melódico", ["autumn"]),
    A("Kankan", "plug", { t: 15, b: 1, e: 2, dark: 1 }, "plug espacial", ["kankan"]),
    A("Lucki", "plug", { t: 22, b: 0, e: 2, dark: 1 }, "melancólico e escuro", ["lucki"]),
    // ---------------- R&B / Pop gringo ----------------
    A("SZA", "rnb", { t: 30, b: 2, e: 2, w: 1 }, "R&B aéreo e brilhante", ["sza"]),
    A("The Weeknd", "rnb", { t: 25, b: 2, e: 3, rt: 2, w: 1 }, "R&B/pop cinematográfico, reverb grande", ["weeknd", "the weeknd"]),
    A("Bryson Tiller", "rnb", { t: 28, b: 1, e: 1, dark: 1 }, "trapsoul suave e escuro", ["bryson tiller", "bryson"]),
    A("Summer Walker", "rnb", { t: 30, b: 1, e: 1 }, "R&B íntimo", ["summer walker"]),
    A("Brent Faiyaz", "rnb", { t: 30, b: 1, s: 1, e: 1, dark: 1 }, "R&B esfumaçado", ["brent faiyaz", "brent"]),
    A("Frank Ocean", "rnb", { t: 40, b: 1, e: 2 }, "R&B alternativo natural", ["frank ocean", "frank"]),
    A("Daniel Caesar", "rnb", { t: 45, b: 1, e: 2, noTune: 1 }, "soul suave", ["daniel caesar"]),
    A("Chris Brown", "rnb", { t: 22, b: 2, e: 1, w: 1 }, "R&B/pop polido", ["chris brown"]),
    A("H.E.R.", "rnb", { t: 40, b: 1, e: 1 }, "R&B soul", ["her", "h.e.r."]),
    A("Jhené Aiko", "rnb", { t: 35, b: 2, e: 2, w: 1 }, "R&B etéreo", ["jhene aiko", "jhené"]),
    A("6LACK", "rnb", { t: 25, b: 0, e: 2, dark: 1 }, "R&B escuro", ["6lack", "black"]),
    A("PARTYNEXTDOOR", "rnb", { t: 18, b: 1, e: 2, dark: 1 }, "R&B atmosférico", ["partynextdoor", "party"]),
    A("Billie Eilish", "pop", { t: 40, b: 0, e: 1, c: 1, dark: 1 }, "voz próxima e sussurrada", ["billie", "billie eilish"]),
    A("Ariana Grande", "pop", { t: 30, b: 3, e: 2, w: 2 }, "pop R&B aéreo com muitas dobras", ["ariana", "ariana grande"]),
    A("Dua Lipa", "pop", { t: 25, b: 2, e: 1, w: 1 }, "pop dance", ["dua lipa", "dua"]),
    A("Bad Bunny", "trap", { t: 10, b: 2, e: 1, w: 1 }, "reggaeton/trap latino", ["bad bunny", "benito"]),
    A("Rauw Alejandro", "pop", { t: 15, b: 2, e: 1, w: 1 }, "reggaeton melódico", ["rauw", "rauw alejandro"]),
    A("Feid", "pop", { t: 12, b: 2, e: 1 }, "reggaeton suave", ["feid"]),
    A("Karol G", "pop", { t: 15, b: 2, e: 1 }, "reggaeton pop", ["karol g"])
  ];

  function artistPreset (artist) {
    const m = artist.mods || {}; const v = Object.assign({}, styleValues (artist.style));
    if (m.noTune) v.tune_on = 0; else if (m.t != null) { v.tune_on = 1; v.tune_speed = m.t; v.tune_humanize = m.t >= 30 ? 25 : m.t >= 15 ? 10 : 0; }
    const b = m.b || 0;
    v.air_high = Math.max (0, Math.min (70, (v.air_high || 15) + b * 6));
    v.eq_high_gain = Math.max (-4, Math.min (7, (v.eq_high_gain || 0) + b * 0.8));
    if (m.dark) { v.eq_high_gain = Math.min (v.eq_high_gain, 0.5); v.air_high = Math.min (v.air_high, 15); v.eq_lpf = 15000; }
    if (m.s) { v.sat_on = 1; v.sat_drive = Math.min (90, (v.sat_drive || 20) + m.s * 12); v.sat_b3_amt = Math.min (10, (v.sat_b3_amt || 0) + m.s); v.sat_b4_amt = Math.min (10, (v.sat_b4_amt || 0) + m.s * 0.8); }
    if (m.st != null) v.sat_type = m.st;
    const e = m.e || 0;
    v.rev_mix = Math.max (4, Math.min (35, (v.rev_mix || 12) + e * 3.5)); v.rev_decay = Math.max (0.5, Math.min (5, (v.rev_decay || 1.2) * (1 + e * 0.22)));
    v.dly_mix = Math.max (0, Math.min (32, (v.dly_mix || 12) + e * 3)); v.dly_feedback = Math.max (8, Math.min (55, (v.dly_feedback || 25) + e * 4));
    if (e >= 2) v.rev_size = Math.min (95, (v.rev_size || 50) + 20);
    if (m.dn != null) { v.dly_on = 1; v.dly_sync = 1; v.dly_note = m.dn; if (v.dly_mode === 3) v.dly_mode = 1; }
    if (m.rt != null) v.rev_type = m.rt;
    if (m.w) { v.dbl_on = 1; v.dbl_mix = Math.min (45, (v.dbl_mix || 10) + m.w * 9); v.dbl_width = Math.min (100, 70 + m.w * 10); if (m.w >= 2) v.dbl_voices = 1; }
    if (m.c) { v.comp_thresh = (v.comp_thresh || -22) - m.c * 3; v.comp_ratio = Math.min (12, (v.comp_ratio || 4) + m.c * 1.5); }
    return v;
  }

  // ---------------------------------------------------------------------------
  //  Presets de reverb (só mexem no ESPAÇO)
  // ---------------------------------------------------------------------------
  const R = (name, desc, v) => ({ name, desc, v: Object.assign ({ rev_on: 1 }, v) });
  const REVERB_PRESETS = [
    R("Plate vocal curto", "Brilho e corpo sem afastar a voz (trap/rap)", { rev_type: 1, rev_size: 40, rev_decay: 0.9, rev_predelay: 25, rev_damp: 35, rev_diff: 75, rev_er: 30, rev_lowcut: 400, rev_highcut: 9000, rev_mix: 14, rev_duck: 30 }),
    R("Plate brilhante", "Plate aberto para hooks", { rev_type: 1, rev_size: 60, rev_decay: 1.6, rev_predelay: 30, rev_damp: 25, rev_diff: 80, rev_lowcut: 350, rev_highcut: 12000, rev_mix: 16, rev_duck: 25 }),
    R("Hall gigante", "Espaço cinematográfico (Travis/Weeknd)", { rev_type: 2, rev_size: 95, rev_decay: 4.2, rev_predelay: 45, rev_damp: 45, rev_diff: 80, rev_mod: 45, rev_lowcut: 300, rev_highcut: 8000, rev_mix: 22, rev_duck: 40 }),
    R("Hall R&B sedoso", "Longo, escuro e macio", { rev_type: 2, rev_size: 75, rev_decay: 2.6, rev_predelay: 50, rev_damp: 60, rev_diff: 70, rev_mod: 35, rev_lowcut: 250, rev_highcut: 7000, rev_mix: 20, rev_duck: 30 }),
    R("Sala pequena", "Ambiente natural, voz próxima", { rev_type: 0, rev_size: 25, rev_decay: 0.5, rev_predelay: 8, rev_damp: 50, rev_diff: 60, rev_er: 55, rev_mix: 10 }),
    R("Câmara vintage", "Câmara densa e quente", { rev_type: 3, rev_size: 50, rev_decay: 1.4, rev_predelay: 15, rev_damp: 55, rev_diff: 70, rev_er: 45, rev_lowcut: 200, rev_highcut: 7000, rev_mix: 14 }),
    R("Mola (reggae)", "Reflexões esparsas tipo reverb de mola", { rev_type: 3, rev_size: 30, rev_decay: 1.8, rev_predelay: 10, rev_damp: 65, rev_diff: 25, rev_er: 75, rev_lowcut: 400, rev_highcut: 5000, rev_mix: 16 }),
    R("Ambiente drill", "Quase seco, só ar", { rev_type: 4, rev_size: 25, rev_decay: 0.6, rev_predelay: 35, rev_damp: 40, rev_diff: 60, rev_lowcut: 300, rev_mix: 7 }),
    R("Plug espacial", "Grande e modulado, abre em volta", { rev_type: 2, rev_size: 85, rev_decay: 3.0, rev_predelay: 35, rev_damp: 35, rev_diff: 85, rev_mod: 60, rev_lowcut: 300, rev_highcut: 9000, rev_mix: 24, rev_duck: 45 }),
    R("Funk curtinho", "Só um brilho de sala", { rev_type: 4, rev_size: 20, rev_decay: 0.45, rev_predelay: 5, rev_mix: 6 }),
    R("Catedral", "Enorme e longo (efeito)", { rev_type: 2, rev_size: 100, rev_decay: 9, rev_predelay: 80, rev_damp: 30, rev_diff: 85, rev_mod: 50, rev_mix: 35 }),
    R("Fantasma", "Voz distante, quase só reverb", { rev_type: 2, rev_size: 100, rev_decay: 6, rev_predelay: 120, rev_damp: 50, rev_mod: 70, rev_mix: 45, rev_duck: 0 }),
    R("Garagem", "Sala dura e reflexiva", { rev_type: 0, rev_size: 45, rev_decay: 1.1, rev_predelay: 5, rev_damp: 15, rev_diff: 40, rev_er: 70, rev_mix: 14 }),
    R("Pop polido", "Plate médio clássico de rádio", { rev_type: 1, rev_size: 55, rev_decay: 1.5, rev_predelay: 30, rev_damp: 40, rev_diff: 75, rev_lowcut: 350, rev_highcut: 10000, rev_mix: 14, rev_duck: 25 }),
    R("Túnel escuro", "Longo, sem agudos (dark trap)", { rev_type: 2, rev_size: 80, rev_decay: 3.4, rev_predelay: 25, rev_damp: 85, rev_diff: 70, rev_highcut: 4500, rev_mix: 22 }),
    R("Ad-lib estéreo", "Reverb largo para ad-libs", { rev_type: 1, rev_size: 65, rev_decay: 1.8, rev_predelay: 20, rev_width: 100, rev_mix: 26, rev_lowcut: 500 })
  ];

  // ---------------------------------------------------------------------------
  //  Classificação de estilo pelo DNA do beat
  // ---------------------------------------------------------------------------
  function gauss (x, mu, sd) { return Math.exp (-0.5 * Math.pow ((x - mu) / sd, 2)); }
  function bpmScore (bpm, lo, hi) {
    let best = 0;
    for (const b of [bpm, bpm / 2, bpm * 2]) { const mid = (lo + hi) / 2, half = (hi - lo) / 2; best = Math.max (best, b >= lo && b <= hi ? 1 : gauss (b, mid, half + 6) * (b === bpm ? 1 : 0.85)); }
    return best;
  }
  function classifyStyle (f) {
    if (!f || !f.valid) return [];
    const bpm = f.bpm || 120, trip = Math.max (f.tripletRatio || 0, f.tripletRatioHalf || 0), sub = f.subSustain || 0, cen = f.centroidHz || 1500, crest = f.crestDb || 14;
    const off = f.offbeatRatioHalf || 0, kicks = f.kickRate || 2, hats = f.hatRate || 4;
    const S = {
      trap:   bpmScore (bpm, 128, 165) * (sub > 0.35 ? 1.3 : 0.6) * (0.6 + trip * 0.8),
      drill:  bpmScore (bpm, 138, 146) * (sub > 0.35 ? 1.2 : 0.5) * (0.4 + trip * 1.4) * 0.9,
      plug:   bpmScore (bpm, 140, 165) * (0.5 + sub * 0.6) * (hats < 5 ? 1.1 : 0.75) * (cen > 1200 ? 1.1 : 0.85) * 0.85,
      funk:   bpmScore (bpm, 125, 135) * (kicks > 3 ? 1.25 : 0.7) * (trip < 0.2 ? 1.1 : 0.6),
      reggae: bpmScore (bpm, 60, 95) * (off > 1.3 ? 1.5 : 0.7) * (sub < 0.3 ? 1.1 : 0.35) * 0.9,
      rnb:    bpmScore (bpm, 60, 100) * (crest > 15 ? 1.15 : 0.8) * (hats < 4 ? 1.1 : 0.8) * (sub < 0.45 ? 1 : 0.5) * 0.9,
      pop:    bpmScore (bpm, 95, 130) * (kicks > 1.6 && kicks < 3 ? 1.1 : 0.8) * (trip < 0.25 ? 1.1 : 0.7) * 0.85,
      rap:    bpmScore (bpm, 80, 98) * (trip < 0.25 ? 1.1 : 0.7) * 0.8
    };
    // funk 150 (mandelão moderno)
    S.funk = Math.max (S.funk, bpmScore (bpm, 145, 155) * (kicks > 3 ? 1.1 : 0.5) * (trip < 0.2 ? 1 : 0.5));
    const total = Object.values (S).reduce ((a, b) => a + b, 0) || 1;
    return Object.entries (S).map (([k, v]) => ({ style: k, name: STYLES[k].name, p: v / total })).sort ((a, b) => b.p - a.p);
  }

  // ---------------------------------------------------------------------------
  //  Diagnóstico voz x beat (32 bandas log 20 Hz – 20 kHz, em dB)
  // ---------------------------------------------------------------------------
  const bandHz = (b) => 20 * Math.pow (1000, (b + 0.5) / 32);
  const bandsIn = (lo, hi) => { const out = []; for (let b = 0; b < 32; b++) { const f = bandHz (b); if (f >= lo && f < hi) out.push (b); } return out; };
  const avg = (arr, idx) => idx.reduce ((s, i) => s + arr[i], 0) / Math.max (1, idx.length);

  function diagnoseMix ({ voiceSpec, beatSpec, voice, beat, values }) {
    const issues = []; let score = 100;
    const add = (pen, title, detail, fix) => { score -= pen; issues.push ({ title, detail, fix: fix || {} }); };
    if (voiceSpec && voiceSpec.length === 32) {
      const vAll = avg (voiceSpec, bandsIn (150, 8000));
      const mud = avg (voiceSpec, bandsIn (200, 450)) - vAll;
      const nasal = avg (voiceSpec, bandsIn (800, 1400)) - vAll;
      const harsh = avg (voiceSpec, bandsIn (2500, 5000)) - vAll;
      const air = avg (voiceSpec, bandsIn (10000, 16000)) - vAll;
      const boom = avg (voiceSpec, bandsIn (40, 110)) - vAll;
      if (mud > 4) add (Math.min (15, mud * 2), "Voz embolada (lama em 200–450 Hz)", `Os médios-graves da voz estão ${mud.toFixed (1)} dB acima da média.`, { eq_on: 1, eq_m1_freq: 300, eq_m1_q: 1.3, eq_m1_gain: -Math.min (6, mud * 0.8) });
      if (nasal > 4) add (Math.min (10, nasal * 1.5), "Voz nasal (800 Hz–1,4 kHz)", `Excesso de ${nasal.toFixed (1)} dB na região nasal.`, { eq_on: 1, eq_m1_freq: 1000, eq_m1_q: 2, eq_m1_gain: -Math.min (5, nasal * 0.7) });
      if (harsh > 5) add (Math.min (12, harsh * 1.5), "Voz ardida (2,5–5 kHz)", `A presença está ${harsh.toFixed (1)} dB acima da média — cansa o ouvido.`, { eq_on: 1, eq_m2_freq: 3500, eq_m2_gain: -Math.min (4, (harsh - 3) * 0.8), ds_on: 1, ds_thresh: Math.min (values.ds_thresh, -32) });
      if (air < -20) add (8, "Falta ar no topo", `Acima de 10 kHz a voz está ${(-air).toFixed (0)} dB abaixo — soa fechada.`, { air_on: 1, air_high: Math.max (values.air_high, 40), eq_high_gain: Math.max (values.eq_high_gain, 2.5) });
      if (boom > -3) add (6, "Grave sobrando na voz", "Tem energia abaixo de 110 Hz que só embola com o 808.", { eq_on: 1, eq_hpf: Math.max (values.eq_hpf, 110) });
      if (beatSpec && beatSpec.length === 32) {
        // voz x beat: onde a voz precisa ganhar
        const pres = bandsIn (1000, 5000), low = bandsIn (180, 500), sub = bandsIn (40, 120);
        const presMargin = avg (voiceSpec, pres) - avg (beatSpec, pres);
        const lowMargin = avg (voiceSpec, low) - avg (beatSpec, low);
        if (presMargin < 0) add (Math.min (20, -presMargin * 3), "Voz escondida no beat", `Na faixa de inteligibilidade (1–5 kHz) o beat está ${(-presMargin).toFixed (1)} dB acima da voz.`,
            { eq_on: 1, eq_m2_freq: findGap (voiceSpec, beatSpec, 2000, 6000), eq_m2_gain: Math.min (5, (values.eq_m2_gain || 0) + 1.5 - presMargin * 0.4), air_on: 1, air_mid: Math.max (values.air_mid, 25) });
        else if (presMargin < 3) add (6, "Voz brigando com o beat", `A voz está só ${presMargin.toFixed (1)} dB acima do beat em 1–5 kHz.`, { eq_on: 1, eq_m2_freq: findGap (voiceSpec, beatSpec, 2000, 6000), eq_m2_gain: Math.min (4, (values.eq_m2_gain || 0) + 1.5) });
        if (lowMargin > -2 && avg (beatSpec, low) > avg (beatSpec, bandsIn (150, 8000)) + 2) add (8, "Voz e beat embolando nos médios-graves", "Os dois ocupam 180–500 Hz ao mesmo tempo.", { eq_on: 1, eq_m1_freq: 280, eq_m1_gain: Math.min (values.eq_m1_gain, -3), eq_hpf: Math.max (values.eq_hpf, 100) });
        if (avg (beatSpec, sub) - avg (beatSpec, bandsIn (150, 8000)) > 8 && values.eq_hpf < 100) add (4, "Beat com 808 pesado", "Limpe o grave da voz para o 808 respirar.", { eq_hpf: 115 });
      }
    }
    if (voice) {
      if (voice.noiseFloorDb > -55) add (10, "Ruído de fundo na gravação", `Piso de ruído em ${Math.round (voice.noiseFloorDb)} dB.`, { nr_on: 1, nr_amount: Math.min (80, (voice.noiseFloorDb + 72) * 2.5), gate_on: 1, gate_thresh: voice.noiseFloorDb + 8 });
      if (voice.sibilanceDb > -14) add (8, "Muito S (sibilância)", "Os 'S' estão saltando.", { ds_on: 1, ds_thresh: -36, ds_range: 10 });
      if (voice.crestDb > 18 && !(values.comp_on > 0.5 && values.comp_ratio >= 5)) add (8, "Voz muito irregular (volume pula)", `Crest factor de ${voice.crestDb.toFixed (0)} dB.`, { comp_on: 1, comp_ratio: 6, comp_thresh: Math.max (-40, voice.rmsDb + 6) });
      if (voice.peakDb > -0.3) add (10, "Gravação clipando", "A voz bate em 0 dBFS — grave com picos entre -12 e -6 dBFS.", {});
    }
    score = Math.max (0, Math.round (score));
    const verdict = score >= 88 ? "Tá da hora! Só detalhe fino." : score >= 72 ? "Tá bom, mas dá pra melhorar." : score >= 50 ? "Tá embolando — bora arrumar." : "Precisa de trabalho — aplica as correções.";
    const fixAll = Object.assign ({}, ...issues.map ((i) => i.fix));
    return { score, verdict, issues, fixAll };
  }
  function findGap (v, b, lo, hi) {   // frequência onde a voz tem mais vantagem sobre o beat
    let best = 3000, bestM = -1e9;
    for (const i of bandsIn (lo, hi)) { const m = (v[i] - b[i]) - Math.abs (Math.log2 (bandHz (i) / 3500)) * 1.5; if (m > bestM) { bestM = m; best = bandHz (i); } }
    return Math.round (best / 50) * 50;
  }

  // ---------------------------------------------------------------------------
  //  Casamento com referência (música do artista)
  // ---------------------------------------------------------------------------
  function matchReference ({ reference, beat, values }) {
    if (!reference || !reference.valid) return null;
    const ch = {}; const notes = [];
    const refStyle = classifyStyle (reference)[0];
    if (refStyle) { Object.assign (ch, styleValues (refStyle.style)); notes.push (`estilo da referência: ${refStyle.name} (${Math.round (refStyle.p * 100)}%)`); }
    if (reference.lufs > -30) { ch.lufs_target = Math.round (reference.lufs * 2) / 2; notes.push (`alvo de volume igual ao da referência (${reference.lufs.toFixed (1)} LUFS)`); }
    // brilho: compara o topo (10–16 kHz) relativo à média, referência x beat
    if (beat && beat.valid && reference.spectrum && beat.spectrum) {
      const top = bandsIn (9000, 16000), mid = bandsIn (300, 4000);
      const refTop = avg (reference.spectrum, top) - avg (reference.spectrum, mid), beatTop = avg (beat.spectrum, top) - avg (beat.spectrum, mid);
      const d = refTop - beatTop;
      if (Math.abs (d) > 1.5) { ch.air_on = 1; ch.air_high = Math.max (0, Math.min (70, (values.air_high || 15) + d * 4)); ch.tilt = Math.max (-3, Math.min (3, d * 0.3)); notes.push (d > 0 ? "referência mais brilhante: subi o ar e o tilt" : "referência mais escura: tirei brilho"); }
    }
    ch.img_width = Math.round (90 + (reference.stereoWidth || 0.4) * 60);
    return { changes: ch, notes };
  }

  window.Knowledge = { STYLES, ARTISTS, REVERB_PRESETS, styleValues, artistPreset, classifyStyle, diagnoseMix, matchReference };
})();
