# CUPIDO — suíte vocal com IA

Plugin de voz completo (VST3 / AU / AAX / LV2 / Standalone) com um agente de IA, o **Cupidinho**, que entende o que tu fala — escrito ou por voz — e mexe na mixagem e na masterização por ti. E tudo continua regulável na mão, nos mínimos detalhes.

> "Bah, aumenta a saturação nos agudos e bota um delay mais demorado" → o Cupidinho liga o Prisma, sobe as bandas de 4.5 kHz e 10 kHz, troca o delay de 1/8 para 1/4 e mostra cada mudança em dourado, com botão de desfazer.

## O que tem dentro

| Módulo | O que faz | Inspirado em |
|---|---|---|
| **LIMPA** | Noise gate + redução de ruído espectral (aprende o perfil do ruído ou rastreia sozinho) | iZotope RX Voice De-noise, gates clássicos |
| **AFINA** | Autotune pitch-síncrono (sem voz-fantasma/oitava errada), tom/escala (8 escalas), velocidade, humanizar, intensidade, transposição ±12, latência 5–7 ms | Antares Auto-Tune, Waves Tune Real-Time |
| **EQ** | Passa-altas 24 dB/oct, shelf grave, 2 médios paramétricos, shelf agudo, passa-baixas — com pontos arrastáveis | FabFilter Pro-Q |
| **SIBILA** | De-esser com frequência ajustável, modo split (EQ dinâmico) ou wide, e "ouvir S" | FabFilter Pro-DS |
| **PUNCH** | Compressor com caráter Limpo / FET (1176) / VCA, joelho, makeup e paralelo | UREI 1176, dbx 160 |
| **OPTO** | Compressor óptico de release em dois estágios | Teletronix LA-2A |
| **PRISMA** | Saturação por bandas: cada banda gera harmônicos só naquela região, 6 tipos (válvula, fita, transistor, quente, digital, fuzz) | Wavesfactory Spectre |
| **AR** | Excitador dinâmico Mid Air / High Air (bandas passa-altas comprimidas em paralelo) | Slate Fresh Air |
| **DOBRA** | Doubler de 2 ou 4 vozes com detune real, atraso e variação de timing | Waves Doubler |
| **ECO** | Delay estéreo / ping-pong / mono / slap, sync com BPM, ducking, filtros e fita | Soundtoys EchoBoy, Valhalla Delay |
| **ESPAÇO** | Reverb FDN 8 linhas com visual 3D: sala, plate, hall, câmara, ambiente; difusão, primeiras reflexões, grave x decay, pré-delay, modulação, ducking; 16 salas prontas | Fruity Reeverb 2, Valhalla Room |
| **MASTER** | Glue (bus comp), tilt EQ, largura estéreo + grave mono, limiter com lookahead, LUFS (curto e integrado) | SSL Bus Comp, FabFilter Pro-L |

Mais:
- **Cupidinho:** agente em português com gírias.
- **Comandos de voz offline:** segura o microfone ou a tecla V. O reconhecimento roda no próprio PC e não precisa de chave.
- **Anexar beat, voz e referência:** o plugin descobre o estilo, o BPM e o tom.
- **Análise da mix:** dá nota de 0 a 100 e diz se a voz tá embolando, escondida, ardida ou com ruído.
- **Gerar mix:** cria a versão antes e depois pra tu ouvir.
- **Copiar referência:** funciona com qualquer artista.
- **130 presets de artista:** trap BR, funk, R&B, reggae, rap, trap gringo e pop.
- **13 receitas de estilo** e **16 salas de reverb**, com o reverb num cilindro 3D.
- **Modo Simples / Pro**, **ganho automático** (nada estoura), 6 **macros**, **A/B**, desfazer ilimitado.
- **Modo Ao vivo:** cerca de 6 ms de latência.

## Como conseguir o instalador (sem precisar programar)

O jeito mais fácil é deixar o GitHub compilar pra ti — de graça:

