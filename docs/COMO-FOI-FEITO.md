# Como o CUPIDO foi feito

## 1. Pesquisa

Antes de escrever código, estudei como funcionam os plugins de referência e como engenheiros mixam voz de trap:

- **Fresh Air (Slate):** não é um shelf comum — são duas bandas passa-altas sobrepostas (Mid Air ≈ presença, High Air ≈ topo), comprimidas suavemente e somadas em paralelo, o velho "truque do Dolby-A". Por isso realça o detalhe baixinho sem deixar ardido. O **AR** do CUPIDO segue essa ideia: passa-altas em ~2.8 kHz e ~9.5 kHz, compressor com limiar em -40 dB e razão 4:1 no ramo paralelo, mais um toque harmônico no topo, e Trim de saída.
- **Spectre (Wavesfactory):** cinco bandas paralelas que funcionam como um EQ de boost, mas o que é somado é a *diferença do boost saturada* — "boostar 10 kHz" vira "adicionar harmônicos em 10 kHz". O **PRISMA** faz exatamente isso: extrai a banda, multiplica pelo ganho do boost, satura só essa diferença e soma; mix de 50% equivale ao ganho nominal.
- **Cadeia vocal de trap:** retune 0–5 ms para hard-tune e 10–25 ms para melódico; compressão de 6–10 dB com FET rápido; de-esser em 5–8 kHz; plate/hall com pré-delay de 20–40 ms; delay 1/4 ou 1/8 pontuado com 20–35% de repetições e passa-altas de 300–400 Hz no retorno. Esses números viraram os padrões dos módulos, as regras do agente e os presets.
- **Tecnologia:** JUCE 8.0.12 (versão estável atual) com interface em WebView — a interface é HTML/CSS/JS e conversa com o C++ por funções nativas.

## 2. Arquitetura

```
         ┌──────────────────────── DAW ────────────────────────┐
 voz  ──►│  CUPIDO (VST3/AU/AAX/LV2)                            │──► saída
 beat ──►│  sidechain                                           │
         │  ┌─────────────── C++ (tempo real) ───────────────┐  │
         │  │ LIMPA → AFINA → EQ → SIBILA → PUNCH → OPTO →     │  │
         │  │ PRISMA → AR → DOBRA → ECO → ESPAÇO → MASTER      │  │
         │  │ medidores · LUFS · captura p/ análise            │  │
         │  └───────────────▲──────────────────┬─────────────┘  │
         │   funções nativas │                  │ eventos 30 fps │
         │  ┌────────────────┴──────────────────▼─────────────┐  │
         │  │ Interface (WebView): módulos, gráficos, chat     │  │
         │  │ Cupidinho local (agent.js) · presets · voz       │  │
         │  └───────────────┬──────────────────────────────────┘  │
         └──────────────────┼────────────────────────────────────┘
                            ▼  (opcional, internet)
              API do Claude  ·  Whisper (Groq / OpenAI / local)
```

Decisões importantes:

- **Uma tabela de parâmetros só** (`source/Params.h`, X-macro). O processador, o motor DSP, a interface e o prompt do Claude leem dela — impossível ficarem dessincronizados.
- **Motor DSP em C++ puro, sem JUCE.** Assim dá para compilar e testar o som com um `g++` simples, e o mesmo código roda no plugin.
- **Nada aloca memória na thread de áudio.** Buffers são criados no `prepareToPlay`; trocar o modo de latência re-prepara fora da thread de áudio.
- **Latência constante e informada ao DAW** (38 ms no modo HQ, 7 ms no modo Gravação), para o DAW compensar e a voz não "atrasar" em relação ao beat. O Mix Geral alinha o sinal seco pela mesma latência.
- **Chaves de API fora da sessão** do DAW (arquivo de configurações do usuário).
- **IA nunca é obrigatória:** o modo local cobre o uso do dia a dia, e o Claude é uma camada a mais.

## 3. Algoritmos de cada módulo

