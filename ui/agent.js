// =============================================================================
//  CUPIDO — Cupidinho, o agente
//  1) Modo LOCAL (sempre disponível, offline): entende português e gíria
//     ("bota mais saturação nos agudos", "delay mais demorado", "estilo Matuê")
//     com regras de engenharia de mixagem.
//  2) Modo CLAUDE (se tiver chave): manda o pedido + estado atual para a API do
//     Claude e aplica o JSON que volta. Se falhar, cai pro modo local.
//  3) AUTO-MIX: usa a análise da voz e do beat (BPM, tom, balanço espectral,
//     ruído, sibilância, dinâmica) para montar a mixagem.
// =============================================================================
window.Agent = (function () {
  const spec = window.CUPIDO_SPEC;
  const byId = {}; spec.forEach((p) => (byId[p.id] = p));
  const NOTE_ORDER = [2, 3, 5, 6, 8, 9, 10, 11];       // 1/16, 1/16., 1/8, 1/8., 1/4, 1/4., 1/2, 1/1
  const KEYS = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const SOLFEGE = { do: 0, re: 2, mi: 4, fa: 5, sol: 7, la: 9, si: 11, c: 0, d: 2, e: 4, f: 5, g: 7, a: 9, b: 11 };

  const norm = (s) => " " + s.toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "").replace(/[!?.;:()"“”]/g, " ").replace(/\s+/g, " ") + " ";
  const has = (t, re) => re.test(t);
  const pick = (arr) => arr[Math.floor(Math.random() * arr.length)];

  // ---------- contexto de mudanças (valores atuais + o que já mudou neste comando) ----------
  function makeCtx(values) {
    const changes = {};
    const get = (id) => (id in changes ? changes[id] : values[id]);
    const clamp = (id, v) => { const p = byId[id]; v = Math.min(p.max, Math.max(p.min, v)); return p.step >= 1 ? Math.round(v) : Math.round(v * 100) / 100; };
    const set = (id, v) => { if (!byId[id]) return; changes[id] = clamp(id, v); };
    const on = (id) => set(id, 1);
    // passo relativo: fração do range (linear) ou fator (log)
    const step = (id, frac, dir) => {
      const p = byId[id]; const cur = get(id);
      if (p.skew === 1) set(id, cur * Math.pow(p.max / p.min, frac * dir * 0.5));
      else set(id, cur + (p.max - p.min) * frac * dir);
    };
    return { changes, get, set, on, step };
  }

  function intensity(t) {
    if (has(t, / (no talo|no maximo|tudo que der|full|ao maximo|maximo) /)) return 2.6;
    if (has(t, / (bem|muito|mt|bastante|demais|pra caralho|pra krl|pacas|absurdo|forte|monte|bem mais|muito mais) /)) return 1.8;
    if (has(t, / (um pouco|pouquinho|pouco|leve|levemente|de leve|sutil|um tico|um toque|discreto|suave mente) /)) return 0.5;
    return 1;
  }
  function direction(t) {
    if (has(t, / (desliga|sem |nada de|tira (o|a|os|as)? ?(todo|toda)?|remove|zera o|zera a) /)) return -2;      // -2 = desligar
    if (has(t, / (menos|diminui|diminuir|abaixa|baixa|reduz|reduzir|tira|tirar|corta|cortar|segura|suaviza|alivia|exagerado|demais o|muito alto) /)) return -1;
    return 1;
  }

  // ---------- reconhecimento de tom ----------
  function parseKey(t) {
    const m = t.match(/ (?:tom|tonalidade|key|escala|afina(?:r|cao)? em|autotune em|em) (?:de |em |: )?(do|re|mi|fa|sol|la|si|[a-g])( ?#| sustenido| bemol|b(?= |m ))? ?(menor|minor|m(?= )|maior|major)? /);
    if (!m) return null;
    const isSolfege = m[1].length > 1;
    if (!isSolfege && !m[3] && !/ (tom|key|tonalidade|escala) /.test(t)) return null;   // "em a" solto é ambíguo
    let k = SOLFEGE[m[1]];
    if (m[2]) { if (/#|sustenido/.test(m[2])) k += 1; else k -= 1; }
    k = (k + 12) % 12;
    const minor = m[3] ? /menor|minor|^m$/.test(m[3].trim()) : null;
    return { key: k, minor };
  }

  // ---------- presets por nome/tag ----------
  function findPreset(t) {
    let best = null, bestLen = 0, word = "";
    for (const p of window.CUPIDO_PRESETS) {
      const words = [p.name, ...(p.tags || [])].map((w) => norm(w).trim()).filter(Boolean);
      for (const w of words) if (w.length >= 3 && t.includes(" " + w + " ") && w.length > bestLen) { best = p; bestLen = w.length; word = w; }
    }
    return best ? { preset: best, word } : null;
  }

  // =========================================================================
  //  Interpretação local
  // =========================================================================
  function interpret(text, values, presetsSoFar) {
    const t = norm(text);
    const ctx = makeCtx(values);
    const notes = []; const actions = []; let preset = null;

    // -------- ações globais --------
    if (has(t, / (desfaz|desfazer|volta como (tava|estava)|voltar como era|undo) /)) return { undo: true, reply: "Voltei pro jeito que tava." };
    if (has(t, / (zera|zerar|reseta|resetar|reset|do zero|limpa tudo|padrao de fabrica) /)) {
      preset = window.CUPIDO_PRESETS[0]; notes.push("zerei tudo pro padrão");
    }
    const wantsMix = has(t, / (mixa|mixar|mixagem|mixe|deixa (perfeito|top|zica|pica|bala|profissional|bom)|arruma (tudo|o som|a voz)|faz (a|uma) mix|auto ?mix|automatic[oa]|deixa no ponto) /);
    const wantsBeat = has(t, / (identifica|analisa|ouve|escuta|le|detecta|reconhece) (o |a )?(beat|batida|instrumental|base)|qual (o |e o )?(tom|bpm)|tom do beat|bpm do beat|no tom do beat|afina no beat /);
    if (wantsMix) actions.push("analyze_beat", "analyze_voice", "auto_mix");
    else if (wantsBeat) actions.push("analyze_beat");
    if (has(t, / (aprende|aprender|captura|pega) (o )?(ruido|barulho|chiado)|perfil de ruido /)) { actions.push("learn_noise"); ctx.on("nr_on"); notes.push("vou aprender o ruído: fica 2 segundos em silêncio"); }
    if (has(t, / (masteriza|masterizar|masterizacao|master) /) && !preset) {
      preset = window.CUPIDO_PRESETS.find((p) => p.name === (has(t, / (spotify|streaming|youtube|dinamic) /) ? "Master Streaming -14" : has(t, / (quente|analogic|vintage) /) ? "Master Quente" : "Master Trap -8"));
      notes.push("modo master: glue, tilt e limiter no alvo de loudness");
    }

    // -------- estilo / preset --------
    if (!preset) {
      const found = findPreset(t);
      const strongWords = ["matue", "tue", "30praum", "teto", "yunk", "vino", "yunk vino", "plug", "pluggnb", "drill", "rage", "funk", "mandelao", "t-pain", "tpain", "podcast", "fantasma", "boom bap"];
      if (found && (has(t, / (estilo|tipo o|tipo a|igual|parecid\w*|que nem|vibe|voz d\w*|preset|som d\w*|jeito d\w*|referencia) /) || strongWords.includes(found.word) || found.word === norm(found.preset.name).trim())) {
        preset = found.preset; notes.push(`carreguei o preset "${preset.name}" (${preset.desc.split(".")[0].toLowerCase()})`);
      }
    }

    // -------- quebra em frases ("e", vírgula, "também", "mas") --------
    const clauses = t.split(/ e | mas | tambem | ai | depois | , |,| porem | so que /).map((c) => " " + c.trim() + " ").filter((c) => c.trim().length > 1);
    const base = preset ? Object.assign({}, specDefaults(), preset.values) : null;
    if (base) values = Object.assign({}, values, base);
    const c2 = makeCtx(values); Object.assign(c2.changes, ctx.changes);
    const C = c2;

    const done = {};
    for (const c of clauses) {
      const k = intensity(c), d = direction(c);
      const up = d > 0 ? 1 : -1, off = d === -2;
      let matched = false;

      // tom explícito
      const key = parseKey(c);
      if (key) {
        C.on("tune_on"); C.set("tune_key", key.key);
        if (key.minor !== null) C.set("tune_scale", key.minor ? 2 : 1);
        notes.push(`autotune em ${KEYS[key.key]}${key.minor === null ? "" : key.minor ? " menor" : " maior"}`); matched = true;
      }

      // ---------- AUTOTUNE ----------
      if (has(c, / (auto ?-?tune|afinacao|afinar|afina|afinad|tune|pitch|robo|robotico|t-?pain|hard ?tune) /)) {
        matched = true;
        if (off) { C.set("tune_on", 0); notes.push("desliguei o autotune"); }
        else {
          C.on("tune_on");
          if (has(c, / (robo|robotico|t-?pain|duro|hard|seco|cravado|travado|maximo) /)) { C.set("tune_speed", 0); C.set("tune_humanize", 0); C.set("tune_amount", 100); notes.push("autotune robótico (velocidade 0)"); }
          else if (has(c, / (natural|humano|disfarcad|discreto|transparente|nao aparecer|invisivel) /)) { C.set("tune_speed", 45); C.set("tune_humanize", 35); notes.push("afinação natural (velocidade 45 ms, humanizar 35%)"); }
          else if (has(c, / (melodic|moderno|trap) /)) { C.set("tune_speed", 15); C.set("tune_humanize", 5); notes.push("autotune melódico (15 ms)"); }
          else if (!key || has(c, / (mais|menos|forte|fraco|aumenta|diminui) /)) { C.set("tune_speed", C.get("tune_speed") * (up > 0 ? 1 / (1 + 0.6 * k) : 1 + 0.8 * k)); C.step("tune_amount", 0.15 * k, up); notes.push(up > 0 ? "autotune mais forte" : "autotune mais suave"); }
          const oct = c.match(/ (oitava|8va) (pra |para )?(cima|acima|abaixo|baixo) /);
          if (oct) { C.set("tune_transpose", /cima|acima/.test(oct[3]) ? 12 : -12); notes.push("transposição de oitava"); }
        }
      }
      const semi = c.match(/ (sobe|desce|transp\w*) (\d+) (semitons?|tons?) /);
      if (semi) { const n = parseInt(semi[2], 10) * (/tons? $/.test(semi[3] + " ") && !/semi/.test(semi[3]) ? 2 : 1); C.on("tune_on"); C.set("tune_transpose", semi[1] === "desce" ? -n : n); matched = true; notes.push(`transpus ${semi[1] === "desce" ? "-" : "+"}${n} semitons`); }

      // ---------- SATURAÇÃO (com região) ----------
      if (has(c, / (satura\w*|drive|sujeira|sujo|suja|distorc\w*|grit|crocancia|esquenta|calor|harmonic\w*|fuzz|valvula|fita|tape) /)) {
        matched = true;
        if (off) { C.set("sat_on", 0); notes.push("tirei a saturação"); }
        else {
          C.on("sat_on");
          const types = [[/ (valvula|tube|valvulad) /, 0, "válvula"], [/ (fita|tape|k7) /, 1, "fita"], [/ (transistor) /, 2, "transistor"], [/ (quente|calor|warm|esquenta) /, 0, "válvula (quente)"], [/ (digital|clip|clipado) /, 4, "digital"], [/ (fuzz|rage|rasgad) /, 5, "fuzz"]];
          for (const [re, idx, nm] of types) if (has(c, re)) { C.set("sat_type", idx); notes.push(`saturação tipo ${nm}`); break; }
          const amt = 3 * k * up;
          if (has(c, / (agud\w*|brilho|topo|high|em cima|alta\w*|aguda) /)) { C.set("sat_b4_amt", C.get("sat_b4_amt") + amt * 0.7); C.set("sat_b5_amt", C.get("sat_b5_amt") + amt); notes.push(`${up > 0 ? "mais" : "menos"} saturação nos agudos (bandas 4.5 kHz e 10 kHz do Prisma)`); }
          else if (has(c, / (medio\w*|mid|meio) /)) { C.set("sat_b3_amt", C.get("sat_b3_amt") + amt); notes.push(`${up > 0 ? "mais" : "menos"} saturação nos médios`); }
          else if (has(c, / (grave\w*|low|corpo|embaixo|baixo) /)) { C.set("sat_b1_amt", C.get("sat_b1_amt") + amt * 0.8); C.set("sat_b2_amt", C.get("sat_b2_amt") + amt); notes.push(`${up > 0 ? "mais" : "menos"} saturação nos graves`); }
          else { C.step("sat_drive", 0.15 * k, up); C.set("sat_b3_amt", C.get("sat_b3_amt") + amt * 0.6); C.set("sat_b4_amt", C.get("sat_b4_amt") + amt * 0.5); notes.push(`${up > 0 ? "mais" : "menos"} drive no Prisma`); }
          if (up > 0 && C.get("sat_mix") < 45) C.set("sat_mix", 55);
        }
      }

      // ---------- DELAY ----------
      if (has(c, / (delay|dilei|eco|repeticao|repeticoes|ping ?pong|slap) /)) {
        matched = true;
        if (off) { C.set("dly_on", 0); notes.push("desliguei o delay"); }
        else {
          C.on("dly_on"); C.set("dly_sync", 1);
          const idx = NOTE_ORDER.indexOf(C.get("dly_note")); const cur = idx < 0 ? 4 : idx;
          if (has(c, / (demorad\w*|longo|lento|mais tempo|espacad\w*|arrastad\w*|atrasad\w*|tempo maior|mais devagar) /)) { const n = NOTE_ORDER[Math.min(NOTE_ORDER.length - 1, cur + (k > 1.4 ? 2 : 1))]; C.set("dly_note", n); C.step("dly_feedback", 0.05 * k, 1); notes.push(`delay mais demorado (${byId.dly_note.choices[n]})`); }
          else if (has(c, / (curto|rapido|ligeiro|mais perto|colado|tempo menor) /)) { const n = NOTE_ORDER[Math.max(0, cur - 1)]; C.set("dly_note", n); notes.push(`delay mais curto (${byId.dly_note.choices[n]})`); }
          if (has(c, / (repet\w*|feedback|cauda|vezes|repete) /)) { C.step("dly_feedback", 0.12 * k, up); notes.push(`${up > 0 ? "mais" : "menos"} repetições`); }
          const explicit = [[/ 1\/4 ?(pontuad\w*|\.) /, 9], [/ 1\/8 ?(pontuad\w*|\.) /, 6], [/ 1\/2 /, 10], [/ 1\/4 /, 8], [/ 1\/8 /, 5], [/ 1\/16 /, 2], [/ colcheia pontuada /, 6], [/ seminima pontuada /, 9], [/ colcheia /, 5], [/ seminima /, 8], [/ minima /, 10], [/ (tercina|triplet) /, 7]];
          for (const [re, n] of explicit) if (has(c, re)) { C.set("dly_note", n); notes.push(`delay em ${byId.dly_note.choices[n]}`); break; }
          if (has(c, / ping ?pong /)) { C.set("dly_mode", 1); notes.push("modo ping-pong"); }
          if (has(c, / slap /)) { C.set("dly_mode", 3); notes.push("slap curtinho"); }
          if (has(c, / (fita|tape|sujo|analogic\w*) /)) { C.step("dly_drive", 0.25 * k, 1); C.step("dly_mod", 0.15 * k, 1); }
          if (!has(c, / (demorad|longo|lento|curto|rapido|repet|feedback|1\/|colcheia|seminima|ping|slap|tercina)/)) { C.step("dly_mix", 0.07 * k, up); notes.push(`${up > 0 ? "mais" : "menos"} delay`); }
        }
      }

      // ---------- REVERB ----------
      if (has(c, / (reverb\w*|reverber\w*|reverbe|ambiencia|ambiente|sala|hall|plate|catedral|espaco|espacial|igreja) /) && !has(c, / estereo /)) {
        matched = true;
        if (off) { C.set("rev_on", 0); notes.push("desliguei o reverb"); }
        else {
          C.on("rev_on");
          if (has(c, / (grande|enorme|gigante|longo|catedral|igreja|infinito) /)) { C.step("rev_decay", 0.25 * k, 1); C.step("rev_size", 0.2 * k, 1); notes.push("reverb maior e mais longo"); }
          else if (has(c, / (curto|pequeno|apertado|seco) /)) { C.step("rev_decay", 0.25 * k, -1); C.step("rev_size", 0.15 * k, -1); notes.push("reverb mais curto"); }
          const types = [[/ sala /, 0], [/ plate /, 1], [/ hall /, 2], [/ camara /, 3], [/ ambiente|ambiencia /, 4]];
          for (const [re, n] of types) if (has(c, re)) { C.set("rev_type", n); notes.push(`reverb tipo ${byId.rev_type.choices[n]}`); break; }
          if (!has(c, / (grande|enorme|gigante|longo|catedral|curto|pequeno|apertado) /)) { C.step("rev_mix", 0.06 * k, up); notes.push(`${up > 0 ? "mais" : "menos"} reverb`); }
        }
      }
      if (has(c, / (molhad\w*|wet|lavad\w*|banhad\w*) /)) { matched = true; C.on("rev_on"); C.on("dly_on"); C.step("rev_mix", 0.08 * k, up); C.step("dly_mix", 0.06 * k, up); notes.push("vocal mais molhado (reverb + delay)"); }
      if (has(c, / (sec[oa]|dry|na cara|cara a cara|colad[oa] na cara) /) && !has(c, / (reverb|delay|eco) /) && !done.dry && (done.dry = true)) { matched = true; C.step("rev_mix", 0.08 * k, -1); C.step("dly_mix", 0.06 * k, -1); C.step("comp_ratio", 0.05 * k, 1); notes.push("vocal mais seco e na cara"); }

      // ---------- ABERTO / BRILHO ----------
      if (has(c, / (abert[oa]|abrir|abre|arejad\w*|ar |brilh\w*|crocante|cristalin\w*|claro|clara|aereo|ceu|sparkle|air) /) && !has(c, / (estereo|largura|lados) /)) {
        matched = true; const u = has(c, / (menos|tira|diminui|abaixa) /) ? -1 : 1;
        C.on("air_on"); C.step("air_high", 0.14 * k, u); C.step("air_mid", 0.08 * k, u); C.on("eq_on"); C.set("eq_high_gain", C.get("eq_high_gain") + 1.5 * k * u);
        if (u > 0) { C.set("eq_m1_gain", Math.min(C.get("eq_m1_gain"), -2.5)); if (C.get("ds_on") < 0.5) C.on("ds_on"); }
        notes.push(u > 0 ? "vocal mais aberto (High/Mid Air + agudo, tirei lama em 300 Hz)" : "menos brilho");
      }
      if (has(c, / (fechad\w*|abafad\w*|escur[oa]|opac[oa]|apagad[oa]|sem vida|sem brilho|morto) /)) {
        matched = true;
        if (has(c, / (quero|deixa|bota|mais) (mais )?(fechad|escur|abafad) /)) { C.step("air_high", 0.15 * k, -1); C.set("eq_high_gain", C.get("eq_high_gain") - 2 * k); notes.push("deixei mais escuro"); }
        else { C.on("air_on"); C.step("air_high", 0.15 * k, 1); C.set("eq_m1_gain", C.get("eq_m1_gain") - 2 * k); C.set("eq_high_gain", C.get("eq_high_gain") + 2 * k); notes.push("tirei o abafado (cortei 300 Hz, levantei o topo)"); }
      }
      if (has(c, / (estridente|ardid[oa]|machuca\w*|furand\w*|dur[oa]|metalic\w*|agressiv[oa] demais|harsh|irritante) /)) {
        matched = true; C.on("eq_on"); C.set("eq_m2_freq", 3400); C.set("eq_m2_gain", Math.min(C.get("eq_m2_gain"), 0) - 2.5 * k); C.on("ds_on"); C.step("ds_thresh", 0.06 * k, -1);
        notes.push("aliviei a aspereza em 3.4 kHz e o de-esser trabalha mais");
      }

      // ---------- SIBILÂNCIA ----------
      if (has(c, / (sibil\w*|esses|s muito|chiado d[oa]s? s|ssss|de-?esser|deesser|chiand\w*) /)) {
        matched = true; C.on("ds_on");
        if (off || has(c, / (menos de-?esser|tira o de-?esser|ceceando|lingua presa|sem s) /)) { C.step("ds_thresh", 0.12 * k, 1); C.step("ds_range", 0.1 * k, -1); notes.push("de-esser mais leve"); }
        else { C.step("ds_thresh", 0.1 * k, -1); C.step("ds_range", 0.1 * k, 1); notes.push("de-esser segurando mais os S"); }
      }

      // ---------- RUÍDO ----------
      if (has(c, / (ruido|barulho|chiado|hiss|ventilador|ar condicionado|zumbido|hum |fundo sujo|sujeira de fundo|vazamento|ronco) /) && !has(c, / chiado d[oa]s? s /)) {
        matched = true;
        if (off && has(c, / (desliga|sem) (a )?(reducao|limpeza) /)) { C.set("nr_on", 0); notes.push("desliguei a redução de ruído"); }
        else { C.on("nr_on"); C.step("nr_amount", 0.15 * k, 1); C.on("gate_on"); notes.push("limpei o ruído de fundo (redução espectral + gate)"); }
      }

      // ---------- COMPRESSÃO ----------
      if (has(c, / (comprim\w*|compress\w*|pressao|consistente|nivelad\w*|pesad[oa]|punch|soco|pancada|estavel|constante) /)) {
        matched = true; C.on("comp_on");
        const u = has(c, / (menos|tira|alivia|mais dinamic\w*|respirar|natural|solta) /) ? -1 : 1;
        C.set("comp_thresh", C.get("comp_thresh") - 3 * k * u); C.step("comp_ratio", 0.08 * k, u); C.set("comp_makeup", C.get("comp_makeup") + 1.5 * k * u);
        notes.push(u > 0 ? "mais compressão (threshold, ratio e makeup)" : "menos compressão, mais dinâmica");
      }

      // ---------- GRAVE / CORPO / LAMA / NASAL / PRESENÇA ----------
      if (has(c, / (grave|graves|corpo|gord[oa]|encorpad\w*|peso|cheio|cheia|magr[oa]|fin[oa]) /) && !has(c, / (satura|distor|drive|sujeira|oitava) /)) {
        matched = true; C.on("eq_on");
        let u = up; if (has(c, / (magr|fin) /) && up > 0 && !has(c, / mais (magr|fin) /)) u = 1;
        if (has(c, / (boom|embolad|demais|estourad) /)) u = -1;
        C.set("eq_low_freq", 170); C.set("eq_low_gain", C.get("eq_low_gain") + 2 * k * u);
        if (u < 0) C.step("eq_hpf", 0.15 * k, 1);
        notes.push(u > 0 ? "mais corpo (grave em 170 Hz)" : "menos grave (shelf + passa-altas)");
      }
      if (has(c, / (embolad\w*|lama|mud|boxy|caixa|oco|oca|abafado no meio) /) && !has(t, / (nasal|fanh[oa]|nariz) /)) { matched = true; C.on("eq_on"); C.set("eq_m1_freq", 320); C.set("eq_m1_q", 1.2); C.set("eq_m1_gain", C.get("eq_m1_gain") - 2.5 * k); notes.push("tirei a lama em 320 Hz"); }
      if (has(c, / (nasal|fanh[oa]|nariz) /)) { matched = true; C.on("eq_on"); C.set("eq_m1_freq", 1000); C.set("eq_m1_q", 2); C.set("eq_m1_gain", -3.5 * k); notes.push("cortei o nasal em 1 kHz"); }
      if (has(c, / (presenca|na frente|pra frente|inteligivel|entender (as )?palavras|clareza|dicao|articula\w*) /)) { matched = true; C.on("eq_on"); C.set("eq_m2_freq", 3000); C.set("eq_m2_gain", C.get("eq_m2_gain") + 2 * k * up); notes.push(`${up > 0 ? "mais" : "menos"} presença em 3 kHz`); }

      // ---------- LARGURA / DOUBLER ----------
      if (has(c, / (larg[oa]|largura|estereo|wide|espalh\w*|dobr\w*|double\w*|doubler|nos lados|abrir (o|a)? ?(som|voz|imagem)|abre nos lados|grosso|coro) /)) {
        matched = true;
        if (off) { C.set("dbl_on", 0); notes.push("desliguei o doubler"); }
        else { C.on("dbl_on"); C.step("dbl_mix", 0.12 * k, up); C.step("dbl_width", 0.1 * k, up); C.step("img_width", 0.08 * k, up); if (has(c, / (4|quatro|grosso|coro|bem dobrad) /)) C.set("dbl_voices", 1); notes.push(up > 0 ? "vocal mais largo (doubler + estéreo)" : "vocal mais estreito"); }
      }
      if (has(c, / (mono|estreit[oa]|centralizad[oa]|no meio) /) && !has(c, / grave mono /)) { matched = true; C.step("img_width", 0.2 * k, -1); C.step("dbl_mix", 0.1 * k, -1); notes.push("imagem mais estreita"); }

      // ---------- VOLUME / LOUDNESS ----------
      if (has(c, / (alt[oa]|volume|loud\w*|lufs|mais forte|estour\w*|explod\w*|baixinh[oa]|muito baixo) /) && !has(c, / (agudo|frequencia|alta frequencia) /)) {
        matched = true; C.on("lim_on");
        const u = has(c, / (baixinh|muito baixo|mais alto|aumenta o volume) /) ? 1 : up;
        C.set("lim_gain", C.get("lim_gain") + 2 * k * u);
        if (has(c, / (spotify|streaming|youtube) /)) { C.set("lufs_target", -14); C.set("lim_gain", Math.min(C.get("lim_gain"), 3)); }
        notes.push(u > 0 ? "mais volume no limiter (teto -1 dB)" : "menos volume");
      }
      if (has(c, / (grave mono|mono no grave) /)) { matched = true; C.set("img_monobass", 1); notes.push("grave em mono"); }

      // ---------- ligar/desligar módulos pelo nome ----------
      const mods = [["gate", "gate_on"], ["opto|la-?2a|la2a", "opto_on"], ["compressor|1176|fet", "comp_on"], ["prisma|spectre", "sat_on"], ["fresh air|excitador|exciter", "air_on"], ["limiter|limitador", "lim_on"], ["glue|cola", "glue_on"], ["eq|equalizador|equalizacao", "eq_on"]];
      for (const [re, id] of mods) if (new RegExp(` (liga|ativa|desliga|desativa|tira|bota|coloca) (o |a )?(${re}) `).test(c)) { const v = /desliga|desativa|tira/.test(c) ? 0 : 1; C.set(id, v); notes.push(`${v ? "liguei" : "desliguei"} ${re.split("|")[0]}`); matched = true; }

      if (!matched && clauses.length === 1 && !preset && !actions.length) {
        return { reply: pick(["Não peguei essa, mano. Tenta tipo: \"mais agudo\", \"delay mais longo\", \"autotune robótico\", \"estilo Teto\" ou \"mixa pra mim\".",
          "Bah, essa não entendi. Fala em termos de som: brilho, grave, delay, reverb, autotune, saturação, largura, volume…"]), changes: {} };
      }
    }

    const all = C.changes;   // o preset (se houver) é aplicado inteiro pela interface antes destas mudanças
    if (!notes.length && actions.length) notes.push(actions.includes("auto_mix") ? "vou ouvir a voz e o beat e montar a mix" : "vou ouvir o beat");
    const opener = pick(["Feito!", "Tá na mão!", "Bah, ficou zica!", "Pronto, tchê!", "Fechou!", "Mandei ver!"]);
    const uniq = [...new Set(notes)]; notes.length = 0; notes.push(...uniq);
    return { preset, changes: all, actions, notes, reply: notes.length ? `${opener} ${capital(notes.join("; "))}.` : "Beleza." };
  }

  function specDefaults() { const d = {}; spec.forEach((p) => (d[p.id] = p.def)); return d; }
  const capital = (s) => s.charAt(0).toUpperCase() + s.slice(1);

  // =========================================================================
  //  AUTO-MIX a partir da análise
  // =========================================================================
  function autoMix(voice, beat, values, hostBpm) {
    const C = makeCtx(values); const notes = [];
    const b = beat && beat.valid ? beat : null, v = voice && voice.valid ? voice : null;

    // tom
    const keySrc = b && b.keyConfidence > 0.3 ? b : (v && v.keyConfidence > 0.3 ? v : (b || v));
    if (keySrc && keySrc.key >= 0) {
      C.on("tune_on"); C.set("tune_key", keySrc.key); C.set("tune_scale", keySrc.minor ? 2 : 1);
      if (values.tune_on < 0.5) C.set("tune_speed", 15);
      notes.push(`tom ${KEYS[keySrc.key]} ${keySrc.minor ? "menor" : "maior"} (${keySrc === b ? "do beat" : "da voz"}) no autotune`);
    }
    // BPM
    const bpm = hostBpm > 0 ? hostBpm : (b && b.bpm > 0 ? b.bpm : 0);
    if (bpm > 0) {
      C.on("dly_on");
      if (hostBpm > 0) { C.set("dly_sync", 1); }
      else { C.set("dly_sync", 0); const beats = [0.125, 1 / 6, 0.25, 0.375, 1 / 3, 0.5, 0.75, 2 / 3, 1, 1.5, 2, 4][values.dly_note] || 0.75; C.set("dly_time", 60000 / bpm * beats); }
      if (values.dly_on < 0.5) { C.set("dly_note", bpm > 115 ? 6 : 9); C.set("dly_mix", 16); C.set("dly_feedback", 28); C.set("dly_duck", 50); C.set("dly_lowcut", 380); }
      notes.push(`delay travado em ${Math.round(bpm)} BPM`);
    }
    // voz
    if (v) {
      if (v.noiseFloorDb > -58) {
        C.on("nr_on"); C.set("nr_amount", Math.min(80, Math.max(25, (v.noiseFloorDb + 72) * 2.5)));
        C.on("gate_on"); C.set("gate_thresh", v.noiseFloorDb + 8);
        notes.push(`ruído de fundo em ${Math.round(v.noiseFloorDb)} dB: redução de ruído e gate ligados`);
      } else { C.on("gate_on"); C.set("gate_thresh", Math.max(-70, v.noiseFloorDb + 10)); }
      const activeRms = v.rmsDb + Math.min(10, v.crestDb * 0.35);
      C.on("comp_on"); C.set("comp_thresh", Math.max(-40, Math.min(-8, activeRms + 2)));
      C.set("comp_ratio", v.crestDb > 18 ? 7 : v.crestDb > 14 ? 5 : 4); C.set("comp_makeup", Math.min(10, Math.max(2, (v.crestDb - 8) * 0.5)));
      notes.push(`compressor calibrado pra dinâmica da voz (crest ${Math.round(v.crestDb)} dB)`);
      C.on("ds_on"); C.set("ds_thresh", v.sibilanceDb > -14 ? -36 : v.sibilanceDb > -18 ? -31 : -27); C.set("ds_range", v.sibilanceDb > -14 ? 10 : 7);
      if (v.sibilanceDb > -18) notes.push("bastante S na voz: de-esser mais firme");
      C.on("eq_on");
      if (v.bands[2] > 32) { C.set("eq_m1_freq", 300); C.set("eq_m1_gain", -Math.min(5, (v.bands[2] - 28) * 0.4)); notes.push("cortei lama em 300 Hz"); }
      if (v.bands[4] < 13) { C.set("eq_m2_freq", 3200); C.set("eq_m2_gain", Math.min(4, (14 - v.bands[4]) * 0.5 + 1)); }
      if (v.bands[5] < 6) { C.on("air_on"); C.set("air_high", Math.max(values.air_high, 38)); notes.push("faltava ar no topo: levantei o High Air"); }
    }
    // beat x voz (abrir espaço)
    if (b) {
      if (b.bands[0] + b.bands[1] > 55) { C.set("eq_hpf", Math.max(values.eq_hpf, 115)); notes.push("beat com muito grave: subi o passa-altas da voz pra não embolar"); }
      if (b.bands[4] > 14) { C.set("eq_m2_freq", 4500); C.on("air_on"); C.set("air_high", Math.max(C.get("air_high"), 42)); notes.push("beat ocupando 3 kHz: joguei a presença da voz pra 4.5 kHz e mais ar"); }
      if (b.crestDb < 10) { C.set("comp_ratio", Math.min(20, C.get("comp_ratio") + 1)); C.set("dly_duck", 60); C.set("rev_duck", 40); notes.push("beat denso: mais compressão e ducking nos efeitos"); }
      if (!values.rev_on || values.rev_on < 0.5) { C.on("rev_on"); C.set("rev_type", 1); C.set("rev_predelay", bpm > 0 ? Math.min(60, 60000 / bpm / 16) : 30); C.set("rev_mix", 13); }
    }
    C.on("lim_on"); C.set("lim_ceiling", -1);
    return { changes: C.changes, notes };
  }

  // =========================================================================
  //  Contexto compacto para a IA (Claude)
  // =========================================================================
  function contextFor(values, extra) {
    const vals = {}; spec.forEach((p) => (vals[p.id] = Math.round(values[p.id] * 100) / 100));
    return JSON.stringify(Object.assign({ valores: vals, presets_de_fabrica: window.CUPIDO_PRESETS.map((p) => p.name) }, extra || {}));
  }

  return { interpret, autoMix, contextFor, KEYS, norm };
})();
