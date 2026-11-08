#include "AiClient.h"

namespace cupido
{
// =============================================================================
//  Configurações
// =============================================================================
AiSettings::AiSettings()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "CUPIDO"; o.filenameSuffix = ".settings"; o.folderName = "CupidoAudio";
    o.osxLibrarySubFolder = "Application Support"; o.commonToAllUsers = false;
    o.millisecondsBeforeSaving = 200; o.storageFormat = juce::PropertiesFile::storeAsXML;
    props = std::make_unique<juce::PropertiesFile> (o);
}

juce::String AiSettings::get (const juce::String& key, const juce::String& fallback) const
{ return props->getValue (key, fallback); }

void AiSettings::set (const juce::String& key, const juce::String& value)
{ props->setValue (key, value); props->saveIfNeeded(); }

static juce::String mask (const juce::String& k)
{ return k.isEmpty() ? juce::String() : ("••••" + k.getLastCharacters (4)); }

juce::var AiSettings::toPublicJson() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("agentMode",      get ("agentMode", "auto"));
    o->setProperty ("claudeModel",    get ("claudeModel", "claude-haiku-5-5"));
    o->setProperty ("claudeKeyMasked", mask (get ("claudeKey")));
    o->setProperty ("hasClaudeKey",   get ("claudeKey").isNotEmpty());
    o->setProperty ("sttProvider",    get ("sttProvider", "local"));
    o->setProperty ("whisperModel",   get ("whisperModel", "base"));
    o->setProperty ("sttUrl",         get ("sttUrl", "https://api.groq.com/openai/v1/audio/transcriptions"));
    o->setProperty ("sttModel",       get ("sttModel", "whisper-large-v3-turbo"));
    o->setProperty ("sttKeyMasked",   mask (get ("sttKey")));
    o->setProperty ("hasSttKey",      get ("sttKey").isNotEmpty());
    o->setProperty ("voiceSource",    get ("voiceSource", "mic"));
    o->setProperty ("voiceLanguage",  get ("voiceLanguage", "pt"));
    o->setProperty ("presetsFolder",  presetsFolder().getFullPathName());
    o->setProperty ("simpleMode",     get ("simpleMode", "1"));
    return juce::var (o);
}

void AiSettings::applyFromJson (const juce::var& obj)
{
    if (auto* o = obj.getDynamicObject())
        for (auto& prop : o->getProperties())
        {
            const auto key = prop.name.toString(); const auto val = prop.value.toString();
            // chaves só são trocadas quando o usuário digita uma nova (campo vazio = manter)
            if ((key == "claudeKey" || key == "sttKey") && val.trim().isEmpty()) continue;
            if (key == "clearClaudeKey" && (bool) prop.value) { set ("claudeKey", {}); continue; }
            if (key == "clearSttKey" && (bool) prop.value)    { set ("sttKey", {}); continue; }
            set (key, val.trim());
        }
}

juce::File AiSettings::presetsFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("CUPIDO").getChildFile ("Presets");
    f.createDirectory(); return f;
}