- **Gate:** detector de pico com histerese de 4 dB e hold de 30 ms.
- **Redução de ruído:** STFT de 1024 pontos com 75% de sobreposição (janela Hann; perfeita reconstrução). O ruído é estimado por perfil aprendido (2 s) e/ou rastreamento de mínimo (desce rápido, sobe ~3 dB/s). Ganho tipo Wiener com piso ajustável (até -30 dB), suavizado no tempo (abre rápido, fecha devagar) e na frequência — isso evita o "ruído musical".
- **Autotune:** detecção de pitch por **YIN** (de Cheveigné & Kawahara) em sinal decimado, interpolação parabólica e confiança; nota-alvo na escala com histerese para não "tremer" entre notas; "humanizar" preserva parte do vibrato comparando com um contorno lento; correção suavizada pela velocidade; pitch-shift granular com duas cabeças defasadas e crossfade sin²+cos²=1.
- **EQ:** biquads do *Audio EQ Cookbook* (RBJ); passa-altas Butterworth de 4ª ordem.
- **De-esser:** sidechain passa-banda; modo split implementado como EQ dinâmico (high-shelf com atenuação igual à redução) — transparente quando não há S.
- **Compressores:** curva com joelho suave; FET com release dependente do programa e leve coloração; VCA com detector RMS; Opto com ataque de 10 ms e release de dois estágios com "memória".
- **Prisma / Ar:** descritos na pesquisa acima.
- **Doubler:** cada voz é um pitch-shift fino (± cents), atraso com variação aleatória suave e pan de potência constante.
- **Delay:** linha com interpolação Hermite, tempo suavizado, filtros e saturação de fita no laço de realimentação, wow/flutter, ducking pelo envelope da voz.
- **Reverb:** FDN de 8 linhas com matriz Hadamard, difusão por 4 all-pass, amortecimento por linha calibrado pelo T60, modulação das linhas, pré-delay e ducking.
- **Master:** glue RMS, tilt, M/S com grave mono, limiter com lookahead (filtro de mínimo + média móvel, que garante o teto) e medidor LUFS conforme ITU-R BS.1770 (filtro K, blocos de 400 ms, gate absoluto e relativo).
- **Ouvir o beat:** BPM por fluxo espectral + autocorrelação com prior perceptual; tom por cromagrama + perfis de Krumhansl-Schmuckler (24 tonalidades); balanço espectral em 6 faixas.

## 4. O agente Cupidinho

- **Local (`ui/agent.js`):** normaliza o texto (minúsculas, sem acentos), divide em frases ("e", vírgula, "também"…), detecta direção ("mais", "tira", "desliga"), intensidade ("um pouco", "bem mais", "no talo") e alvo (delay, reverb, agudos, saturação, autotune, tom…). Cada alvo vira mudanças musicais coerentes — ex.: "vocal mais aberto" sobe High/Mid Air e o agudo, corta lama em 300 Hz e garante o de-esser.
- **Estilos:** presets com *tags* (ex.: "matue", "teto", "yunk vino"). As referências descrevem características sonoras (velocidade do autotune, brilho, tipo de delay e reverb) — é uma aproximação de vibe, não uma cópia da voz de ninguém.
- **Auto-mix:** usa a análise da voz (piso de ruído, crest factor, sibilância, balanço espectral) e do beat (BPM, tom, onde o beat ocupa o espectro, densidade) para ligar e calibrar módulos — ex.: se o beat ocupa 3 kHz, a presença da voz vai para 4.5 kHz.
- **Claude:** veja `docs/GUIA-IA-E-VOZ.md`.

## 5. Testes que rodei

`tools/test_dsp.cpp` (roda também no build automático):

| Teste | Resultado |
|---|---|
| Latência reportada = latência real (impulso) | 1841 amostras a 48 kHz, impulso na posição certa |
| Autotune: Lá desafinado +40 cents, Lá menor, velocidade 0 | 57.40 → **57.00** |
| Nota fora da escala (56.1) | vai para a nota da escala mais próxima (57.00) |
| Transpor +12 | 57 → 69.06 |
| Limiter com +12 dB de ganho, teto -1 dBFS | pico de saída **-1.00 dBFS** |
| Tudo ligado no máximo, ruído + voz, todos os tipos/modos | sem NaN/Inf; 9x mais rápido que o tempo real (~11% de um núcleo) |
| Redução de ruído com perfil aprendido | ruído de fundo -34 → **-57 dB**, voz preservada (-15.4 → -15.7 dB) |
| Beat sintético 140 BPM em Lá menor | **140.2 BPM**, **Lá menor** |
| De-esser em 7 kHz | 7 kHz -13.5 → -22.4 dB; 300 Hz intacto |

Além disso:
- o código do plugin (processador, editor, cliente de IA) foi **compilado contra os headers do JUCE 8.0.12** sem erros;
- a interface foi aberta num **Chrome headless** com o simulador, testando chat, presets, auto-mix, todos os módulos, configurações e tela estreita, sem erros no console;
- o agente local foi testado com 21 frases reais (incluindo as do pedido original).

**O que ainda não foi testado aqui:** o binário final dentro de um DAW. O ambiente onde desenvolvi não tem as bibliotecas gráficas/áudio do Linux para linkar o plugin completo; a primeira compilação completa acontece no GitHub Actions (Windows, macOS e Linux). Se algum passo do build falhar lá, o log mostra a linha exata para corrigir.

## 6. Próximos passos sugeridos

- Formantes no autotune (para transposições grandes soarem naturais).
- Oversampling 2x/4x opcional no Prisma e no limiter (true peak).
- Modo "gráfico" de afinação nota a nota.
- Assinatura de código (Apple Developer ID / certificado Windows) para instalar sem avisos.