1. Cria uma conta em [github.com](https://github.com) e um repositório novo (pode ser privado).
2. Sobe **todo o conteúdo desta pasta** pro repositório (botão "Add file → Upload files", arrasta tudo, inclusive a pasta `.github`).
3. Vai na aba **Actions**. O build começa sozinho (leva uns 15–25 min na primeira vez).
4. Quando ficar verde, clica no build e baixa em **Artifacts**: `CUPIDO-Windows` (instalador .exe), `CUPIDO-macOS` (instalador .pkg) e `CUPIDO-Linux` (.tar.gz).
5. Para gerar um "Release" com download público: cria uma tag `v1.0.0` (aba Releases → "Draft a new release" → tag `v1.0.0`).

### Instalar
- **Windows:** roda `CUPIDO-1.0.0-Windows-Instalador.exe`. Ele coloca o VST3 em `C:\Program Files\Common Files\VST3` e instala o WebView2 se faltar.
- **macOS:** abre o `.pkg` e escolhe AU / VST3 / AAX / App. Sem assinatura da Apple, na primeira vez clique com o botão direito → Abrir (ou Ajustes → Privacidade e Segurança → "Abrir mesmo assim").
- **Linux:** descompacta e roda `./install.sh`.

## Compilar no próprio computador

Precisa de CMake 3.22+ e um compilador C++17 (Visual Studio 2022 no Windows, Xcode no Mac, GCC/Clang no Linux). O JUCE é baixado automaticamente.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

- **Windows:** o WebView2 SDK precisa estar instalado (veja o passo "WebView2 SDK" em `.github/workflows/build.yml`) ou passe `-DJUCE_WEBVIEW2_PACKAGE_LOCATION=<pasta>`.
- **Pro Tools (AAX):** baixe o AAX SDK da Avid (conta de desenvolvedor gratuita) e passe `-DAAX_SDK_PATH=<pasta>`. Para rodar no Pro Tools comum o plugin precisa ser assinado com PACE/iLok (programa de desenvolvedores da Avid).
- Resultado em `build/Cupido_artefacts/Release/`.
- Testes do motor de áudio: `build/cupido_dsp_tests` (ou `build/Release/...`).

## Usando no DAW

1. Coloca o CUPIDO como insert na faixa de voz.
2. **Para ele ouvir o beat:** manda a faixa do beat para o sidechain do CUPIDO.
   - Ableton: no CUPIDO, ative o sidechain e escolha a faixa do beat.
   - FL Studio: no Mixer, selecione o canal do beat, clique com o direito na seta de envio do canal da voz → "Sidechain to this track"; no plugin, configure a entrada lateral.
   - Logic: no topo do plugin, menu "Side Chain" → faixa do beat.
   - Reaper: canais 3/4 da faixa da voz recebem um envio do beat.
   - Cubase / Studio One: ative o sidechain no plugin e envie o beat para ele.
3. Dá play uns 10 segundos e pede ao Cupidinho: **"mixa pra mim"**.

## Base de conhecimento

Tudo que o Cupidinho sabe de mixagem e masterização por estilo, com as fontes, está em **[docs/BASE-DE-CONHECIMENTO.md](docs/BASE-DE-CONHECIMENTO.md)**.

## Inteligência artificial e voz

Funciona **sem internet** (modo local). Para deixar o Cupidinho bem mais esperto e ligar a voz, veja o guia passo a passo: **[docs/GUIA-IA-E-VOZ.md](docs/GUIA-IA-E-VOZ.md)**.

## Como foi feito

Arquitetura, algoritmos, decisões e testes: **[docs/COMO-FOI-FEITO.md](docs/COMO-FOI-FEITO.md)**.

## Estrutura

```
source/Params.h          tabela única com os 115 parâmetros
source/dsp/*.h           motor de áudio em C++ puro (testável sem JUCE)
source/Plugin*.cpp       plugin JUCE + ponte com a interface
source/AiClient.*        Claude (API Anthropic) + transcrição de voz
ui/                      interface (HTML/CSS/JS) embutida no plugin
tools/test_dsp.cpp       testes automáticos do motor
installer/               instaladores Windows / macOS / Linux
.github/workflows/       build automático
```

## Licença

AGPLv3 (por causa do JUCE). Para vender com código fechado, é preciso licença comercial do JUCE. Veja `LICENSE.txt`.
