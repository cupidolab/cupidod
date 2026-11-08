// =============================================================================
//  CUPIDO — ponte JS <-> C++
//  No plugin: usa a API de baixo nível do JUCE 8 (window.__JUCE__.backend):
//    chamada:  emitEvent("__juce__invoke", { name, params, resultId })
//    resposta: evento "__juce__complete" { promiseId, result }
//    eventos do C++: addEventListener("cupido", cb)
//  No navegador (prévia): um simulador responde no lugar do plugin.
// =============================================================================
window.Bridge = (function () {
  const backend = window.__JUCE__ && window.__JUCE__.backend;
  const isPlugin = !!backend;
  let nextId = 1;
  const pending = new Map();
  const listeners = [];

  if (isPlugin) {
    backend.addEventListener("__juce__complete", ({ promiseId, result }) => {
      const p = pending.get(promiseId);
      if (p) { pending.delete(promiseId); p(result); }
    });
    backend.addEventListener("cupido", (data) => listeners.forEach((l) => l(data)));
  }

  function call(name, ...params) {
    if (!isPlugin) return Mock.call(name, params);
    return new Promise((resolve) => {
      const id = nextId++;
      pending.set(id, resolve);
      backend.emitEvent("__juce__invoke", { name, params, resultId: id });
    });
  }
  function onUpdate(cb) { listeners.push(cb); if (!isPlugin) Mock.start(cb); }

  // ---------------------------------------------------------------------------
  //  Simulador (só na prévia do navegador): medidores, espectro, pitch e análise
  // ---------------------------------------------------------------------------
  const Mock = (function () {
    const spec = window.CUPIDO_SPEC;
    const values = {}; spec.forEach((p) => (values[p.id] = p.def));
    let t = 0, timer = null;
    const store = {
      get(k, d) { try { const v = localStorage.getItem("cupido:" + k); return v ? JSON.parse(v) : d; } catch (e) { return d; } },
      set(k, v) { try { localStorage.setItem("cupido:" + k, JSON.stringify(v)); } catch (e) {} }
    };
    function settings() {
      return Object.assign({ agentMode: "local", claudeModel: "claude-haiku-5-5", hasClaudeKey: false, sttProvider: "browser",
        sttUrl: "", sttModel: "", hasSttKey: false, voiceSource: "mic", voiceLanguage: "pt", presetsFolder: "(navegador)" }, store.get("settings", {}));
    }
    function start(cb) {
      if (timer) return;
      timer = setInterval(() => {
        t += 1 / 30;
        const phrase = Math.max(0, Math.sin(t * 0.9)) * (0.6 + 0.4 * Math.sin(t * 7.3) * Math.sin(t * 2.1));
        const lvl = Math.min(1, 0.08 + phrase * 0.75);
        const midi = 57 + 2 * Math.round(Math.sin(t * 0.45) * 2) / 2 + 0.35 * Math.sin(t * 5.5) + 0.25 * Math.sin(t * 1.3);
        const tuneOn = values.tune_on > 0.5;
        const target = Math.round(midi);
        const corr = tuneOn ? (target - midi) * (values.tune_amount / 100) : 0;
        const sp = [];
        for (let i = 0; i < 96; i++) {
          const f = 20 * Math.pow(1000, i / 96);
          let db = -32 - 9 * Math.log2(Math.max(1, f / 300)) + 7 * Math.exp(-Math.pow(Math.log2(f / 220), 2) * 2);
          db += values.eq_on > 0.5 ? (f > 6000 ? values.eq_high_gain * 0.8 : 0) + (f < 200 ? values.eq_low_gain * 0.8 : 0) : 0;
          if (f < values.eq_hpf) db -= 24 * Math.log2(values.eq_hpf / f);
          db += values.air_on > 0.5 && f > 5000 ? values.air_high * 0.06 : 0;
          db += (Math.random() - 0.5) * 4 + 20 * Math.log10(lvl + 0.02);
          sp.push(Math.round(db * 10) / 10);
        }
        const bsp = sp.map((d, i) => Math.round((d + 8 - i * 0.06 + (i < 18 ? 10 : 0)) * 10) / 10);
        const comp = values.comp_on > 0.5 ? Math.max(0, (20 * Math.log10(lvl) + 6 - values.comp_thresh) * (1 - 1 / values.comp_ratio)) : 0;
        cb({
          m: {
            inL: lvl, inR: lvl * 0.97, outL: Math.min(1, lvl * 1.15), outR: Math.min(1, lvl * 1.12),
            gate: phrase < 0.05 && values.gate_on > 0.5 ? values.gate_range * 0.8 : 0,
            deess: values.ds_on > 0.5 ? Math.max(0, Math.sin(t * 3.7) * 6 - 2) : 0,
            comp, opto: values.opto_on > 0.5 ? comp * 0.5 : 0, glue: values.glue_on > 0.5 ? 1.5 : 0,
            lim: values.lim_on > 0.5 ? Math.max(0, lvl * 6 + values.lim_gain * 0.6 - 3) : 0,
            hz: phrase > 0.05 ? 440 * Math.pow(2, (midi - 69) / 12) : 0, midi: phrase > 0.05 ? midi : 0, target: phrase > 0.05 ? target : 0, corr,
            lufsM: -14 + phrase * 6 + values.lim_gain * 0.8, lufsS: -12 + values.lim_gain * 0.8, lufsI: -12.4 + values.lim_gain * 0.8,
            sc: true, bpm: 140, learning: false, noiseProfile: false, mic: 0, latency: 1841
          },
          sp, bsp
        });
      }, 33);
    }
    const presets = () => store.get("userPresets", []);
    function call(name, params) {
      switch (name) {
        case "getSpec":  return Promise.resolve({ spec, values: Object.assign({}, values), info: { version: "1.0.0", format: "Prévia (navegador)", sampleRate: 48000, latency: 1841, preset: "Init", host: "Navegador" }, settings: settings() });
        case "setParam": values[params[0]] = params[1]; return Promise.resolve();
        case "setParams": Object.assign(values, params[0]); return Promise.resolve();
        case "analyze": {
          const beat = params[0] === "beat";
          return new Promise((res) => setTimeout(() => res(beat
            ? { valid: true, source: "beat", seconds: 16, bpm: 140.2, bpmConfidence: 0.92, key: 9, keyName: "A", minor: true, keyConfidence: 0.81,
                rmsDb: -13.5, peakDb: -0.8, crestDb: 12.7, noiseFloorDb: -60, sibilanceDb: -21, bands: [22, 36, 15, 13, 9, 5], sidechainActive: true, hostBpm: 140 }
            : { valid: true, source: "voice", seconds: 16, bpm: 0, key: 9, keyName: "A", minor: true, keyConfidence: 0.6,
                rmsDb: -21, peakDb: -4, crestDb: 17, noiseFloorDb: -55, sibilanceDb: -14, bands: [1, 14, 33, 30, 15, 7], sidechainActive: true, hostBpm: 140 }), 900));
        }
        case "askAI": return Promise.resolve({ ok: false, error: "Na prévia do navegador a IA do Claude não está conectada — usando o modo local." });
        case "getSettings": return Promise.resolve(settings());
        case "setSettings": { const s = Object.assign(settings(), params[0]); delete s.claudeKey; delete s.sttKey; store.set("settings", s); return Promise.resolve(s); }
        case "testAI": return Promise.resolve({ ok: false, error: "Teste disponível dentro do plugin." });
        case "listPresets": return Promise.resolve(presets());
        case "savePreset": { const list = presets().filter((p) => p.name !== params[0]); list.push({ name: params[0], data: JSON.parse(params[1]) }); store.set("userPresets", list); return Promise.resolve(true); }
        case "deletePreset": store.set("userPresets", presets().filter((p) => p.name !== params[0])); return Promise.resolve(true);
        case "pickAudio": return pickInBrowser(params[0]);
        case "loadAudioData": return fakeMedia(params[0], params[1], null);
        case "clearAudio": return Promise.resolve(true);
        case "getMedia": return Promise.resolve({});
        case "renderMix": return new Promise((res) => setTimeout(() => res({ ok: true, after: "previa.wav", before: "previa-antes.wav", lufsAfter: values.lufs_target, lufsBefore: values.lufs_target,
            voiceSpectrum: spec32(-30, { mud: 5, air: -14 }), beatSpectrum: spec32(-27, { sub: 12, pres: 3 }), beatGainDb: -2 }), 1200));
        case "startVoice": return Promise.resolve({ ok: false, error: "browser" });
        case "stopVoice": return Promise.resolve({ ok: false, error: "browser" });
        default: return Promise.resolve(true);
      }
    }
    // ---- anexos na prévia: lê o arquivo de verdade (forma de onda) e simula a análise ----
    function spec32(base, o) {
      return Array.from({ length: 32 }, (_, i) => { const f = 20 * Math.pow(1000, (i + 0.5) / 32);
        return base - 6 * Math.log2(Math.max(1, f / 400)) + (o.sub && f < 120 ? o.sub : 0) + (o.mud && f > 200 && f < 450 ? o.mud : 0) + (o.pres && f > 1000 && f < 5000 ? o.pres : 0) + (o.air && f > 10000 ? o.air : 0); });
    }
    async function fakeMedia(kind, name, file) {
      let wave = Array.from({ length: 160 }, (_, i) => 0.3 + 0.5 * Math.abs(Math.sin(i * 0.37)) * Math.random()), seconds = 150;
      if (file && window.AudioContext) {
        try { const buf = await new AudioContext().decodeAudioData(await file.arrayBuffer()); seconds = buf.duration; const d = buf.getChannelData(0), step = Math.floor(d.length / 160);
          wave = Array.from({ length: 160 }, (_, i) => { let p = 0; for (let j = i * step; j < (i + 1) * step; j += 32) p = Math.max(p, Math.abs(d[j])); return p; }); } catch (e) {}
      }
      const beat = kind !== "voice";
      return { ok: true, media: { kind, name: name || "arquivo", valid: true, seconds, sampleRate: 44100, bpm: beat ? 142 : 0, bpmConfidence: 0.8, key: 9, keyName: "A", minor: true, keyConfidence: 0.7,
        lufs: beat ? -8.5 : -19, rmsDb: beat ? -11 : -21, peakDb: -0.5, crestDb: beat ? 11 : 17, noiseFloorDb: beat ? -60 : -54, sibilanceDb: -15, centroidHz: 900,
        subSustain: beat ? 0.55 : 0.02, hatRate: 7, kickRate: 2, tripletRatio: 0.5, tripletRatioHalf: 0.5, offbeatRatio: 0.4, offbeatRatioHalf: 1, stereoWidth: 0.5,
        bands: beat ? [24, 34, 14, 13, 10, 5] : [1, 14, 33, 30, 15, 7], spectrum: beat ? spec32(-27, { sub: 12, pres: 3 }) : spec32(-30, { mud: 5, air: -14 }), wave } };
    }
    function pickInBrowser(kind) {
      return new Promise((res) => {
        const inp = document.createElement("input"); inp.type = "file"; inp.accept = "audio/*";
        inp.onchange = async () => { const f = inp.files[0]; if (!f) return res({ ok: false, cancelled: true }); res(await fakeMedia(kind, f.name, f)); };
        inp.click();
        setTimeout(() => { if (!inp.files || !inp.files.length) {} }, 0);
      });
    }
    return { call, start };
  })();

  return { call, onUpdate, isPlugin };
})();