// =============================================================================
//  Prompt de sistema — o "cérebro" de engenheiro de mixagem do Cupidinho
// =============================================================================
juce::String AiClient::buildSystemPrompt (const juce::String& spec)
{
    return juce::String (juce::CharPointer_UTF8 (R"PROMPT(Você é o CUPIDINHO, o agente de IA do plugin vocal CUPIDO — um engenheiro de mixagem e masterização brasileiro, experiente em trap, plug, drill, funk, R&B e pop. Você fala português do Brasil de forma descontraída (pode usar gírias tipo "bah", "tchê", "mano", "zica", "pica", "bala") mas é tecnicamente preciso.

O usuário descreve o som que quer (às vezes por voz, com erros de transcrição — interprete com bom senso). Você responde SEMPRE e SOMENTE com um objeto JSON válido, sem texto fora dele, neste formato:
{
  "reply": "resposta curta (1-3 frases) explicando o que mudou e por quê",
  "preset": "nome exato de um preset de fábrica para carregar ANTES das mudanças (opcional)",
  "changes": { "id_do_parametro": valor, ... },
  "actions": ["analyze_beat" | "analyze_voice" | "learn_noise" | "auto_mix" | "diagnose" | "render" | "reference" | "reset_lufs"]  (opcional)
}

REGRAS
- Use apenas ids da lista de parâmetros. Valores em unidades reais, dentro de min..max. Escolhas (choice) usam o ÍNDICE (0,1,2...). Liga/desliga (bool) usa 0 ou 1.
- Ao mexer num módulo desligado, ligue-o também (ex.: mexeu no delay → "dly_on": 1).
- Mudanças relativas ("mais", "menos", "um pouco", "bem mais") partem do valor ATUAL que vem no contexto. "Um pouco" ≈ 10–20% do range útil; "bem/muito" ≈ 35–50%.
- Seja musical: mudanças coerentes entre si (ex.: mais brilho → considere o de-esser; mais compressão → ajuste o makeup).
- Se pedirem estilo de um artista, faça uma aproximação de CARACTERÍSTICAS sonoras (afinação, brilho, espaço, delay), sem prometer que vai soar idêntico. Se existir preset parecido, use "preset" e refine com "changes".
- Se o pedido for ambíguo, aplique a interpretação mais provável e diga isso no reply.
- "Mixa pra mim / deixa perfeito / identifica o beat" → actions ["analyze_beat","analyze_voice","auto_mix"].
- "Analisa minha mix / tá embolando? / tá bom?" → actions ["diagnose"] (o plugin mede voz x beat e mostra nota 0–100).
- "Gera/exporta a mix pra eu ouvir" → actions ["render"]. "Copia o som da referência" → actions ["reference"].
- Se o pedido for estilo de um artista que está em "artistas_conhecidos", use "preset" com o nome EXATO dele e refine com "changes".
  Se NÃO estiver, monte você mesmo a receita em "changes" a partir do que você sabe do som desse artista (afinação, brilho, saturação,
  espaço, tipo de delay/reverb, largura) partindo do estilo mais próximo — e diga no reply que é uma aproximação.
- O contexto pode trazer "anexos" (beat, voice, reference) com BPM, tom, LUFS, crest, espectro em 32 bandas (20 Hz–20 kHz, log) e
  "estilo_provavel". Pode vir também uma IMAGEM com os espectrogramas. USE ISSO: ex. beat com 808 forte → passa-altas da voz 110–120 Hz;
  beat cheio em 2–5 kHz → presença da voz mais alta (4–5 kHz) + ar; voz com lama → corte 250–400 Hz; afine no tom do beat.
- Nunca invente parâmetros. Nunca devolva markdown.

CONHECIMENTO DE MIXAGEM (use como base)
- Cadeia: LIMPA (gate+redução de ruído) → AFINA (autotune) → EQ → SIBILA (de-esser) → PUNCH (compressor FET/VCA) → OPTO (LA-2A) → PRISMA (saturação multibanda paralela, cada banda gera harmônicos) → AR (excitador dinâmico Mid/High Air) → DOBRA (doubler estéreo) → ECO (delay) → ESPAÇO (reverb) → MASTER (glue, tilt, largura, limiter).
- Autotune: velocidade 0–5 ms = robótico/hard-tune (trap); 10–25 ms = melódico moderno; 30–60 ms = natural. Sempre no tom do beat (tune_key + tune_scale). Humanizar 0 para trap; 20–40 para R&B.
- Trap: compressão forte (ratio 6–10, 6–10 dB de redução), FET rápido (ataque 1–5 ms), de-esser 5–8 kHz, presença 2.5–5 kHz, ar 10–12 kHz, passa-altas 90–120 Hz.
- Delay: 1/4 ou 1/8 pontuado sincronizado, repetições 20–35%, corte grave 300–400 Hz no retorno, ducking para não embolar. "Delay mais demorado" = nota mais longa (1/8 → 1/4 → 1/4. → 1/2) e/ou mais repetições.
- Reverb: plate/hall, pré-delay 20–40 ms (mantém a voz na frente), decay 1.2–2.5 s, mix baixo; "molhado" = mais mix/decay; "seco" = menos.
- Vocal "aberto/arejado": AR (air_high/air_mid), eq_high_gain +, pouca lama em 250–400 Hz, doubler leve, largura.
- "Saturação nos agudos" = PRISMA bandas 4/5 (sat_b4_amt, sat_b5_amt) + sat_on 1; nos graves = bandas 1/2; "quente" = tipo Válvula/Fita.
- "Embolado/abafado" = cortar eq_m1 ~250–400 Hz; "nasal/fanho" = cortar ~800–1200 Hz; "ardido/estridente" = cortar 2.5–4 kHz e/ou mais de-esser; "fino" = mais grave em 150–250 Hz.
- Master: limiter teto -1 dBTP; streaming ≈ -14 LUFS; trap/funk ≈ -9 a -7 LUFS (lim_gain). Glue 2:1 com 1–3 dB de redução.

RECEITAS POR ESTILO (pesquisa: guias de engenheiros e manuais)
- Trap: HPF 110–130 Hz; lama −3 a −5 dB em 200–400 Hz; nasal −2 dB 0,8–1,2 kHz; compressão 6–10:1 com 6–10 dB de redução, ataque 3–8 ms, release 40–80 ms;
  de-esser 5–7 kHz antes do autotune; presença +2–3 dB 2–5 kHz; ar +2–4 dB 10–16 kHz; saturação paralela 40–70%; plate curto 0,6–1,2 s, pré-delay 15–30 ms,
  HP no retorno 300–500 Hz; delay 1/4 ou 1/8 (pontuado), 20–30% feedback, retorno HP 400 Hz / LP 8 kHz; ad-libs 6–10 dB abaixo, retune 5–8 ms, reverb 1,2–2 s.
- R&B: HPF 60–80 Hz; corte 200–350 Hz; calor +1–2 dB 150–250 Hz; compressão 2–3:1, ataque 15–30 ms, release 80–150 ms, 3–5 dB; de-esser 3–5 dB;
  saturação de fita 10–20%; plate/hall 1,8–3 s, pré-delay 30–60 ms, damping acima de 6–8 kHz; delay 1/8 com high-cut 4–6 kHz; dobras 30–50% L/R.
- Plug: retune médio; HPF 70–110 Hz; compressão 3–4:1 média; ar 10–12 kHz depois do de-esser; delay 1/4 ou 1/8 pontuado filtrado; reverb largo com ducking.
- Drill (UK/NY): HPF ~90 Hz; compressão em 2 estágios (2:1 lento + 1–2 dB rápido); saturação 5–10%; slap mono 80–120 ms filtrado 150 Hz–6 kHz;
  plate 0,6–1 s com pré-delay 20–50 ms só no hook; efeito telefone 300 Hz–3 kHz em palavras pontuais.
- Funk BR: voz estourada e na frente: VCA forte, presença 2,5–3 kHz +4 dB, saturação de transistor, reverb mínimo, master −7 a −5 LUFS.
- Pop: compressão em série (VCA + opto), presença +2 dB, ar 10–16 kHz, autotune transparente 20–30 ms, dobras e plate médio 1,5 s.
- Reggae/dub: voz quente (topo levemente escuro), FET médio, saturação de fita, eco de fita escuro (repetições altas, retorno 300 Hz–4 kHz),
  reverb de mola/câmara esparso, throws no fim das frases.
- Masterização: streaming normaliza (Spotify −14, YouTube ~−13, Apple −16 LUFS); true peak −1 dBTP; trap/funk comerciais costumam ir a −9…−6 LUFS;
  glue 2:1–4:1 com ≤3 dB de redução; grave mono abaixo de ~150 Hz; alargar só acima de ~2 kHz.

REFERÊNCIAS DE ESTILO (aproximações de características, não cópias)
- Trap melódico cearense / 30PRAUM (ex.: Matuê): autotune melódico rápido no tom (10–20 ms), voz brilhante e na cara, delay 1/4 pontuado com ducking, plate médio, doubler sutil.
- Teto: autotune mais duro (0–8 ms), timbre agudo e saturado, bem comprimido, slap + 1/8, dobras abertas nas pontas.
- Yunk Vino / trap melódico do Recife: atmosfera mais escura e longa, delay 1/2 ou 1/4 com mais repetições, hall grande, saturação de fita, autotune melódico.
- Plug/pluggnb: autotune suave, muito reverb e delay espaçoso, brilho sedoso.
- Rage: saturação/fuzz forte, hard-tune, doubler largo, bem alto.

PARÂMETROS DISPONÍVEIS (id | rótulo | módulo | min..max | unidade | escolhas)
)PROMPT")) + spec;
}

// =============================================================================
//  HTTP helpers
// =============================================================================
static AiResult postJson (const juce::String& url, const juce::String& headers, const juce::String& body, int timeoutMs)
{
    AiResult r; int status = 0; juce::StringPairArray responseHeaders;
    juce::URL u = juce::URL (url).withPOSTData (body);
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders (headers).withConnectionTimeoutMs (timeoutMs)
                    .withStatusCode (&status).withResponseHeaders (&responseHeaders).withHttpRequestCmd ("POST");
    std::unique_ptr<juce::InputStream> in (u.createInputStream (opts));
    if (in == nullptr) { r.error = "Sem conexão com " + juce::URL (url).getDomain() + " (verifique a internet/firewall)."; return r; }
    const auto text = in->readEntireStreamAsString();
    if (status < 200 || status >= 300)
    {
        auto parsed = juce::JSON::parse (text);
        juce::String msg = parsed.getProperty ("error", {}).getProperty ("message", {}).toString();
        if (msg.isEmpty()) msg = text.substring (0, 300);
        r.error = "HTTP " + juce::String (status) + ": " + msg; return r;
    }
    r.ok = true; r.text = text; return r;
}

static juce::String stripFences (juce::String s)
{
    s = s.trim();
    if (s.startsWith ("```")) { s = s.fromFirstOccurrenceOf ("\n", false, false); s = s.upToLastOccurrenceOf ("```", false, false); }
    const int a = s.indexOfChar ('{'), b = s.lastIndexOfChar ('}');
    return (a >= 0 && b > a) ? s.substring (a, b + 1) : s;
}

// =============================================================================
//  Claude
// =============================================================================
AiResult AiClient::askClaude (const AiSettings& s, const juce::String& userText, const juce::String& contextJson, const juce::String& spec, const juce::String& png)
{
    const auto key = s.get ("claudeKey");
    if (key.isEmpty()) { AiResult r; r.error = "Nenhuma chave da API do Claude configurada (⚙ Configurações)."; return r; }

    auto* msg = new juce::DynamicObject(); msg->setProperty ("role", "user");
    const juce::String text = "ESTADO ATUAL DO PLUGIN (JSON):\n" + contextJson + "\n\nPEDIDO DO USUÁRIO:\n" + userText;
    if (png.isNotEmpty())
    {   // o Claude "vê" o som: espectrograma do beat/voz/referência anexados
        auto* src = new juce::DynamicObject(); src->setProperty ("type", "base64"); src->setProperty ("media_type", "image/png"); src->setProperty ("data", png);
        auto* img = new juce::DynamicObject(); img->setProperty ("type", "image"); img->setProperty ("source", juce::var (src));
        auto* txt = new juce::DynamicObject(); txt->setProperty ("type", "text");
        txt->setProperty ("text", "A imagem mostra espectrogramas dos áudios anexados (beat, voz, referência). Use-a junto com os números da análise.\n\n" + text);
        juce::Array<juce::var> parts; parts.add (juce::var (img)); parts.add (juce::var (txt));
        msg->setProperty ("content", parts);
    }
    else msg->setProperty ("content", text);
    juce::Array<juce::var> msgs; msgs.add (juce::var (msg));

    auto* body = new juce::DynamicObject();
    body->setProperty ("model", s.get ("claudeModel", "claude-haiku-5-5"));
    body->setProperty ("max_tokens", 2500);
    body->setProperty ("system", buildSystemPrompt (spec));
    body->setProperty ("messages", msgs);

    const juce::String headers = "Content-Type: application/json\r\n"
                                 "x-api-key: " + key + "\r\n"
                                 "anthropic-version: 2023-06-01\r\n";
    auto r = postJson ("https://api.anthropic.com/v1/messages", headers, juce::JSON::toString (juce::var (body), true), 45000);
    if (! r.ok) return r;

    auto resp = juce::JSON::parse (r.text);
    juce::String out;
    if (auto* arr = resp.getProperty ("content", {}).getArray())
        for (auto& block : *arr) if (block.getProperty ("type", {}).toString() == "text") out += block.getProperty ("text", {}).toString();
    auto parsed = juce::JSON::parse (stripFences (out));
    AiResult res;
    if (! parsed.isObject()) { res.error = "A IA respondeu fora do formato esperado."; res.text = out; return res; }
    res.ok = true; res.json = parsed; res.text = parsed.getProperty ("reply", {}).toString(); return res;
}

AiResult AiClient::testClaude (const AiSettings& s)
{
    return askClaude (s, "Teste de conexão: responda com reply curto dizendo que está tudo certo e changes vazio.", "{}", "(teste)");
}

// =============================================================================
//  Transcrição (multipart/form-data, OpenAI-compatível)
// =============================================================================
AiResult AiClient::transcribe (const AiSettings& s, const juce::MemoryBlock& wav)
{
    AiResult r;
    const auto url = s.get ("sttUrl", "https://api.groq.com/openai/v1/audio/transcriptions");
    const auto key = s.get ("sttKey");
    const bool local = url.contains ("127.0.0.1") || url.contains ("localhost");
    if (key.isEmpty() && ! local) { r.error = "Configure a chave de transcrição de voz (⚙ Configurações → Voz)."; return r; }

    juce::URL u = juce::URL (url)
                    .withParameter ("model", s.get ("sttModel", "whisper-large-v3-turbo"))
                    .withParameter ("language", s.get ("voiceLanguage", "pt"))
                    .withParameter ("response_format", "json")
                    .withParameter ("prompt", "Comandos de mixagem vocal: autotune, delay, reverb, agudos, graves, saturação, de-esser, compressor, Matuê, Teto, Yunk Vino, trap, mixa, masteriza.")
                    .withDataToUpload ("file", "comando.wav", wav, "audio/wav");
    int status = 0;
    juce::String headers = key.isNotEmpty() ? "Authorization: Bearer " + key + "\r\n" : juce::String();
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders (headers).withConnectionTimeoutMs (30000).withStatusCode (&status);
    std::unique_ptr<juce::InputStream> in (u.createInputStream (opts));
    if (in == nullptr) { r.error = "Sem conexão com o serviço de voz."; return r; }
    const auto text = in->readEntireStreamAsString();
    if (status < 200 || status >= 300) { r.error = "Voz HTTP " + juce::String (status) + ": " + text.substring (0, 200); return r; }
    auto parsed = juce::JSON::parse (text);
    r.text = parsed.isObject() ? parsed.getProperty ("text", {}).toString().trim() : text.trim();
    r.ok = r.text.isNotEmpty(); if (! r.ok) r.error = "Não entendi o áudio (tenta falar mais perto do mic).";
    return r;
}
} // namespace cupido
