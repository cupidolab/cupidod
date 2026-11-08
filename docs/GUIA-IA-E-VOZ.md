# Guia: ligando a IA e os comandos de voz do Cupidinho

O Cupidinho tem três "cérebros". Tu pode usar só o primeiro (grátis, offline) ou ligar os outros.

| Camada | Precisa de quê | O que ganha |
|---|---|---|
| **Modo local** | nada — já vem ligado | Entende centenas de jeitos de pedir em português/gíria, presets por estilo, auto-mix com análise do beat |
| **Claude (Anthropic)** | chave da API do Claude | Entende qualquer frase, combina pedidos complexos, explica o que fez, raciocina sobre o estado inteiro da mix |
| **Voz** | chave do Groq (grátis) ou OpenAI, ou servidor local | Segura o microfone e fala em vez de digitar |

---

## 1. Ligando o Claude

1. Entra em **console.anthropic.com**, cria a conta e adiciona créditos (a cobrança é por uso; o modelo Haiku é o mais barato — confira os preços atuais na página de preços da Anthropic).
2. Vai em **Settings → API Keys → Create Key** e copia a chave (começa com `sk-ant-`).
3. No CUPIDO, clica na **engrenagem ⚙** no topo.
4. Em **Inteligência**:
   - Modo do agente: **Automático**
   - Cola a chave no campo **Chave da API do Claude**
   - Modelo: **Claude Haiku 5.5** (rápido e barato) — ou Sonnet 5.5 / Opus 5.5 se quiser mais capricho
5. Clica **Testar Claude**. Se aparecer "Claude conectado", clica **Salvar**.

O selo no canto do Cupidinho muda de `local` para o nome do modelo. Pronto.

**Onde a chave fica salva:** num arquivo de configurações só do teu computador
(Windows: `%APPDATA%\CupidoAudio\CUPIDO.settings`; macOS: `~/Library/Application Support/CupidoAudio/`).
Ela **nunca** vai para o projeto do DAW, então tu pode mandar a sessão pra outra pessoa sem vazar a chave.

**Se a internet cair ou a chave falhar**, o Cupidinho avisa e responde no modo local automaticamente.

### Como a conversa com o Claude funciona (por dentro)

A cada pedido o plugin manda para `https://api.anthropic.com/v1/messages`:
- um **prompt de sistema** (em `source/AiClient.cpp`, função `buildSystemPrompt`) com conhecimento de mixagem, as referências de estilo e a lista dos 115 parâmetros com faixas e opções;
- o **estado atual** (todos os valores, presets disponíveis, análise do beat/voz, BPM);
- o **teu pedido**.

O Claude responde só um JSON, por exemplo:
```json
{
  "reply": "Bah, abri a voz: mais High Air, tirei lama em 300 Hz e o delay foi pra 1/4.",
  "changes": { "air_high": 45, "eq_m1_gain": -3, "dly_on": 1, "dly_note": 8 },
  "actions": ["analyze_beat"]
}
```
A interface valida (limita cada valor ao mínimo/máximo), aplica, pinta em dourado e mostra "desfazer".

---

## 2. Ligando os comandos de voz

### Opção A — Groq (recomendado: grátis e muito rápido)
1. Entra em **console.groq.com/keys** e cria uma chave (começa com `gsk_`).
2. No CUPIDO: ⚙ → **Voz** → Transcrição: **Groq Whisper**.
3. Cola a chave em **Chave da transcrição** → **Salvar**.

### Opção B — OpenAI
⚙ → Voz → Transcrição: **OpenAI** → cola tua chave `sk-...` → Salvar.

### Opção C — 100% offline (servidor no teu PC)
Instala o [Docker](https://www.docker.com) e roda:
```bash
docker run -p 8000:8000 ghcr.io/speaches-ai/speaches:latest-cpu
```
⚙ → Voz → Transcrição: **Servidor local** (endereço `http://127.0.0.1:8000/v1/audio/transcriptions`). Não precisa de chave.
Qualquer servidor compatível com a API de transcrição da OpenAI funciona — é só colocar o endereço.

### Usando
- **Segura o botão do microfone** (ou a tecla **V**) e fala. Solta → transcreve → o Cupidinho executa.
- **Microfone do sistema** (padrão) abre teu mic por alguns segundos e fecha.
- **Entrada do próprio canal** usa o áudio que já chega no plugin — útil quando a faixa está armada para gravar.
- Dica: fala perto do mic e sem o beat tocando alto.

---

## 3. Fazendo o Cupidinho "ouvir" o beat

1. Manda o beat pro **sidechain** do CUPIDO (veja o README para cada DAW).
2. Dá play uns 10 segundos.
3. Clica **Ouvir o beat** ou diz "identifica o beat".

Ele mede **BPM** (fluxo espectral + autocorrelação), **tom** (cromagrama + perfis de Krumhansl), **balanço espectral** e **dinâmica**, e oferece "Afinar nesse tom" e "Mixar com base nisso".

---

## 4. Personalizando o agente

| Quero… | Onde mexer |
|---|---|
| Ensinar novas gírias / frases ao modo local | `ui/agent.js`, função `interpret` — cada bloco é uma regra (ex.: o bloco `// ---------- DELAY ----------`) |
| Mudar a personalidade ou o conhecimento do Claude | `source/AiClient.cpp`, `buildSystemPrompt` |
| Criar presets de fábrica novos | `ui/presets.js` (as `tags` são as palavras que ativam o preset por voz/texto) |
| Mudar a lógica do auto-mix | `ui/agent.js`, função `autoMix` |
| Adicionar um parâmetro novo | `source/Params.h` → depois rode `tools/gen_params` para atualizar `ui/params.js` |

Depois de mexer em arquivos de `ui/`, é só compilar de novo: a interface é embutida no plugin.

## 5. Problemas comuns

- **"Nenhuma chave da API do Claude configurada"** → cola a chave na ⚙ e salva.
- **"HTTP 401"** → chave errada ou revogada. **"HTTP 429"** → limite de uso; espera um pouco.
- **"Microfone indisponível"** (macOS) → Ajustes → Privacidade → Microfone → libera o teu DAW.
- **Voz não entende** → testa a opção "Entrada do próprio canal", ou aproxima o mic.
- **Ouvir o beat diz que não ouviu nada** → confere se o sidechain está realmente chegando (o espectro azul do beat aparece no EQ quando está).
