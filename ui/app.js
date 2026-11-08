// =============================================================================
//  CUPIDO — aplicação da interface
// =============================================================================
(function () {
  "use strict";
  const $ = (s, el = document) => el.querySelector(s);
  const h = (tag, attrs = {}, ...kids) => {
    const el = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs || {})) {
      if (k === "class") el.className = v; else if (k === "style") el.style.cssText = v;
      else if (k.startsWith("on")) el.addEventListener(k.slice(2), v); else if (v !== false && v != null) el.setAttribute(k, v === true ? "" : v);
    }
    for (const kid of kids.flat()) if (kid != null && kid !== false) el.append(kid.nodeType ? kid : document.createTextNode(kid));
    return el;
  };
  const svg = (tag, attrs = {}) => { const el = document.createElementNS("http://www.w3.org/2000/svg", tag); for (const [k, v] of Object.entries(attrs)) el.setAttribute(k, v); return el; };

  // ---------------------------------------------------------------------------
  //  Estado
  // ---------------------------------------------------------------------------
  const S = {
    spec: window.CUPIDO_SPEC, byId: {}, values: {}, sel: "afina", aiTouched: {}, history: [], future: [],
    slots: { A: null, B: null }, slot: "A", presetName: "Init", presetCat: "Padrão", userPresets: [],
    analysis: { beat: null, voice: null }, settings: {}, info: {}, meters: {}, sp: [], bsp: [],
    hist: { pitch: [], gate: [], comp: [], opto: [], lufs: [], lim: [], deess: [] }, busy: false
  };
  const controls = {};       // id -> [fn(value)]
  const defaults = () => { const d = {}; S.spec.forEach((p) => (d[p.id] = p.def)); return d; };

  // ---------------------------------------------------------------------------
  //  Módulos (ordem = cadeia de sinal)
  // ---------------------------------------------------------------------------
  const MODULES = [
    { key: "limpa", name: "LIMPA", sub: "Gate + ruído", power: ["gate_on", "nr_on"], gr: "gate", viz: "gate",
      desc: "Tira o ruído de fundo antes de tudo — o autotune e o compressor agradecem.",
      groups: [{ title: "Noise gate", power: "gate_on", params: ["gate_thresh", "gate_range", "gate_attack", "gate_release"] },
               { title: "Redução de ruído", power: "nr_on", params: ["nr_amount", "nr_sens", "nr_adaptive"], extra: "learn" }] },
    { key: "afina", name: "AFINA", sub: "Autotune", power: ["tune_on"], viz: "pitch",
      desc: "Velocidade 0 = robótico (trap). 10–25 ms = melódico. 40+ ms = natural.",
      groups: [{ title: "Tom", params: ["tune_key", "tune_scale"], extra: "keys" },
               { title: "Correção", power: "tune_on", params: ["tune_speed", "tune_humanize", "tune_amount", "tune_transpose"] }] },
    { key: "eq", name: "EQ", sub: "Equalizador", power: ["eq_on"], viz: "eq",
      desc: "Arraste os pontos no gráfico. Roda do mouse num ponto muda o Q.",
      groups: [{ title: "Filtros", power: "eq_on", params: ["eq_hpf", "eq_lpf"] },
               { title: "Grave", params: ["eq_low_freq", "eq_low_gain"] },
               { title: "Médio 1", params: ["eq_m1_freq", "eq_m1_gain", "eq_m1_q"] },
               { title: "Médio 2", params: ["eq_m2_freq", "eq_m2_gain", "eq_m2_q"] },
               { title: "Agudo", params: ["eq_high_freq", "eq_high_gain"] }] },
    { key: "sibila", name: "SIBILA", sub: "De-esser", power: ["ds_on"], gr: "deess", viz: "deess",
      desc: "Segura os S e os chiados. Ative “Ouvir S” para escutar só o que está sendo cortado.",
      groups: [{ title: "De-esser", power: "ds_on", params: ["ds_freq", "ds_thresh", "ds_range", "ds_mode", "ds_listen"] }] },
    { key: "punch", name: "PUNCH", sub: "Compressor FET/VCA", power: ["comp_on"], gr: "comp", viz: "comp",
      desc: "Trap costuma pedir 6–10 dB de redução, ataque rápido e caráter FET.",
      groups: [{ title: "Compressor", power: "comp_on", params: ["comp_thresh", "comp_ratio", "comp_attack", "comp_release", "comp_knee", "comp_makeup", "comp_mix", "comp_char"] }] },
    { key: "opto", name: "OPTO", sub: "Compressor óptico", power: ["opto_on"], gr: "opto", viz: "opto",
      desc: "Nivelamento suave e musical, estilo LA-2A, depois do PUNCH.",
      groups: [{ title: "Opto", power: "opto_on", params: ["opto_peak", "opto_gain", "opto_mix"] }] },
    { key: "prisma", name: "PRISMA", sub: "Saturação por bandas", power: ["sat_on"], viz: "prisma",
      desc: "Cada banda gera harmônicos só naquela região. Arraste os pontos: altura = quantidade.",
      groups: [{ title: "Caráter", power: "sat_on", params: ["sat_type", "sat_drive", "sat_mix", "sat_out"] },
               { title: "Bandas", params: ["sat_b1_amt", "sat_b2_amt", "sat_b3_amt", "sat_b4_amt", "sat_b5_amt"] },
               { title: "Frequências", params: ["sat_b1_freq", "sat_b2_freq", "sat_b3_freq", "sat_b4_freq", "sat_b5_freq"] }] },
    { key: "ar", name: "AR", sub: "Excitador dinâmico", power: ["air_on"], viz: "air",
      desc: "Mid Air dá presença, High Air dá brilho. É dinâmico: não fica ardido.",
      groups: [{ title: "Ar", power: "air_on", params: ["air_mid", "air_high", "air_trim"], big: true }] },
    { key: "dobra", name: "DOBRA", sub: "Doubler estéreo", power: ["dbl_on"], viz: "dbl",
      desc: "Cria dobras com detune e atraso reais, abertas nas laterais.",
      groups: [{ title: "Doubler", power: "dbl_on", params: ["dbl_voices", "dbl_detune", "dbl_delay", "dbl_width", "dbl_mod", "dbl_mix"] }] },
    { key: "eco", name: "ECO", sub: "Delay", power: ["dly_on"], viz: "delay",
      desc: "Sincronizado com o BPM. O ducking abaixa o delay enquanto tu canta.",
      groups: [{ title: "Tempo", power: "dly_on", params: ["dly_sync", "dly_note", "dly_time", "dly_mode"] },
               { title: "Som", params: ["dly_feedback", "dly_mix", "dly_duck", "dly_lowcut", "dly_highcut", "dly_mod", "dly_drive", "dly_width"] }] },
    { key: "espaco", name: "ESPAÇO", sub: "Reverb", power: ["rev_on"], viz: "reverb",
      desc: "Pré-delay de 20–40 ms mantém a voz na frente mesmo com reverb grande.",
      groups: [{ title: "Sala", power: "rev_on", params: ["rev_type", "rev_size", "rev_decay", "rev_predelay"] },
               { title: "Cor", params: ["rev_damp", "rev_lowcut", "rev_highcut", "rev_mod", "rev_width", "rev_duck", "rev_mix"] }] },
    { key: "master", name: "MASTER", sub: "Glue + limiter", power: ["lim_on"], gr: "lim", viz: "master",
      desc: "No master do projeto vira masterização completa; no vocal, segura os picos.",
      groups: [{ title: "Glue", power: "glue_on", params: ["glue_thresh", "glue_ratio", "glue_release"] },
               { title: "Tom e imagem", params: ["tilt", "img_width", "img_monobass"] },
               { title: "Limiter", power: "lim_on", params: ["lim_gain", "lim_ceiling", "lim_release", "lufs_target"] },
               { title: "Saída", params: ["in_gain", "out_gain", "mix"] }] }
  ];
  const moduleOfParam = {};
  MODULES.forEach((m) => { m.power.forEach((p) => (moduleOfParam[p] = m.key)); m.groups.forEach((g) => { if (g.power) moduleOfParam[g.power] = m.key; g.params.forEach((p) => (moduleOfParam[p] = m.key)); }); });

  // ---------------------------------------------------------------------------
  //  Valores: normalização, formatação
  // ---------------------------------------------------------------------------
  function toNorm(p, v) { if (p.skew === 1 && p.min > 0) return Math.log(v / p.min) / Math.log(p.max / p.min); return (v - p.min) / (p.max - p.min); }
  function fromNorm(p, n) {
    n = Math.min(1, Math.max(0, n));
    let v = p.skew === 1 && p.min > 0 ? p.min * Math.pow(p.max / p.min, n) : p.min + n * (p.max - p.min);
    if (p.step >= 1) v = Math.round(v);
    return v;
  }
  function fmt(p, v) {
    if (p.unit === "bool") return v > 0.5 ? "Ligado" : "Desligado";
    if (p.unit === "choice") return p.choices[Math.round(v)] || "";
    const u = p.unit;
    if (u === "Hz") return v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 1 : 2) + " kHz" : Math.round(v) + " Hz";
    if (u === "dB" || u === "LUFS") return (v > 0 && u === "dB" ? "+" : "") + v.toFixed(1) + " " + u;
    if (u === "ms") return v < 10 ? v.toFixed(2) + " ms" : v < 100 ? v.toFixed(1) + " ms" : Math.round(v) + " ms";
    if (u === "s") return v.toFixed(2) + " s";
    if (u === "%") return Math.round(v) + "%";
    if (u === ":1") return v.toFixed(1) + ":1";
    if (u === "st") return (v > 0 ? "+" : "") + Math.round(v) + " st";
    if (u === "cents") return v.toFixed(1) + " ct";
    return v.toFixed(2);
  }
  const grTxt = (v) => (v || 0) < 0.05 ? "0.0 dB" : `−${v.toFixed(1)} dB`;
  const midiName = (m) => Agent.KEYS[((Math.round(m) % 12) + 12) % 12] + (Math.floor(Math.round(m) / 12) - 1);

  // ---------------------------------------------------------------------------
  //  Escrita de parâmetros
  // ---------------------------------------------------------------------------
  function refresh(id) { (controls[id] || []).forEach((fn) => fn(S.values[id])); updateTiles(); }
  function setValue(id, v, opts = {}) {
    const p = S.byId[id]; if (!p) return;
    v = Math.min(p.max, Math.max(p.min, v)); if (p.step >= 1) v = Math.round(v);
    S.values[id] = v;
    if (!opts.silent) Bridge.call("setParam", id, v, opts.gesture !== false);
    if (opts.ai) S.aiTouched[id] = performance.now(); else delete S.aiTouched[id];
    refresh(id);
  }
  function snapshot() { return Object.assign({}, S.values); }
  function pushHistory() { S.history.push(snapshot()); if (S.history.length > 120) S.history.shift(); S.future = []; }
  function applyValues(obj, { ai = false, history = true } = {}) {
    if (history) pushHistory();
    const changed = {};
    for (const [id, v0] of Object.entries(obj)) {
      const p = S.byId[id]; if (!p) continue;
      let v = Math.min(p.max, Math.max(p.min, Number(v0))); if (p.step >= 1) v = Math.round(v);
      if (Math.abs(v - S.values[id]) > 1e-6) changed[id] = { from: S.values[id], to: v };
      S.values[id] = v; if (ai) S.aiTouched[id] = performance.now();
    }
    const send = {}; Object.keys(changed).forEach((k) => (send[k] = changed[k].to));
    if (Object.keys(send).length) Bridge.call("setParams", send);
    Object.keys(changed).forEach(refresh); updateTiles();
    return changed;
  }
  function loadPreset(preset, { ai = false } = {}) {
    const vals = Object.assign(defaults(), preset.values || preset.data || {});
    // tom e escala do autotune são da música, não do preset: preserva
    if (!(preset.values && "tune_key" in (preset.data || {}))) { vals.tune_key = S.values.tune_key; }
    if (preset.values && !("tune_scale" in preset.values)) vals.tune_scale = S.values.tune_scale;
    const changed = applyValues(vals, { ai });
    S.presetName = preset.name; S.presetCat = preset.cat || "Meus presets";
    $("#presetName").textContent = S.presetName; $("#presetCat").textContent = S.presetCat;
    Bridge.call("setPresetName", S.presetName);
    return changed;
  }
  function undo() { if (!S.history.length) return toast("Nada pra desfazer"); S.future.push(snapshot()); applyValues(S.history.pop(), { history: false }); toast("Desfeito"); }
  function redo() { if (!S.future.length) return toast("Nada pra refazer"); S.history.push(snapshot()); applyValues(S.future.pop(), { history: false }); toast("Refeito"); }

  // ---------------------------------------------------------------------------
  //  Controles
  // ---------------------------------------------------------------------------
  function knob(id, big = false) {
    const p = S.byId[id];
    const el = h("div", { class: "knob" + (big ? " big" : ""), tabindex: 0, role: "slider", "aria-label": p.label, title: "Arraste • Shift = fino • duplo clique = padrão" });
    const s = svg("svg", { viewBox: "0 0 52 52" });
    const A0 = -135, A1 = 135, R = 21, cx = 26, cy = 26;
    const arcPath = (a0, a1) => { const r = (a) => ((a - 90) * Math.PI) / 180; const x0 = cx + R * Math.cos(r(a0)), y0 = cy + R * Math.sin(r(a0)), x1 = cx + R * Math.cos(r(a1)), y1 = cy + R * Math.sin(r(a1)); return `M${x0} ${y0} A${R} ${R} 0 ${a1 - a0 > 180 ? 1 : 0} 1 ${x1} ${y1}`; };
    s.append(svg("path", { class: "track", d: arcPath(A0, A1), fill: "none", "stroke-width": 4, "stroke-linecap": "round" }));
    const arc = svg("path", { class: "arc", fill: "none", "stroke-width": 4, "stroke-linecap": "round" });
    s.append(arc, svg("circle", { class: "cap", cx, cy, r: 15, "stroke-width": 1 }));
    const ptr = svg("line", { class: "ptr", x1: cx, y1: cy - 6, x2: cx, y2: cy - 13, "stroke-width": 2.4, "stroke-linecap": "round" });
    s.append(ptr);
    const val = h("div", { class: "val num" }); const lbl = h("div", { class: "lbl" }, p.label);
    el.append(s, val, lbl);
    const bipolar = p.min < 0 && p.max > 0;
    const update = (v) => {
      const n = toNorm(p, v), a = A0 + n * (A1 - A0);
      const zero = bipolar ? A0 + toNorm(p, 0) * (A1 - A0) : A0;
      arc.setAttribute("d", Math.abs(a - zero) < 0.5 ? "" : arcPath(Math.min(zero, a), Math.max(zero, a)));
      ptr.setAttribute("transform", `rotate(${a} ${cx} ${cy})`);
      if (!val.querySelector("input")) val.textContent = fmt(p, v);
      el.classList.toggle("ai", !!S.aiTouched[id]);
      el.setAttribute("aria-valuetext", fmt(p, v));
    };
    (controls[id] = controls[id] || []).push(update); update(S.values[id]);

    let startY = 0, startN = 0, dragging = false;
    el.addEventListener("pointerdown", (e) => {
      if (e.target === val || val.contains(e.target)) return;
      dragging = true; startY = e.clientY; startN = toNorm(p, S.values[id]); el.setPointerCapture(e.pointerId);
      pushHistory(); Bridge.call("beginGesture", id);
    });
    el.addEventListener("pointermove", (e) => {
      if (!dragging) return;
      const sens = e.shiftKey ? 0.0012 : 0.005;
      setValue(id, fromNorm(p, startN + (startY - e.clientY) * sens), { gesture: false });
    });
    const end = () => { if (dragging) { dragging = false; Bridge.call("endGesture", id); } };
    el.addEventListener("pointerup", end); el.addEventListener("pointercancel", end);
    el.addEventListener("dblclick", (e) => { if (e.target === val) return; pushHistory(); setValue(id, p.def); });
    el.addEventListener("wheel", (e) => { e.preventDefault(); setValue(id, fromNorm(p, toNorm(p, S.values[id]) - Math.sign(e.deltaY) * (e.shiftKey ? 0.004 : 0.02))); }, { passive: false });
    el.addEventListener("keydown", (e) => {
      const d = e.key === "ArrowUp" || e.key === "ArrowRight" ? 1 : e.key === "ArrowDown" || e.key === "ArrowLeft" ? -1 : 0;
      if (d) { e.preventDefault(); setValue(id, fromNorm(p, toNorm(p, S.values[id]) + d * (e.shiftKey ? 0.005 : 0.03))); }
    });
    val.addEventListener("click", () => {
      const input = h("input", { class: "edit", value: (Math.round(S.values[id] * 100) / 100).toString() });
      val.textContent = ""; val.append(input); input.focus(); input.select();
      const done = (ok) => { const n = parseFloat(input.value.replace(",", ".").replace(/k$/i, "e3")); input.remove(); if (ok && isFinite(n)) { pushHistory(); setValue(id, n); } else update(S.values[id]); };
      input.addEventListener("keydown", (e) => { if (e.key === "Enter") done(true); if (e.key === "Escape") done(false); e.stopPropagation(); });
      input.addEventListener("blur", () => done(true));
    });
    return el;
  }

  function toggle(id, label) {
    const p = S.byId[id];
    const el = h("button", { class: "toggle", type: "button", role: "switch", "aria-label": label || p.label }, h("span", { class: "sw" }), label === "" ? null : (label || p.label));
    const update = (v) => { el.classList.toggle("on", v > 0.5); el.setAttribute("aria-checked", v > 0.5); };
    (controls[id] = controls[id] || []).push(update); update(S.values[id]);
    el.addEventListener("click", () => { pushHistory(); setValue(id, S.values[id] > 0.5 ? 0 : 1); });
    return el;
  }

  function choice(id) {
    const p = S.byId[id];
    if (p.choices.length > 8) {
      const sel = h("select", { "aria-label": p.label }, p.choices.map((c, i) => h("option", { value: i }, c)));
      const update = (v) => (sel.value = Math.round(v));
      (controls[id] = controls[id] || []).push(update); update(S.values[id]);
      sel.addEventListener("change", () => { pushHistory(); setValue(id, +sel.value); });
      return h("div", { class: "select" }, h("span", { class: "lbl" }, p.label), sel);
    }
    const wrap = h("div", { class: "seg" }, h("span", { class: "lbl" }, p.label));
    const opts = h("div", { class: "opts" });
    const btns = p.choices.map((c, i) => { const b = h("button", { type: "button", onclick: () => { pushHistory(); setValue(id, i); } }, c); opts.append(b); return b; });
    wrap.append(opts);
    const update = (v) => { btns.forEach((b, i) => b.classList.toggle("on", i === Math.round(v))); wrap.classList.toggle("ai", !!S.aiTouched[id]); };
    (controls[id] = controls[id] || []).push(update); update(S.values[id]);
    return wrap;
  }

  function control(id, big) {
    const p = S.byId[id];
    if (p.unit === "bool") return h("div", { style: "align-self:center;padding:0 6px" }, toggle(id));
    if (p.unit === "choice") return choice(id);
    return knob(id, big);
  }

  // ---------------------------------------------------------------------------
  //  Rack
  // ---------------------------------------------------------------------------
  const tiles = {};
  function renderRack() {
    const rack = $("#rack"); rack.innerHTML = "";
    rack.append(h("h2", {}, "Cadeia do sinal"));
    MODULES.forEach((m, i) => {
      const pw = h("button", { class: "pw", type: "button", title: "Liga/desliga", "aria-label": "Liga/desliga " + m.name });
      pw.addEventListener("click", (e) => { e.stopPropagation(); const on = m.power.some((id) => S.values[id] > 0.5); const o = {}; m.power.forEach((id) => (o[id] = on ? 0 : 1)); applyValues(o); });
      const gr = h("div", { class: "gr" }, h("i"));
      const t = h("div", { class: "tile", tabindex: 0, role: "button", onclick: () => select(m.key), onkeydown: (e) => { if (e.key === "Enter") select(m.key); } },
        i < MODULES.length - 1 ? h("span", { class: "chain-line" }) : null, pw, h("span", { class: "nm" }, m.name, h("span", { class: "sb" }, m.sub)), m.gr ? gr : h("span"));
      tiles[m.key] = { el: t, pw, gr: gr.firstChild };
      rack.append(t);
    });
    updateTiles();
  }
  function updateTiles() {
    MODULES.forEach((m) => {
      const t = tiles[m.key]; if (!t) return;
      const on = m.power.some((id) => S.values[id] > 0.5);
      t.pw.classList.toggle("on", on); t.el.classList.toggle("off", !on); t.el.classList.toggle("sel", S.sel === m.key);
    });
    const lm = S.values.latency_mode; $("#modePill").innerHTML = `<b>${lm > 0.5 ? "Gravação" : "Mixagem HQ"}</b>`;
  }
  function select(key) { S.sel = key; renderModule(); updateTiles(); }

  // ---------------------------------------------------------------------------
  //  Painel do módulo
  // ---------------------------------------------------------------------------
  let viz = null;
  function renderModule() {
    Object.keys(controls).forEach((k) => (controls[k] = controls[k].filter((fn) => fn.persistent)));
    const m = MODULES.find((x) => x.key === S.sel);
    const root = $("#module"); root.innerHTML = "";
    root.append(h("div", { class: "mod-head" }, h("h1", {}, m.name), h("p", {}, m.desc), h("div", { class: "grow" })));
    const wrap = h("div", { class: "viz-wrap" }); const cv = h("canvas"); wrap.append(cv);
    const legend = h("div", { class: "viz-legend" }); const readout = h("div", { class: "big-readout" });
    wrap.append(legend, readout); root.append(wrap);
    viz = { m, cv, ctx: cv.getContext("2d"), legend, readout, drag: null, nodes: [] };
    setupVizInteraction(viz);
    const groups = h("div", { class: "groups" });
    m.groups.forEach((g) => {
      const head = h("div", { class: "group-head" }, h("h3", {}, g.title), h("div", { class: "grow" }), g.power ? toggle(g.power, "") : null);
      const ctl = h("div", { class: "controls" });
      if (g.extra === "keys") ctl.append(keyPicker());
      g.params.forEach((id) => { if (g.extra === "keys" && id === "tune_key") return; ctl.append(control(id, g.big)); });
      if (g.extra === "learn") ctl.append(h("div", { style: "display:flex;flex-direction:column;gap:6px;justify-content:center;padding-left:6px" },
        h("button", { class: "btn", type: "button", onclick: learnNoise }, "Aprender ruído"), h("small", { id: "nrStatus", style: "color:var(--muted);font-size:11px;max-width:150px" }, "Fique 2 s em silêncio ao clicar")));
      groups.append(h("div", { class: "group" }, head, ctl));
    });
    root.append(groups);
    legendFor(viz);
  }
  function keyPicker() {
    const keys = h("div", { class: "keys", role: "group", "aria-label": "Tom" });
    const black = [1, 3, 6, 8, 10];
    const btns = Agent.KEYS.map((k, i) => { const b = h("button", { type: "button", class: black.includes(i) ? "b" : "w", onclick: () => { pushHistory(); setValue("tune_key", i); setValue("tune_on", 1); } }, k); keys.append(b); return b; });
    const scales = [[1,1,1,1,1,1,1,1,1,1,1,1],[1,0,1,0,1,1,0,1,0,1,0,1],[1,0,1,1,0,1,0,1,1,0,1,0],[1,0,1,1,0,1,0,1,1,0,0,1],[1,0,0,1,0,1,0,1,0,0,1,0],[1,0,1,0,1,0,0,1,0,1,0,0],[1,0,1,1,0,1,0,1,0,1,1,0],[1,1,0,1,0,1,0,1,1,0,1,0]];
    const update = () => { const k = S.values.tune_key, sc = scales[Math.round(S.values.tune_scale)] || scales[0]; btns.forEach((b, i) => { b.classList.toggle("on", i === Math.round(k)); b.classList.toggle("inscale", !!sc[(i - k + 12) % 12]); }); };
    (controls.tune_key = controls.tune_key || []).push(update); (controls.tune_scale = controls.tune_scale || []).push(update); update();
    return h("div", { class: "seg" }, h("span", { class: "lbl" }, "Tom (o beat pode definir: “Ouvir o beat”)"), keys);
  }
  function legendFor(v) {
    const L = (color, text) => h("span", {}, h("i", { style: `background:${color}` }), text);
    const map = { pitch: [L("#A98DB3", "Voz"), L("#FF3E86", "Corrigida"), L("#3D2049", "Notas da escala")], eq: [L("#FF3E86", "Curva"), L("#6E5578", "Espectro")],
      prisma: [L("#FFC56A", "Harmônicos por banda"), L("#6E5578", "Espectro")], deess: [L("#FF7AAE", "Faixa do sidechain"), L("#6E5578", "Espectro")],
      comp: [L("#FF3E86", "Curva de compressão"), L("#FFC56A", "Nível agora")], delay: [L("#FF3E86", "Esquerda"), L("#FF7AAE", "Direita")],
      gate: [L("#A98DB3", "Nível"), L("#FF3E86", "Threshold")], master: [L("#FF3E86", "LUFS curto"), L("#FFC56A", "Alvo")], air: [L("#FFC56A", "Ganho dinâmico estimado"), L("#6E5578", "Espectro")],
      reverb: [L("#FF3E86", "Envelope do reverb")], dbl: [L("#FF3E86", "Vozes no campo estéreo")], opto: [L("#FFC56A", "Redução de ganho")] };
    v.legend.innerHTML = ""; (map[v.m.viz] || []).forEach((x) => v.legend.append(x));
    if (S.analysis.beat && S.bsp.length && ["eq", "prisma", "deess", "air"].includes(v.m.viz)) v.legend.append(L("#4C6B9A", "Beat (sidechain)"));
  }

  // ---------------------------------------------------------------------------
  //  Visualizações (canvas)
  // ---------------------------------------------------------------------------
  const FMIN = 20, FMAX = 20000;
  const xOfF = (f, w) => (Math.log(f / FMIN) / Math.log(FMAX / FMIN)) * w;
  const fOfX = (x, w) => FMIN * Math.pow(FMAX / FMIN, x / w);
  function biquadDb(type, f0, q, g, f, sr = 48000) {
    const A = Math.pow(10, g / 40), w0 = 2 * Math.PI * f0 / sr, cw = Math.cos(w0), sw = Math.sin(w0), al = sw / (2 * q);
    let b0, b1, b2, a0, a1, a2;
    if (type === "peak") { b0 = 1 + al * A; b1 = -2 * cw; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cw; a2 = 1 - al / A; }
    else if (type === "ls" || type === "hs") { const sq = 2 * Math.sqrt(A) * al, s = type === "ls" ? 1 : -1;
      b0 = A * ((A + 1) - s * (A - 1) * cw + sq); b1 = 2 * s * A * ((A - 1) - s * (A + 1) * cw); b2 = A * ((A + 1) - s * (A - 1) * cw - sq);
      a0 = (A + 1) + s * (A - 1) * cw + sq; a1 = -2 * s * ((A - 1) + s * (A + 1) * cw); a2 = (A + 1) + s * (A - 1) * cw - sq; }
    else if (type === "hp") { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; }
    else { b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; }
    const w = 2 * Math.PI * f / sr, c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w);
    const nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2), dr = a0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
    return 10 * Math.log10((nr * nr + ni * ni) / (dr * dr + di * di));
  }
  function eqResponse(f) {
    const V = S.values; if (V.eq_on < 0.5) return 0;
    let db = 0;
    if (V.eq_hpf > 21) db += biquadDb("hp", V.eq_hpf, 0.5412, 0, f) + biquadDb("hp", V.eq_hpf, 1.3066, 0, f);
    db += biquadDb("ls", V.eq_low_freq, 0.707, V.eq_low_gain, f) + biquadDb("peak", V.eq_m1_freq, V.eq_m1_q, V.eq_m1_gain, f);
    db += biquadDb("peak", V.eq_m2_freq, V.eq_m2_q, V.eq_m2_gain, f) + biquadDb("hs", V.eq_high_freq, 0.707, V.eq_high_gain, f);
    if (V.eq_lpf < 21500) db += biquadDb("lp", V.eq_lpf, 0.707, 0, f);
    return db;
  }
  function drawSpectrum(ctx, w, h, data, color, fillTop) {
    if (!data || !data.length) return;
    ctx.beginPath();
    data.forEach((db, i) => { const x = (i / (data.length - 1)) * w, y = h - ((Math.max(-90, db) + 90) / 90) * h * 0.92; i ? ctx.lineTo(x, y) : ctx.moveTo(x, y); });
    if (fillTop) { ctx.lineTo(w, h); ctx.lineTo(0, h); ctx.closePath(); ctx.fillStyle = fillTop; ctx.fill(); }
    else { ctx.strokeStyle = color; ctx.lineWidth = 1.2; ctx.stroke(); }
  }
  function gridFreq(ctx, w, h) {
    ctx.strokeStyle = "#2a1531"; ctx.lineWidth = 1; ctx.fillStyle = "#6E5578"; ctx.font = "10px " + getComputedStyle(document.body).fontFamily;
    [50, 100, 200, 500, 1000, 2000, 5000, 10000].forEach((f) => { const x = Math.round(xOfF(f, w)) + 0.5; ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke(); ctx.fillText(f >= 1000 ? f / 1000 + "k" : f, x + 3, h - 6); });
  }
  const dbToY = (db, h, range = 18) => h / 2 - (db / range) * (h * 0.42);
  const yToDb = (y, h, range = 18) => ((h / 2 - y) / (h * 0.42)) * range;

  function drawViz() {
    if (!viz) return;
    const { cv, ctx, m } = viz; const dpr = window.devicePixelRatio || 1;
    const w = cv.clientWidth, hh = cv.clientHeight;
    if (cv.width !== Math.round(w * dpr) || cv.height !== Math.round(hh * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(hh * dpr); }
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0); ctx.clearRect(0, 0, w, hh);
    const V = S.values, M = S.meters; viz.nodes = [];
    const ro = (v, l) => { viz.readout.innerHTML = `<div class="v num">${v}</div><div class="l">${l}</div>`; };
    switch (m.viz) {
      case "pitch": {
        const H = S.hist.pitch; const lo = 40, hi = 76;
        const yOf = (mid) => hh - ((mid - lo) / (hi - lo)) * hh;
        const scales = [[1,1,1,1,1,1,1,1,1,1,1,1],[1,0,1,0,1,1,0,1,0,1,0,1],[1,0,1,1,0,1,0,1,1,0,1,0],[1,0,1,1,0,1,0,1,1,0,0,1],[1,0,0,1,0,1,0,1,0,0,1,0],[1,0,1,0,1,0,0,1,0,1,0,0],[1,0,1,1,0,1,0,1,0,1,1,0],[1,1,0,1,0,1,0,1,1,0,1,0]];
        const sc = scales[Math.round(V.tune_scale)] || scales[0];
        ctx.font = "10px sans-serif";
        for (let n = lo; n <= hi; n++) { const inS = sc[(n - V.tune_key + 120) % 12]; const y = yOf(n); ctx.fillStyle = inS ? "#24112c" : "#140810"; ctx.fillRect(0, y - (hh / (hi - lo)) / 2, w, hh / (hi - lo) - 1); if (inS) { ctx.fillStyle = "#6E5578"; ctx.fillText(midiName(n), 6, y + 3); } }
        const step = w / 300;
        const line = (key, color, width) => { ctx.beginPath(); let pen = false; H.forEach((pt, i) => { const v = pt[key]; if (!v) { pen = false; return; } const x = w - (H.length - i) * step, y = yOf(v); pen ? ctx.lineTo(x, y) : ctx.moveTo(x, y); pen = true; }); ctx.strokeStyle = color; ctx.lineWidth = width; ctx.stroke(); };
        line("midi", "#A98DB3", 1.5); if (V.tune_on > 0.5) line("out", "#FF3E86", 2.5);
        if (M.midi > 0) { const cents = Math.round((M.midi - Math.round(M.midi)) * 100); ro(midiName(M.target || M.midi), `${M.hz ? M.hz.toFixed(1) : "–"} Hz • ${cents > 0 ? "+" : ""}${cents} cents • correção ${(M.corr || 0).toFixed(2)} st`); } else ro("–", "canta que eu escuto");
        break;
      }
      case "eq": {
        gridFreq(ctx, w, hh);
        if (S.bsp.length) drawSpectrum(ctx, w, hh, S.bsp, null, "#4C6B9A33");
        drawSpectrum(ctx, w, hh, S.sp, null, "#6E557844");
        ctx.strokeStyle = "#3D2049"; ctx.beginPath(); ctx.moveTo(0, hh / 2); ctx.lineTo(w, hh / 2); ctx.stroke();
        ctx.beginPath(); for (let x = 0; x <= w; x += 2) { const y = dbToY(eqResponse(fOfX(x, w)), hh); x ? ctx.lineTo(x, y) : ctx.moveTo(x, y); }
        ctx.strokeStyle = V.eq_on > 0.5 ? "#FF3E86" : "#6E5578"; ctx.lineWidth = 2.2; ctx.stroke();
        const N = (id, f, gId, label) => viz.nodes.push({ id, gId, x: xOfF(V[f], w), y: gId ? dbToY(V[gId], hh) : hh / 2, label, fId: f });
        N("hpf", "eq_hpf", null, "HP"); N("low", "eq_low_freq", "eq_low_gain", "G"); N("m1", "eq_m1_freq", "eq_m1_gain", "1"); N("m2", "eq_m2_freq", "eq_m2_gain", "2"); N("high", "eq_high_freq", "eq_high_gain", "A"); N("lpf", "eq_lpf", null, "LP");
        drawNodes(ctx); ro(viz.hover ? viz.hover : "EQ", "arraste os pontos");
        break;
      }
      case "prisma": {
        gridFreq(ctx, w, hh); if (S.bsp.length) drawSpectrum(ctx, w, hh, S.bsp, null, "#4C6B9A33");
        drawSpectrum(ctx, w, hh, S.sp, null, "#6E557844");
        const base = hh - 18;
        for (let b = 1; b <= 5; b++) {
          const f = V[`sat_b${b}_freq`], a = V[`sat_b${b}_amt`]; const x0 = xOfF(f, w), y = base - (a / 12) * (hh - 50);
          const grad = ctx.createLinearGradient(0, y, 0, base); grad.addColorStop(0, V.sat_on > 0.5 ? "#FFC56A88" : "#6E557855"); grad.addColorStop(1, "#FFC56A00");
          ctx.beginPath(); ctx.moveTo(x0 - 90, base); ctx.bezierCurveTo(x0 - 30, base, x0 - 30, y, x0, y); ctx.bezierCurveTo(x0 + 30, y, x0 + 30, base, x0 + 90, base); ctx.fillStyle = grad; ctx.fill();
          // harmônicos (2x, 3x)
          for (let k = 2; k <= 4; k++) { const xh = xOfF(f * k, w); if (xh < w) { ctx.fillStyle = "#FFC56A" + (V.sat_on > 0.5 ? "55" : "22"); const hgt = (a / 12) * (hh - 50) * (V.sat_drive / 100) / k; ctx.fillRect(xh - 1, base - hgt, 2, hgt); } }
          viz.nodes.push({ id: "b" + b, fId: `sat_b${b}_freq`, aId: `sat_b${b}_amt`, x: x0, y, label: String(b) });
        }
        drawNodes(ctx, "#FFC56A"); ro(S.byId.sat_type.choices[Math.round(V.sat_type)], `drive ${Math.round(V.sat_drive)}% • mix ${Math.round(V.sat_mix)}%`);
        break;
      }
      case "deess": {
        gridFreq(ctx, w, hh); drawSpectrum(ctx, w, hh, S.sp, null, "#6E557844");
        const x = xOfF(V.ds_freq, w); const g = ctx.createLinearGradient(x - 60, 0, x + 60, 0); g.addColorStop(0, "#FF7AAE00"); g.addColorStop(0.5, "#FF7AAE44"); g.addColorStop(1, "#FF7AAE00");
        ctx.fillStyle = g; ctx.fillRect(x - 60, 0, 120, hh); ctx.strokeStyle = "#FF7AAE"; ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, hh); ctx.stroke();
        viz.nodes.push({ id: "ds", fId: "ds_freq", x, y: hh * 0.25, label: "S" }); drawNodes(ctx, "#FF7AAE");
        drawHistory(ctx, w, hh, S.hist.deess, 24, "#FFC56A"); ro(grTxt(M.deess), "redução nos S agora");
        break;
      }
      case "comp": {
        const sz = Math.min(hh - 20, w * 0.45), ox = 30, oy = 10; const toX = (db) => ox + ((db + 60) / 60) * sz, toY = (db) => oy + sz - ((db + 60) / 60) * sz;
        ctx.strokeStyle = "#2a1531"; for (let d = -60; d <= 0; d += 12) { ctx.beginPath(); ctx.moveTo(toX(d), oy); ctx.lineTo(toX(d), oy + sz); ctx.moveTo(ox, toY(d)); ctx.lineTo(ox + sz, toY(d)); ctx.stroke(); }
        ctx.beginPath(); for (let d = -60; d <= 0; d += 0.5) { const over = d - V.comp_thresh, knee = V.comp_knee; let gr = 0; const sl = 1 - 1 / V.comp_ratio;
          if (knee > 0 && Math.abs(over) * 2 <= knee) gr = sl * Math.pow(over + knee / 2, 2) / (2 * knee); else if (over > 0) gr = sl * over;
          const out = d - gr + V.comp_makeup; d === -60 ? ctx.moveTo(toX(d), toY(out)) : ctx.lineTo(toX(d), toY(Math.max(-60, Math.min(0, out)))); }
        ctx.strokeStyle = "#FF3E86"; ctx.lineWidth = 2.2; ctx.stroke();
        const lvl = 20 * Math.log10(Math.max(1e-4, Math.max(M.inL || 0, M.inR || 0))); ctx.fillStyle = "#FFC56A"; ctx.beginPath(); ctx.arc(toX(Math.max(-60, lvl)), toY(Math.max(-60, lvl - (M.comp || 0) + V.comp_makeup)), 5, 0, 7); ctx.fill();
        drawHistory(ctx, w, hh, S.hist.comp, 20, "#FFC56A", ox + sz + 30);
        ro(grTxt(M.comp), `${S.byId.comp_char.choices[Math.round(V.comp_char)]} • ${V.comp_ratio.toFixed(1)}:1`);
        break;
      }
      case "opto": drawHistory(ctx, w, hh, S.hist.opto, 15, "#FFC56A"); ro(grTxt(M.opto), "redução do opto"); break;
      case "gate": {
        const H = S.hist.gate; const yOf = (db) => hh - ((db + 80) / 80) * hh;
        ctx.beginPath(); H.forEach((p, i) => { const x = (i / 299) * w; i ? ctx.lineTo(x, yOf(p)) : ctx.moveTo(x, yOf(p)); }); ctx.strokeStyle = "#A98DB3"; ctx.lineWidth = 1.5; ctx.stroke();
        ctx.strokeStyle = "#FF3E86"; ctx.setLineDash([6, 5]); ctx.beginPath(); ctx.moveTo(0, yOf(V.gate_thresh)); ctx.lineTo(w, yOf(V.gate_thresh)); ctx.stroke(); ctx.setLineDash([]);
        viz.nodes.push({ id: "gt", aId: "gate_thresh", x: w - 40, y: yOf(V.gate_thresh), label: "T", gateNode: true }); drawNodes(ctx);
        ro(M.learning ? "Ouvindo…" : (M.noiseProfile ? "Perfil salvo" : "Adaptativo"), `gate ${M.gate > 0.5 ? "fechando −" + M.gate.toFixed(0) + " dB" : "aberto"}`);
        break;
      }
      case "air": {
        gridFreq(ctx, w, hh); drawSpectrum(ctx, w, hh, S.sp, null, "#6E557844");
        ctx.beginPath(); for (let x = 0; x <= w; x += 2) { const f = fOfX(x, w); const mid = V.air_mid / 100 * 7 / (1 + Math.pow(2800 / f, 4)) * (f > 16000 ? 0.5 : 1); const hi = V.air_high / 100 * 9 / (1 + Math.pow(9500 / f, 4));
          const y = dbToY((V.air_on > 0.5 ? mid + hi : 0) + V.air_trim, hh, 12); x ? ctx.lineTo(x, y) : ctx.moveTo(x, y); }
        ctx.strokeStyle = "#FFC56A"; ctx.lineWidth = 2.2; ctx.stroke(); ro(`${Math.round(V.air_mid)} / ${Math.round(V.air_high)}`, "Mid Air / High Air");
        break;
      }
      case "dbl": {
        const cx = w / 2, cy = hh - 20, R = Math.min(w / 2 - 30, hh - 40);
        ctx.strokeStyle = "#2a1531"; for (let r = 0.33; r <= 1; r += 0.33) { ctx.beginPath(); ctx.arc(cx, cy, R * r, Math.PI, 0); ctx.stroke(); }
        ctx.fillStyle = "#6E5578"; ctx.font = "11px sans-serif"; ctx.fillText("E", cx - R - 14, cy); ctx.fillText("D", cx + R + 6, cy);
        const pans = [-1, 1, -0.55, 0.55], n = V.dbl_voices > 0.5 ? 4 : 2, t = performance.now() / 1000;
        ctx.fillStyle = "#F3E8F6"; ctx.beginPath(); ctx.arc(cx, cy - R * 0.92, 7, 0, 7); ctx.fill();
        for (let i = 0; i < n; i++) { const p = pans[i] * V.dbl_width / 100 + Math.sin(t * (0.6 + i * 0.3)) * 0.04 * V.dbl_mod / 100; const a = Math.PI / 2 - p * Math.PI / 2.2; const rr = R * (0.75 + 0.1 * Math.sin(t + i));
          ctx.fillStyle = V.dbl_on > 0.5 ? "#FF3E86" : "#6E5578"; ctx.globalAlpha = 0.35 + V.dbl_mix / 140; ctx.beginPath(); ctx.arc(cx + Math.cos(a) * rr, cy - Math.sin(a) * rr, 5 + V.dbl_mix / 10, 0, 7); ctx.fill(); ctx.globalAlpha = 1; }
        ro(`${n} vozes`, `±${V.dbl_detune.toFixed(1)} cents • ${V.dbl_delay.toFixed(0)} ms`);
        break;
      }
      case "delay": {
        const beats = [0.125, 1 / 6, 0.25, 0.375, 1 / 3, 0.5, 0.75, 2 / 3, 1, 1.5, 2, 4][Math.round(V.dly_note)];
        const bpm = M.bpm || 120; let tms = V.dly_sync > 0.5 ? beats * 60000 / bpm : V.dly_time; if (Math.round(V.dly_mode) === 3) tms = Math.min(tms, 140);
        const span = Math.max(tms * 8, 1500), mid = hh / 2; ctx.strokeStyle = "#2a1531"; ctx.beginPath(); ctx.moveTo(0, mid); ctx.lineTo(w, mid); ctx.stroke();
        ctx.fillStyle = "#F3E8F6"; ctx.fillRect(14, mid - hh * 0.4, 3, hh * 0.4);
        let amp = V.dly_mix / 100 + 0.25;
        for (let k = 1; k * tms < span && amp > 0.01; k++) {
          const x = 14 + (k * tms / span) * (w - 28); const mode = Math.round(V.dly_mode); const left = mode === 1 ? k % 2 === 1 : true; const both = mode !== 1;
          const hgt = amp * hh * 0.4; ctx.fillStyle = V.dly_on > 0.5 ? "#FF3E86" : "#6E5578"; if (left || both) ctx.fillRect(x, mid - hgt, 3, hgt);
          ctx.fillStyle = V.dly_on > 0.5 ? "#FF7AAE" : "#6E5578"; if (!left || both) ctx.fillRect(x, mid, 3, hgt * (both ? 0.92 : 1));
          amp *= V.dly_feedback / 100; if (k === 1) amp = Math.max(amp, 0.0);
        }
        ro(`${Math.round(tms)} ms`, `${V.dly_sync > 0.5 ? S.byId.dly_note.choices[Math.round(V.dly_note)] + " a " + Math.round(bpm) + " BPM" : "tempo livre"} • ${S.byId.dly_mode.choices[Math.round(V.dly_mode)]}`);
        break;
      }
      case "reverb": {
        const total = V.rev_predelay / 1000 + V.rev_decay * 1.3, px = (s) => 14 + (s / total) * (w - 28), base = hh - 18;
        ctx.fillStyle = "#F3E8F6"; ctx.fillRect(px(0), base - hh * 0.75, 3, hh * 0.75);
        const er = V.rev_predelay / 1000; for (let i = 0; i < 14; i++) { const t = er + i * 0.006 * (0.5 + V.rev_size / 100); ctx.fillStyle = "#FF7AAE88"; const hgt = hh * 0.5 * (1 - i / 16) * (0.6 + 0.4 * Math.sin(i * 7.1)); ctx.fillRect(px(t), base - hgt, 2, hgt); }
        ctx.beginPath(); ctx.moveTo(px(er), base); for (let s = er; s <= total; s += total / 200) { const a = Math.pow(10, -3 * (s - er) / V.rev_decay); ctx.lineTo(px(s), base - a * hh * 0.55 * (0.3 + V.rev_mix / 70)); }
        ctx.lineTo(px(total), base); ctx.closePath(); const g = ctx.createLinearGradient(0, 0, w, 0); g.addColorStop(0, "#FF3E8699"); g.addColorStop(1, "#FF3E8611"); ctx.fillStyle = V.rev_on > 0.5 ? g : "#6E557844"; ctx.fill();
        ro(`${V.rev_decay.toFixed(1)} s`, `${S.byId.rev_type.choices[Math.round(V.rev_type)]} • pré-delay ${Math.round(V.rev_predelay)} ms`);
        break;
      }
      case "master": {
        const H = S.hist.lufs; const yOf = (l) => hh - ((l + 36) / 36) * hh;
        ctx.strokeStyle = "#2a1531"; for (let l = -30; l <= 0; l += 6) { ctx.beginPath(); ctx.moveTo(0, yOf(l)); ctx.lineTo(w, yOf(l)); ctx.stroke(); ctx.fillStyle = "#6E5578"; ctx.fillText(l + " LUFS", 6, yOf(l) - 3); }
        ctx.setLineDash([6, 5]); ctx.strokeStyle = "#FFC56A"; ctx.beginPath(); ctx.moveTo(0, yOf(V.lufs_target)); ctx.lineTo(w, yOf(V.lufs_target)); ctx.stroke(); ctx.setLineDash([]);
        ctx.beginPath(); H.forEach((l, i) => { const x = (i / 299) * w; i ? ctx.lineTo(x, yOf(Math.max(-36, l))) : ctx.moveTo(x, yOf(Math.max(-36, l))); }); ctx.strokeStyle = "#FF3E86"; ctx.lineWidth = 2; ctx.stroke();
        const diff = (M.lufsS || -70) - V.lufs_target;
        ro(`${(M.lufsS || -70).toFixed(1)}`, Math.abs(diff) < 1 ? "no alvo de loudness" : diff < 0 ? `faltam ${(-diff).toFixed(1)} dB pro alvo — suba o Ganho do limiter` : `${diff.toFixed(1)} dB acima do alvo`);
        break;
      }
    }
  }
  function drawHistory(ctx, w, hh, H, range, color, x0 = 0) {
    ctx.beginPath(); H.forEach((v, i) => { const x = x0 + (i / 299) * (w - x0), y = 10 + (v / range) * (hh - 30); i ? ctx.lineTo(x, y) : ctx.moveTo(x, y); });
    ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.stroke();
  }
  function drawNodes(ctx, color = "#FF3E86") {
    ctx.font = "600 10px sans-serif"; ctx.textAlign = "center";
    viz.nodes.forEach((n) => { const hot = viz.drag && viz.drag.id === n.id; ctx.fillStyle = hot ? color : "#22102A"; ctx.strokeStyle = color; ctx.lineWidth = 2;
      ctx.beginPath(); ctx.arc(n.x, n.y, hot ? 10 : 8.5, 0, 7); ctx.fill(); ctx.stroke(); ctx.fillStyle = hot ? "#fff" : color; ctx.fillText(n.label, n.x, n.y + 3.5); });
    ctx.textAlign = "start";
  }
  function setupVizInteraction(v) {
    const pos = (e) => { const r = v.cv.getBoundingClientRect(); return { x: e.clientX - r.left, y: e.clientY - r.top, w: r.width, h: r.height }; };
    v.cv.addEventListener("pointerdown", (e) => {
      const p = pos(e); const n = v.nodes.find((n) => Math.hypot(n.x - p.x, n.y - p.y) < 14); if (!n) return;
      v.drag = n; v.cv.setPointerCapture(e.pointerId); pushHistory();
      [n.fId, n.gId, n.aId].filter(Boolean).forEach((id) => Bridge.call("beginGesture", id));
    });
    v.cv.addEventListener("pointermove", (e) => {
      const p = pos(e); if (!v.drag) { v.cv.style.cursor = v.nodes.some((n) => Math.hypot(n.x - p.x, n.y - p.y) < 14) ? "grab" : "default"; return; }
      const n = v.drag;
      if (n.fId) setValue(n.fId, fOfX(Math.max(0, Math.min(p.w, p.x)), p.w), { gesture: false });
      if (n.gId) setValue(n.gId, yToDb(p.y, p.h), { gesture: false });
      if (n.aId && n.gateNode) setValue(n.aId, -80 + (1 - p.y / p.h) * 80, { gesture: false });
      else if (n.aId) setValue(n.aId, ((p.h - 18 - p.y) / (p.h - 50)) * 12, { gesture: false });
      v.hover = [n.fId && fmt(S.byId[n.fId], S.values[n.fId]), n.gId && fmt(S.byId[n.gId], S.values[n.gId]), n.aId && fmt(S.byId[n.aId], S.values[n.aId])].filter(Boolean).join(" • ");
    });
    const end = () => { if (v.drag) { [v.drag.fId, v.drag.gId, v.drag.aId].filter(Boolean).forEach((id) => Bridge.call("endGesture", id)); v.drag = null; } };
    v.cv.addEventListener("pointerup", end); v.cv.addEventListener("pointercancel", end);
    v.cv.addEventListener("wheel", (e) => {
      const p = pos(e); const n = v.nodes.find((n) => Math.hypot(n.x - p.x, n.y - p.y) < 16); if (!n) return; e.preventDefault();
      const qId = n.id === "m1" ? "eq_m1_q" : n.id === "m2" ? "eq_m2_q" : null; if (qId) setValue(qId, S.values[qId] * (e.deltaY < 0 ? 1.12 : 0.89));
    }, { passive: false });
  }

  // ---------------------------------------------------------------------------
  //  Macros (atalhos de vários parâmetros ao mesmo tempo)
  // ---------------------------------------------------------------------------
  const MACROS = [
    { name: "Brilho", map: [["air_high", 0, 70], ["eq_high_gain", -4, 7], ["sat_b5_amt", 0, 5]], on: ["air_on", "eq_on"] },
    { name: "Corpo", map: [["eq_low_gain", -4, 6], ["sat_b2_amt", 0, 5], ["eq_hpf", 140, 60]], on: ["eq_on"] },
    { name: "Pressão", map: [["comp_thresh", -10, -32], ["comp_ratio", 2, 10], ["comp_makeup", 0, 9], ["opto_peak", 10, 60]], on: ["comp_on"] },
    { name: "Afinação", map: [["tune_speed", 80, 0], ["tune_amount", 30, 100], ["tune_humanize", 40, 0]], on: ["tune_on"] },
    { name: "Espaço", map: [["rev_mix", 0, 35], ["dly_mix", 0, 30], ["rev_decay", 0.8, 4]], on: ["rev_on", "dly_on"] },
    { name: "Largura", map: [["dbl_mix", 0, 55], ["img_width", 80, 160], ["dbl_width", 40, 100]], on: ["dbl_on"] }
  ];
  function macroValue(mc) { const [id, a, b] = mc.map[0]; return Math.min(1, Math.max(0, (S.values[id] - a) / (b - a))); }
  function renderMacros() {
    const el = $("#macros"); el.innerHTML = "";
    el.append(h("span", { class: "macro-title" }, "Macros"));
    MACROS.forEach((mc) => {
      const fake = { id: "macro_" + mc.name, label: mc.name, min: 0, max: 100, def: 50, step: 0, skew: 0, unit: "%", choices: [] };
      S.byId[fake.id] = fake; S.values[fake.id] = macroValue(mc) * 100;
      const k = knob(fake.id, true);
      const upd = () => { S.values[fake.id] = macroValue(mc) * 100; (controls[fake.id] || []).forEach((fn) => fn(S.values[fake.id])); };
      upd.persistent = true; mc.map.forEach(([id]) => (controls[id] = controls[id] || []).push(upd));
      controls[fake.id].forEach((fn) => (fn.persistent = true));
      el.append(k);
    });
  }
  // intercepta escrita dos macros
  const _setValue = setValue;
  setValue = function (id, v, opts = {}) {
    if (id.startsWith("macro_")) {
      const mc = MACROS.find((m) => "macro_" + m.name === id); const n = Math.min(1, Math.max(0, v / 100));
      S.values[id] = n * 100; (controls[id] || []).forEach((fn) => fn(S.values[id]));
      mc.on.forEach((pid) => { if (S.values[pid] < 0.5 && n > 0.02) _setValue(pid, 1); });
      mc.map.forEach(([pid, a, b]) => _setValue(pid, a + (b - a) * n, opts));
      return;
    }
    _setValue(id, v, opts);
  };

  // ---------------------------------------------------------------------------
  //  Atualização em tempo real (30 fps do C++)
  // ---------------------------------------------------------------------------
  function onUpdate(d) {
    if (d.v) for (const [id, v] of Object.entries(d.v)) { if (S.byId[id] && Math.abs(S.values[id] - v) > 1e-5 && !(viz && viz.drag)) { S.values[id] = v; refresh(id); } }
    if (d.m) {
      const M = (S.meters = d.m);
      const push = (arr, v) => { arr.push(v); if (arr.length > 300) arr.shift(); };
      push(S.hist.pitch, { midi: M.midi, out: M.midi ? M.midi + (M.corr || 0) : 0 });
      push(S.hist.gate, 20 * Math.log10(Math.max(1e-4, M.inL || 0)));
      push(S.hist.comp, M.comp || 0); push(S.hist.opto, M.opto || 0); push(S.hist.deess, M.deess || 0); push(S.hist.lufs, M.lufsS || -70);
      const bar = (sel, v) => ($(sel).style.width = Math.max(0, Math.min(100, ((20 * Math.log10(Math.max(1e-4, v)) + 60) / 60) * 100)) + "%");
      bar("#mIn", Math.max(M.inL, M.inR)); bar("#mOut", Math.max(M.outL, M.outR)); $("#mLim").style.width = Math.min(100, (M.lim || 0) * 8) + "%";
      $("#lufsS").textContent = M.lufsS > -69 ? M.lufsS.toFixed(1) : "–"; $("#lufsI").textContent = M.lufsI > -69 ? M.lufsI.toFixed(1) : "–";
      $("#bpmView").textContent = M.bpm ? Math.round(M.bpm) : "–";
      MODULES.forEach((m) => { if (m.gr && tiles[m.key]) tiles[m.key].gr.style.width = Math.min(100, (M[m.gr] || 0) * 8) + "%"; });
      const st = $("#nrStatus"); if (st) st.textContent = M.learning ? "Ouvindo o ruído… fica em silêncio" : M.noiseProfile ? "Perfil de ruído aprendido ✓" : "Fique 2 s em silêncio ao clicar";
      if (micState.active && micState.native) $("#micBtn").style.setProperty("--lvl", Math.min(0.5, M.mic || 0));
    }
    if (d.sp) S.sp = d.sp; if (d.bsp) S.bsp = d.bsp;
  }
  function loop() {
    const now = performance.now();
    for (const [id, t] of Object.entries(S.aiTouched)) if (now - t > 5000) { delete S.aiTouched[id]; (controls[id] || []).forEach((fn) => fn(S.values[id])); }
    drawViz(); requestAnimationFrame(loop);
  }

  // ---------------------------------------------------------------------------
  //  Chat / Cupidinho
  // ---------------------------------------------------------------------------
  const chat = () => $("#chat");
  function addMsg(role, html, extra) {
    const el = h("div", { class: "msg " + role }); el.innerHTML = html; if (extra) el.append(extra);
    chat().append(el); chat().scrollTop = chat().scrollHeight; return el;
  }
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
  function setBusy(b, status) { S.busy = b; $("#cupid").classList.toggle("busy", b); $("#agentStatus").textContent = status || (b ? "Pensando…" : "Fala o que tu quer no som"); }
  function changeChips(changed, snapshotBefore) {
    const box = h("div");
    const ids = Object.keys(changed).filter((id) => !id.startsWith("macro_"));
    if (ids.length) {
      const list = h("div", { class: "changes" });
      ids.slice(0, 14).forEach((id) => { const p = S.byId[id]; const mod = MODULES.find((m) => m.key === moduleOfParam[id]); const lab = p.unit === "bool" && mod ? mod.name : (mod ? mod.name + " " : "") + p.label.toLowerCase();
        list.append(h("span", { class: "chg", title: "Ir para o módulo", onclick: () => select(moduleOfParam[id] || S.sel) }, p.unit === "bool" ? `${lab} ${changed[id].to > 0.5 ? "ligado" : "desligado"}` : `${lab}: ${fmt(p, changed[id].from)} → ${fmt(p, changed[id].to)}`)); });
      if (ids.length > 14) list.append(h("span", { class: "chg" }, `+${ids.length - 14}`));
      box.append(list);
      box.append(h("div", { class: "actions" }, h("button", { onclick: () => { pushHistory(); applyValues(snapshotBefore, { history: false }); toast("Mudanças desfeitas"); } }, "Desfazer isso")));
    }
    return box;
  }
  function flyArrows(changed) {
    const mods = [...new Set(Object.keys(changed).map((id) => moduleOfParam[id]).filter(Boolean))];
    const from = $("#cupid").getBoundingClientRect();
    mods.slice(0, 6).forEach((key, i) => {
      const t = tiles[key]; if (!t) return; const to = t.el.getBoundingClientRect();
      const reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
      setTimeout(() => {
        t.el.classList.remove("ai-hit"); void t.el.offsetWidth; t.el.classList.add("ai-hit");
        if (reduce) return;
        const a = document.createElementNS("http://www.w3.org/2000/svg", "svg"); a.setAttribute("viewBox", "0 0 64 14"); a.classList.add("arrow-fly");
        a.innerHTML = '<path d="M2 7h50" stroke="#FFC56A" stroke-width="2.5" stroke-linecap="round"/><path d="M62 7l-11-5v10z" fill="#FF3E86"/><path d="M2 7l6-5M2 7l6 5M8 7l6-5M8 7l6 5" stroke="#FFE7B8" stroke-width="1.5"/>';
        document.body.append(a);
        const x0 = from.left, y0 = from.top + from.height / 2, x1 = to.right - 10, y1 = to.top + to.height / 2;
        const ang = Math.atan2(y1 - y0, x1 - x0) * 180 / Math.PI;
        a.animate([{ transform: `translate(${x0 - 64}px, ${y0 - 7}px) rotate(${ang}deg)`, opacity: 0 }, { opacity: 1, offset: 0.15 }, { transform: `translate(${x1 - 64}px, ${y1 - 7}px) rotate(${ang}deg)`, opacity: 1 }],
          { duration: 650, easing: "cubic-bezier(.3,.1,.2,1)" }).onfinish = () => a.remove();
      }, i * 120);
    });
  }

  async function send(text) {
    text = (text || "").trim(); if (!text || S.busy) return;
    $("#input").value = ""; autoGrow();
    addMsg("user", esc(text));
    const useClaude = Bridge.isPlugin && S.settings.agentMode !== "local" && S.settings.hasClaudeKey;
    if (useClaude) {
      setBusy(true, "Consultando o Claude…");
      const typing = addMsg("bot", '<span class="typing"><span></span><span></span><span></span></span>');
      const res = await Bridge.call("askAI", text, Agent.contextFor(S.values, { analise: S.analysis, bpm_host: S.meters.bpm, preset_atual: S.presetName }));
      typing.remove(); setBusy(false);
      if (res && res.ok && res.data) return applyAgentResult({ reply: res.reply, preset: res.data.preset, changes: res.data.changes || {}, actions: res.data.actions || [] }, true);
      addMsg("sys", esc((res && res.error) || "Falha na IA") + " — usando o modo local.");
    }
    const r = Agent.interpret(text, S.values);
    if (r.undo) { undo(); return addMsg("bot", esc(r.reply)); }
    applyAgentResult(r, false);
  }

  async function applyAgentResult(r, fromClaude) {
    const before = snapshot(); let changed = {};
    if (r.preset) {
      const p = typeof r.preset === "string" ? window.CUPIDO_PRESETS.find((x) => Agent.norm(x.name) === Agent.norm(r.preset)) : r.preset;
      if (p) changed = loadPreset(p, { ai: true });
    }
    if (r.changes && Object.keys(r.changes).length) Object.assign(changed, applyValues(r.changes, { ai: true, history: !r.preset }));
    const msg = addMsg("bot", esc(r.reply || "Feito."), changeChips(changed, before));
    if (Object.keys(changed).length) { flyArrows(changed); const first = moduleOfParam[Object.keys(changed).find((k) => moduleOfParam[k])]; if (first && Object.keys(changed).length < 10) select(first); }
    for (const a of r.actions || []) {
      if (a === "learn_noise") learnNoise();
      if (a === "reset_lufs") Bridge.call("resetLufs");
    }
    const acts = r.actions || [];
    if (acts.includes("auto_mix")) await autoMix();
    else { if (acts.includes("analyze_beat")) await analyse("beat", true); if (acts.includes("analyze_voice")) await analyse("voice", true); }
    return msg;
  }

  function analysisCard(a) {
    const box = h("div", { class: "analysis-card" });
    const cell = (v, l) => h("div", {}, h("b", { class: "num" }, v), h("small", {}, l));
    box.append(cell(a.bpm > 0 ? a.bpm.toFixed(1) : "–", "BPM"), cell(a.key >= 0 ? `${a.keyName}${a.minor ? "m" : ""}` : "–", a.key >= 0 ? (a.minor ? "menor" : "maior") : "tom"), cell(a.rmsDb > -90 ? a.rmsDb.toFixed(1) : "–", "dB RMS"));
    const bal = h("div", { class: "balance", title: "Sub • Grave • Médio-grave • Médio • Presença • Ar" });
    const mx = Math.max(...a.bands, 1); a.bands.forEach((b) => bal.append(h("i", { style: `height:${Math.max(4, (b / mx) * 100)}%` })));
    box.append(bal); return box;
  }
  async function analyse(which, talk) {
    setBusy(true, which === "beat" ? "Ouvindo o beat…" : "Ouvindo a voz…");
    const a = await Bridge.call("analyze", which); setBusy(false);
    if (!a || !a.valid) {
      if (talk) addMsg("bot", which === "beat"
        ? "Ainda não ouvi o beat. Toca o projeto por uns 10 segundos com o beat mandado pro <b>sidechain</b> do CUPIDO (no DAW: envia a faixa do beat para a entrada lateral do plugin) e me pede de novo."
        : "Não ouvi voz suficiente. Toca uns 10 segundos da faixa de voz e tenta de novo.");
      return null;
    }
    if (which === "beat" && !a.sidechainActive && talk) addMsg("sys", "Sem sidechain ativo: analisei o próprio sinal do canal.");
    S.analysis[which] = a; legendFor(viz);
    if (talk) {
      const msg = addMsg("bot", which === "beat" ? `Ouvi o beat: <b>${a.bpm > 0 ? Math.round(a.bpm) + " BPM" : "BPM incerto"}</b> em <b>${a.key >= 0 ? Agent.KEYS[a.key] + (a.minor ? " menor" : " maior") : "tom incerto"}</b>.` : `Ouvi tua voz: tom provável <b>${a.key >= 0 ? Agent.KEYS[a.key] + (a.minor ? " menor" : " maior") : "–"}</b>, ruído de fundo em ${Math.round(a.noiseFloorDb)} dB.`, analysisCard(a));
      if (which === "beat" && a.key >= 0) msg.append(h("div", { class: "actions" },
        h("button", { onclick: () => { const before = snapshot(); const ch = applyValues({ tune_on: 1, tune_key: a.key, tune_scale: a.minor ? 2 : 1 }, { ai: true }); flyArrows(ch); addMsg("bot", `Autotune em ${Agent.KEYS[a.key]} ${a.minor ? "menor" : "maior"}.`, changeChips(ch, before)); } }, "Afinar nesse tom"),
        h("button", { onclick: () => autoMix() }, "Mixar com base nisso")));
    }
    return a;
  }
  async function autoMix() {
    addMsg("sys", "Analisando beat e voz…");
    const beat = await analyse("beat", false), voice = await analyse("voice", false);
    if (!beat && !voice) return addMsg("bot", "Preciso ouvir alguma coisa primeiro: dá play no projeto por uns 10 segundos (voz no canal e, se der, o beat no sidechain) e me pede de novo.");
    const before = snapshot();
    const r = Agent.autoMix(voice, beat, S.values, (beat && beat.hostBpm) || (voice && voice.hostBpm) || 0);
    const changed = applyValues(r.changes, { ai: true }); flyArrows(changed);
    const box = h("div"); if (beat) box.append(analysisCard(beat)); box.append(changeChips(changed, before));
    addMsg("bot", "Mixagem montada! " + esc(r.notes.join("; ")) + ".", box);
  }
  function learnNoise() { Bridge.call("learnNoise"); setValue("nr_on", 1); toast("Aprendendo o ruído: 2 segundos de silêncio"); }

  // ---------------------------------------------------------------------------
  //  Voz: segura o botão (ou a tecla V) e fala
  // ---------------------------------------------------------------------------
  const micState = { active: false, native: false, rec: null, finalText: "" };
  const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
  function voiceMode() {
    if (Bridge.isPlugin && S.settings.sttProvider !== "browser") return "native";
    return SR ? "browser" : (Bridge.isPlugin ? "native" : "none");
  }
  async function micDown() {
    if (micState.active) return;
    const mode = voiceMode();
    if (mode === "none") return addMsg("sys", "Comando de voz: abra no Chrome/Edge (prévia) ou configure a transcrição em ⚙ Configurações → Voz (no plugin).");
    micState.active = true; micState.finalText = ""; $("#micBtn").classList.add("rec"); setBusy(true, "Ouvindo… solta pra enviar");
    if (mode === "native") {
      micState.native = true;
      const r = await Bridge.call("startVoice", S.settings.voiceSource || "mic");
      if (!r || !r.ok) { micState.active = false; $("#micBtn").classList.remove("rec"); setBusy(false); if (SR && r && r.error === "browser") return; addMsg("sys", esc((r && r.error) || "Não consegui abrir o microfone.")); }
      return;
    }
    micState.native = false;
    const rec = new SR(); rec.lang = (S.settings.voiceLanguage || "pt") === "pt" ? "pt-BR" : S.settings.voiceLanguage; rec.interimResults = true; rec.continuous = true;
    rec.onresult = (e) => { let interim = "", fin = ""; for (let i = 0; i < e.results.length; i++) (e.results[i].isFinal ? (fin += e.results[i][0].transcript) : (interim += e.results[i][0].transcript)); micState.finalText = (fin + " " + interim).trim(); $("#input").value = micState.finalText; };
    rec.onerror = (e) => { if (e.error !== "aborted" && e.error !== "no-speech") addMsg("sys", "Reconhecimento de voz do navegador falhou (" + e.error + "). No plugin, configure Groq/OpenAI em ⚙."); };
    micState.rec = rec; try { rec.start(); } catch (e) {}
  }
  async function micUp() {
    if (!micState.active) return;
    micState.active = false; $("#micBtn").classList.remove("rec"); $("#micBtn").style.setProperty("--lvl", 0);
    if (micState.native) {
      setBusy(true, "Transcrevendo…");
      const r = await Bridge.call("stopVoice"); setBusy(false);
      if (r && r.ok && r.text) send(r.text); else addMsg("sys", esc((r && r.error) || "Não entendi o áudio."));
      return;
    }
    const rec = micState.rec; setBusy(false);
    if (rec) { rec.onend = () => { const t = micState.finalText || $("#input").value; if (t.trim()) send(t); }; try { rec.stop(); } catch (e) {} }
  }

  // ---------------------------------------------------------------------------
  //  Presets (modal)
  // ---------------------------------------------------------------------------
  async function openPresets(focusSave) {
    S.userPresets = (await Bridge.call("listPresets")) || [];
    const modal = $("#presetModal .modal"); modal.innerHTML = "";
    const cats = [...new Set(window.CUPIDO_PRESETS.map((p) => p.cat)), "Meus presets"];
    let cat = S.presetCat && cats.includes(S.presetCat) ? S.presetCat : cats[1];
    const catBox = h("div", { class: "preset-cats" }), items = h("div", { class: "preset-items" });
    const draw = () => {
      catBox.innerHTML = ""; cats.forEach((c) => catBox.append(h("button", { class: c === cat ? "on" : "", onclick: () => { cat = c; draw(); } }, c)));
      items.innerHTML = "";
      const list = cat === "Meus presets" ? S.userPresets.map((u) => ({ name: u.name, cat: "Meus presets", desc: "Preset salvo por você", data: u.data, user: true })) : window.CUPIDO_PRESETS.filter((p) => p.cat === cat);
      if (!list.length) items.append(h("p", {}, "Nenhum preset ainda. Ajuste o som e use “Salvar”."));
      list.forEach((p) => items.append(h("div", { class: "preset-item" + (p.name === S.presetName ? " on" : ""), onclick: () => { const ch = loadPreset(p.user ? { name: p.name, cat: "Meus presets", values: p.data } : p); closeModals(); toast(`Preset “${p.name}” carregado`); flyArrows(ch); } },
        h("div", { class: "grow" }, h("b", {}, p.name), h("p", {}, p.desc)),
        p.user ? h("button", { class: "del", title: "Apagar", onclick: async (e) => { e.stopPropagation(); await Bridge.call("deletePreset", p.name); S.userPresets = (await Bridge.call("listPresets")) || []; draw(); } }, "✕") : null)));
    };
    const nameIn = h("input", { type: "text", placeholder: "Nome do preset", value: S.presetCat === "Meus presets" ? S.presetName : "" });
    const save = async () => { const n = nameIn.value.trim(); if (!n) return nameIn.focus(); await Bridge.call("savePreset", n, JSON.stringify(S.values)); S.presetName = n; S.presetCat = "Meus presets"; $("#presetName").textContent = n; $("#presetCat").textContent = "Meus presets"; Bridge.call("setPresetName", n); S.userPresets = (await Bridge.call("listPresets")) || []; cat = "Meus presets"; draw(); toast("Preset salvo"); };
    modal.append(h("h2", {}, "Presets"), h("p", {}, "Estilos prontos para começar. O tom do autotune é mantido ao trocar de preset."),
      h("div", { class: "preset-list" }, catBox, items),
      h("h3", {}, "Salvar o som atual"), h("div", { class: "row" }, nameIn, h("button", { class: "btn primary", style: "flex:0 0 auto", onclick: save }, "Salvar preset")),
      h("div", { class: "foot" }, Bridge.isPlugin ? h("button", { class: "btn", onclick: () => Bridge.call("openPresetsFolder") }, "Abrir pasta") : null, h("button", { class: "btn", onclick: closeModals }, "Fechar")));
    draw(); $("#presetModal").classList.add("show"); if (focusSave) nameIn.focus();
  }
  function stepPreset(dir) {
    const all = window.CUPIDO_PRESETS; let i = all.findIndex((p) => p.name === S.presetName); i = (i + dir + all.length) % all.length;
    const ch = loadPreset(all[i]); toast(`${all[i].name}`); flyArrows(ch);
  }

  // ---------------------------------------------------------------------------
  //  Configurações
  // ---------------------------------------------------------------------------
  async function openSettings() {
    S.settings = (await Bridge.call("getSettings")) || S.settings;
    const s = S.settings; const modal = $("#settingsModal .modal"); modal.innerHTML = "";
    const sel = (id, opts, val) => h("select", { id }, opts.map(([v, l]) => h("option", Object.assign({ value: v }, v === val ? { selected: true } : {}), l)));
    const STT = { groq: ["https://api.groq.com/openai/v1/audio/transcriptions", "whisper-large-v3-turbo"], openai: ["https://api.openai.com/v1/audio/transcriptions", "gpt-4o-mini-transcribe"], local: ["http://127.0.0.1:8000/v1/audio/transcriptions", "Systran/faster-whisper-small"] };
    const note = h("div", { class: "note", style: "margin-top:12px" }, Bridge.isPlugin ? "As chaves ficam salvas só neste computador, fora do projeto do DAW." : "Prévia do navegador: as configurações reais ficam no plugin.");
    modal.append(
      h("h2", {}, "Configurações do Cupidinho"),
      h("p", {}, "O agente funciona sem internet no modo local. Com uma chave do Claude ele entende pedidos bem mais livres."),
      h("h3", {}, "Inteligência"),
      h("label", { for: "agentMode" }, "Modo do agente"), sel("agentMode", [["auto", "Automático: Claude quando tiver chave, senão local"], ["local", "Só local (offline)"]], s.agentMode),
      h("div", { class: "row" },
        h("div", {}, h("label", { for: "claudeKey" }, `Chave da API do Claude ${s.hasClaudeKey ? "(salva: " + s.claudeKeyMasked + ")" : ""}`), h("input", { id: "claudeKey", type: "password", placeholder: s.hasClaudeKey ? "deixe vazio para manter" : "sk-ant-…", autocomplete: "off" })),
        h("div", {}, h("label", { for: "claudeModel" }, "Modelo"), sel("claudeModel", [["claude-haiku-5-5", "Claude Haiku 5.5 (rápido e barato)"], ["claude-sonnet-5-5", "Claude Sonnet 5.5 (mais esperto)"], ["claude-opus-5-5", "Claude Opus 5.5 (máximo)"]], s.claudeModel))),
      h("p", {}, "Pegue a chave em ", h("button", { class: "btn link", onclick: () => Bridge.call("openUrl", "https://console.anthropic.com/settings/keys") }, "console.anthropic.com"), "."),
      h("h3", {}, "Voz"),
      h("div", { class: "row" },
        h("div", {}, h("label", { for: "sttProvider" }, "Transcrição"), sel("sttProvider", [["groq", "Groq Whisper (grátis, rápido)"], ["openai", "OpenAI"], ["local", "Servidor local (offline)"], ["browser", "Reconhecimento do navegador"]], s.sttProvider)),
        h("div", {}, h("label", { for: "voiceSource" }, "Microfone"), sel("voiceSource", [["mic", "Microfone do sistema"], ["channel", "Entrada do próprio canal"]], s.voiceSource))),
      h("div", { class: "row" },
        h("div", {}, h("label", { for: "sttUrl" }, "Endereço"), h("input", { id: "sttUrl", type: "text", value: s.sttUrl || "" })),
        h("div", { style: "flex:0 0 38%" }, h("label", { for: "sttModel" }, "Modelo"), h("input", { id: "sttModel", type: "text", value: s.sttModel || "" }))),
      h("label", { for: "sttKey" }, `Chave da transcrição ${s.hasSttKey ? "(salva: " + s.sttKeyMasked + ")" : ""}`), h("input", { id: "sttKey", type: "password", placeholder: s.hasSttKey ? "deixe vazio para manter" : "gsk_… / sk-…", autocomplete: "off" }),
      h("p", {}, "Groq: crie a chave grátis em ", h("button", { class: "btn link", onclick: () => Bridge.call("openUrl", "https://console.groq.com/keys") }, "console.groq.com/keys"), ". Segure o botão do microfone (ou a tecla V) e fale."),
      note,
      h("div", { class: "foot" },
        h("button", { class: "btn", onclick: async () => { await saveSettings(); note.className = "note"; note.textContent = "Testando…"; const r = await Bridge.call("testAI"); note.className = "note " + (r && r.ok ? "ok" : "err"); note.textContent = r && r.ok ? "Claude conectado: " + r.text : "Falhou: " + ((r && r.error) || "sem resposta"); } }, "Testar Claude"),
        h("button", { class: "btn", onclick: closeModals }, "Cancelar"),
        h("button", { class: "btn primary", onclick: async () => { await saveSettings(); closeModals(); toast("Configurações salvas"); } }, "Salvar")));
    $("#sttProvider", modal).addEventListener("change", (e) => { const v = STT[e.target.value]; if (v) { $("#sttUrl", modal).value = v[0]; $("#sttModel", modal).value = v[1]; } });
    $("#settingsModal").classList.add("show");
  }
  async function saveSettings() {
    const m = $("#settingsModal .modal"); const g = (id) => $("#" + id, m).value;
    S.settings = (await Bridge.call("setSettings", { agentMode: g("agentMode"), claudeKey: g("claudeKey"), claudeModel: g("claudeModel"), sttProvider: g("sttProvider"), voiceSource: g("voiceSource"), sttUrl: g("sttUrl"), sttModel: g("sttModel"), sttKey: g("sttKey") })) || S.settings;
    updateAiMode();
  }
  function updateAiMode() {
    const claude = Bridge.isPlugin && S.settings.agentMode !== "local" && S.settings.hasClaudeKey;
    const el = $("#aiMode"); el.textContent = claude ? (S.settings.claudeModel || "claude").replace("claude-", "").replace(/-(\d)-(\d)$/, " $1.$2") : "local"; el.classList.toggle("claude", claude);
  }
  function openHelp() {
    const m = $("#helpModal .modal"); m.innerHTML = "";
    m.append(h("h2", {}, "Como usar o CUPIDO"),
      h("h3", {}, "Falando com o Cupidinho"), h("p", {}, "Escreve ou segura o microfone e fala do jeito que tu fala: “bota mais saturação nos agudos”, “delay mais demorado”, “vocal mais aberto”, “estilo Teto”, “autotune robótico em lá menor”, “mixa pra mim”. Cada mudança aparece em dourado e pode ser desfeita."),
      h("h3", {}, "Fazendo ele ouvir o beat"), h("p", {}, "Manda a faixa do beat para o sidechain do CUPIDO (entrada lateral). Dá play uns 10 segundos e clica “Ouvir o beat”: ele detecta BPM, tom e onde o beat ocupa o espectro, afina o autotune e abre espaço na voz."),
      h("h3", {}, "Atalhos"), h("p", {}, "Ctrl+Z desfaz • Ctrl+Shift+Z refaz • V (segurar) comando de voz • Enter envia • Nos knobs: arrastar, Shift = ajuste fino, duplo clique = padrão, clique no número = digitar • No EQ: arrastar pontos, roda do mouse = Q."),
      h("h3", {}, "Modo Gravação"), h("p", {}, "Clique em “Mixagem HQ” no topo para trocar para o modo de baixa latência (para gravar ouvindo o efeito). Ele desliga a redução de ruído e encurta o lookahead."),
      h("div", { class: "foot" }, h("button", { class: "btn primary", onclick: closeModals }, "Fechou")));
    $("#helpModal").classList.add("show");
  }
  function closeModals() { document.querySelectorAll(".overlay").forEach((o) => o.classList.remove("show")); }
  let toastT = 0; function toast(t) { const el = $("#toast"); el.textContent = t; el.classList.add("show"); clearTimeout(toastT); toastT = setTimeout(() => el.classList.remove("show"), 1600); }

  // ---------------------------------------------------------------------------
  //  A/B
  // ---------------------------------------------------------------------------
  function switchSlot(to) {
    if (to === S.slot) return; S.slots[S.slot] = snapshot();
    if (S.slots[to]) applyValues(S.slots[to]); S.slot = to;
    $("#slotA").classList.toggle("on", to === "A"); $("#slotB").classList.toggle("on", to === "B"); toast("Versão " + to);
  }

  // ---------------------------------------------------------------------------
  //  Inicialização
  // ---------------------------------------------------------------------------
  function autoGrow() { const t = $("#input"); t.style.height = "40px"; t.style.height = Math.min(110, t.scrollHeight) + "px"; }
  async function init() {
    const r = await Bridge.call("getSpec");
    if (r && r.spec) S.spec = r.spec;
    S.spec.forEach((p) => (S.byId[p.id] = p));
    S.spec.forEach((p) => (S.values[p.id] = r && r.values && p.id in r.values ? r.values[p.id] : p.def));
    S.info = (r && r.info) || {}; S.settings = (r && r.settings) || {};
    S.presetName = S.info.preset || "Init"; const fp = window.CUPIDO_PRESETS.find((p) => p.name === S.presetName); S.presetCat = fp ? fp.cat : (S.presetName === "Init" ? "Padrão" : "Meus presets");
    $("#presetName").textContent = S.presetName; $("#presetCat").textContent = S.presetCat; $("#ver").textContent = "v" + (S.info.version || "1.0");
    S.slots.A = snapshot();
    renderRack(); renderModule(); renderMacros(); updateAiMode();
    Bridge.onUpdate(onUpdate); requestAnimationFrame(loop);

    const chips = [["Mixa pra mim", "mixa pra mim e deixa perfeito"], ["Ouvir o beat", "identifica o beat"], ["Estilo Matuê", "quero uma voz no estilo do Matuê"], ["Estilo Teto", "estilo teto"], ["Estilo Yunk Vino", "voz parecida com a do yunk vino"],
      ["Vocal mais aberto", "vocal mais aberto"], ["Delay mais demorado", "delay mais demorado"], ["Saturação nos agudos", "aumenta a saturação nos agudos"], ["Autotune robótico", "autotune robótico"], ["Tirar ruído", "tira o ruído de fundo"], ["Masterizar", "masteriza"]];
    chips.forEach(([l, cmd]) => $("#chips").append(h("button", { onclick: () => send(cmd) }, l)));

    addMsg("bot", `E aí! Eu sou o <b>Cupidinho</b>. Me diz o som que tu quer — escrevendo ou segurando o <b>microfone</b> — que eu mexo em tudo pra ti. Se mandar o beat pro sidechain, eu ouço o BPM e o tom e mixo em cima dele.`);
    if (!Bridge.isPlugin) $("header.top .spacer").after(h("span", { class: "preview-banner", title: "O processamento de áudio roda dentro do plugin no DAW" }, "Prévia no navegador"));

    // eventos
    $("#sendBtn").onclick = () => send($("#input").value);
    $("#input").addEventListener("keydown", (e) => { if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); send($("#input").value); } e.stopPropagation(); });
    $("#input").addEventListener("keyup", (e) => e.stopPropagation()); $("#input").addEventListener("input", autoGrow);
    const mic = $("#micBtn");
    mic.addEventListener("pointerdown", (e) => { e.preventDefault(); mic.setPointerCapture(e.pointerId); micDown(); });
    mic.addEventListener("pointerup", micUp); mic.addEventListener("pointercancel", micUp);
    document.addEventListener("keydown", (e) => {
      if (e.target.tagName === "INPUT" || e.target.tagName === "TEXTAREA" || e.target.tagName === "SELECT") return;
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "z") { e.preventDefault(); e.shiftKey ? redo() : undo(); }
      else if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "y") { e.preventDefault(); redo(); }
      else if (e.key.toLowerCase() === "v" && !e.repeat && !e.ctrlKey && !e.metaKey) micDown();
      else if (e.key === "Escape") closeModals();
    });
    document.addEventListener("keyup", (e) => { if (e.key.toLowerCase() === "v") micUp(); });
    $("#undo").onclick = undo; $("#redo").onclick = redo;
    $("#slotA").onclick = () => switchSlot("A"); $("#slotB").onclick = () => switchSlot("B");
    $("#abCopy").onclick = () => { S.slots[S.slot === "A" ? "B" : "A"] = snapshot(); toast(`Copiado para ${S.slot === "A" ? "B" : "A"}`); };
    $("#presetOpen").onclick = () => openPresets(false); $("#presetOpen").onkeydown = (e) => { if (e.key === "Enter") openPresets(false); };
    $("#presetSave").onclick = () => openPresets(true);
    $("#presetPrev").onclick = () => stepPreset(-1); $("#presetNext").onclick = () => stepPreset(1);
    $("#settingsBtn").onclick = openSettings; $("#helpBtn").onclick = openHelp;
    $("#modePill").onclick = () => { pushHistory(); setValue("latency_mode", S.values.latency_mode > 0.5 ? 0 : 1); toast(S.values.latency_mode > 0.5 ? "Modo gravação (baixa latência)" : "Modo mixagem (alta qualidade)"); };
    document.querySelectorAll(".overlay").forEach((o) => o.addEventListener("pointerdown", (e) => { if (e.target === o) closeModals(); }));
    window.addEventListener("resize", () => drawViz());
  }
  init();
})();
