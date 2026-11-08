# Base de conhecimento do Cupidinho

Esse é o resumo da pesquisa que virou as receitas do plugin (`ui/knowledge.js`), as regras do agente local e o prompt do Claude (`source/AiClient.cpp`). São pontos de partida usados por engenheiros. O ouvido sempre manda.

## Ordem da cadeia vocal

Ganho de entrada → gate/redução de ruído → EQ subtrativo → compressão → de-esser → afinação → saturação → EQ aditivo/ar → reverb → delay → master.

Variação comum em trap: de-esser **antes** do autotune, para o detector de pitch não "pegar" o S.

## Faixas de frequência da voz

| Faixa | O que é | O que fazer |
|---|---|---|
| 20–80 Hz | ronco, vento, grave que embola com o 808 | passa-altas (voz masculina 100–120 Hz, feminina 120–150 Hz em beat de trap; R&B 60–80 Hz) |
| 80–300 Hz | corpo e fundamental | manter; +1–2 dB em 150–250 Hz para calor (R&B) |
| 200–500 Hz | lama / "embolado" / caixa | corte de 2–6 dB com Q 1,2–3 onde acumula |
| 800 Hz–1,5 kHz | nasal / fanho | varrer e atenuar 2–3 dB |
| 2–5 kHz | presença e inteligibilidade (e onde ficam os hi-hats) | +2–4 dB largo; se ardido, −1,5 a −2 dB em 3–5 kHz |
| 5–9 kHz | sibilância (S, T, CH) | de-esser 5–8 kHz (4–6 kHz em voz masculina com consoante dura) |
| 10–16 kHz | ar / brilho | shelf +2–4 dB, sempre depois do de-esser |

## Por estilo

**Trap** (130–160 BPM)
- **Ganho:** voz com média de −18 a −14 dBFS RMS.
- **Gate:** −50 a −40 dB.
- **EQ:** passa-altas em 110–130 Hz.
- **Compressão:** 6:1 a 10:1, ataque de 3–8 ms, release de 40–80 ms, 6–10 dB de redução, joelho duro.
- **Autotune:** 0 ms robótico; 5–15 ms melódico; 15–25 ms com glide; 30–50 ms sutil.
- **Saturação:** fita/válvula em paralelo, 40–70% de mix.
- **Reverb:** plate curto de 0,6–1,2 s, pré-delay de 15–30 ms, mix de 15–25%, passa-altas de 300–500 Hz no retorno.
- **Delay:** 1/4 ou 1/8 pontuado, 20–30% de feedback, retorno HP 400 Hz / LP 8 kHz. Ping-pong 1/8 pontuado é a marca do som estilo Travis Scott.
- **Ad-libs:** 6–10 dB abaixo do lead, panorama 15–30% L/R, passa-altas em 150–200 Hz, retune de 5–8 ms, reverb de 1,2–2 s.
- **Beat denso:** o 808 briga com a voz em 80–200 Hz, os acordes em 200–800 Hz e os hi-hats em 2–6 kHz. Compressão em dois estágios (Opto + FET). Compressão paralela a 10:1 misturada 12–20 dB abaixo.

**R&B** (60–100 BPM)
- **EQ:** passa-altas em 60–80 Hz; corte de 2–3 dB em 200–350 Hz; calor +1–2 dB em 150–250 Hz; presença +1–3 dB em 3–5 kHz; ar +2–4 dB em 12–16 kHz.
- **Compressão:** 2:1 a 3:1, ataque de 15–30 ms, release de 80–150 ms, no máximo 5 dB de redução.
- **De-esser:** 5–8 kHz, 3–5 dB.
- **Saturação:** fita, 10–20%.
- **Reverb:** plate ou hall de 1,8–3 s, pré-delay de 30–60 ms, damping acima de 6–8 kHz, envio de 20–35%.
- **Delay:** 1/8 na esquerda e 1/8 pontuado na direita, 20–35% de feedback, high-cut de 4–6 kHz.
- **Dobras e harmonias:** dobras abertas 30–50% L/R; harmonias 70–100% L/R com reverb de 2,5–3,5 s.

**Plug / pluggnb**
- **Afinação:** retune médio.
- **EQ:** passa-altas em 70–90 Hz (voz grave) ou 90–110 Hz (voz aguda); lama cortada só onde acumula; ar em 10–12 kHz depois do de-esser.
- **Compressão:** 3:1 a 4:1, ataque e release médios.
- **Saturação:** suave, de fita.
- **Delay:** 1/4 ou 1/8 pontuado, filtrado, com throws no fim das frases.
- **Reverb:** plate/hall largo com ducking.

**Drill** (UK/NY, cerca de 140 BPM)
- **EQ:** passa-altas em 90 Hz; −1 a −2 dB em 250 Hz; notch em 1 kHz se a voz for nasal.
- **Compressão:** primeiro estágio a 2:1 (ataque 20 ms, release 120 ms, 3–5 dB); depois um rápido de 1–2 dB.
- **Saturação:** 5–10%.
- **Delay:** slap mono de 80–120 ms filtrado entre 150 Hz e 6 kHz.
- **Reverb:** plate de 0,6–1 s com pré-delay de 20–50 ms, mínimo nos versos.
- **Efeito telefone:** passa-banda 300 Hz–3 kHz em palavras pontuais.
- **Integração com o beat:** dip dinâmico de 2–4 kHz no beat acionado pela voz.

**Funk BR** (130 / 150 BPM)
- **Voz:** do MC na cara, compressão VCA pesada.
- **EQ:** presença forte em 2,5–3 kHz.
- **Saturação:** de transistor nos médios.
- **Reverb:** curtinho ou nenhum.
- **Master:** alto, comumente entre −7 e −5 LUFS.

**Pop**
- **Compressão:** em série, VCA seguida de opto.
- **EQ:** presença +2 dB; ar em 10–16 kHz.
- **Autotune:** transparente, 20–30 ms.
- **Espaço:** dobras e plate médio de cerca de 1,5 s; delays curtos com throws.

**Reggae / dub**
- **Voz:** quente, com o topo levemente escuro.
- **Compressão:** FET média.
- **Saturação:** de fita.
- **Delay:** eco de fita escuro, com muitas repetições e retorno entre 300 Hz e 4 kHz.
- **Reverb:** de mola ou câmara, esparso.
- **Envios:** cerca de 20% para o delay e 50% para o reverb em voz e metais.
- **Throws:** o reverb entra no primeiro tempo da caixa e o delay no segundo.

## Compressão em série (1176 → LA-2A)

O FET (1176) entra primeiro e segura só os picos, com ataque rápido e 3–5 dB de redução. O óptico (LA-2A) vem depois e nivela de forma suave. No CUPIDO, isso é o **PUNCH** seguido do **OPTO**.

## Masterização

- **Antes de masterizar:** a mix com picos entre −6 e −3 dBFS; WAV de 24 bits a 44,1 ou 48 kHz.
- **Limpeza:** passa-altas em cerca de 30 Hz, com EQ linear e EQ dinâmico para picos.
- **Glue:** ataque lento, release rápido ou automático, 2:1 a 4:1, no máximo 3 dB de redução.
- **Imagem:** grave mono abaixo de cerca de 180 Hz; alargar só acima de cerca de 2 kHz.
- **Limiter:** true peak entre −1 e −0,2 dB; dither uma única vez, no fim.
- **Loudness:** streaming normaliza (Spotify −14, YouTube cerca de −12,5, Apple −16, Tidal −14,5 LUFS). Pop pode chegar a −8 LUFS; trap e funk comerciais costumam ficar entre −9 e −6 LUFS.
- **Teste final:** usar referências e ouvir em vários sistemas (fone, celular, carro).

## Reverb no estilo do Fruity Reeverb 2

O reverb do CUPIDO segue a mesma lógica de parâmetros do Fruity Reeverb 2. A tabela mostra a correspondência:

| Fruity Reeverb 2 | CUPIDO (ESPAÇO) |
|---|---|
| H.CUT | Corte Agudo |
| L.CUT | Corte Grave |
| DELAY | Pré-delay |
| SIZE | Tamanho |
| DIFF | Difusão |
| MOD | Modulação |
| BASS | Grave x Decay |
| DEC | Decay |
| DAMP | Amortecer |
| ER | Primeiras Reflexões |
| SEP | Largura |

O visual é um cilindro 3D:
- **Altura:** decay.
- **Largura:** tamanho.
- **Pontos:** difusão.
- **Brilho das paredes:** inverso do amortecimento.

## Fontes consultadas

**Em português**
- [Alataj — Guia de masterização (Marcelo Oriano)](https://alataj.com.br/papo-de-estudio/marcelo-oriano-guia-de-masterizacao-impecavel)
- [protools.com.br — Guia de mixagem e masterização](https://protools.com.br/conteudo/mixagem-masterizacao-profissional-guia-completo)
- [iGroove — 7 plugins para vocais](https://www.igroove.com.br/post/7-plugins-essenciais-para-vocais-bem-mixados)
- [eMastered — Como equalizar vocais](https://emastered.com/pt/blog/how-to-eq-vocals)
- [Brabo na Produção — Masterizar no FL Studio](https://brabonaproducao.com/como-masterizar-no-fl-studio-guia-completo/)
- [Brabo na Produção — Autotune no FL](https://brabonaproducao.com/como-usar-autotune-no-fl-studio-guia-completo/)
- [bchillmix — Mixagem estilo UK drill](https://bchillmix.com/pt/blogs/news/how-to-mix-vocals-like-central-cee-uk-drill-clarity-pace)
- [Ditto — Mixar vocais em 8 passos](https://dittomusic.com/pt/blog/how-to-mix-vocals-professionally-in-8-steps)

**Em inglês (engenheiros e fabricantes)**
- [RysUp — How to mix trap vocals](https://rysupaudio.com/blogs/news/how-to-mix-trap-vocals)
- [RysUp — How to mix R&B vocals](https://rysupaudio.com/blogs/news/how-to-mix-rnb-vocals)
- [Antares — Vocals in dense trap beats](https://www.antarestech.com/blog/how-to-make-your-vocals-sit-in-dense-hip-hop-trap-beats)
- [Antares — AutoTune for rap & trap](https://www.antarestech.com/blog/auto-tune-for-rap-hip-hop-how-trap-producers-use-pitch-correction)
- [bchillmix — Pluggnb vocal chain](https://bchillmix.com/blogs/news/pluggnb-vocal-chain-settings-for-home-studio-sessions)
- [Soundbridge — Dub reggae vibe](https://www.soundbridge.io/crafting-a-dub-reggae-vibe)
- [Joey Sturgis Tones — Pop vocals](https://joeysturgistones.com/blogs/learn/a-step-by-step-guide-for-mixing-pop-vocals)
- [Sage Audio — Loudness for mastering](https://www.sageaudio.com/articles/what-is-loudness-for-mastering)
- [Image-Line — Fruity Reeverb 2](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/Fruity%20Reeverb%202.htm)
- [Slate Digital — Fresh Air](https://docs.slatedigital.com/FreshAir/Fresh%20Air.html)
- [Wavesfactory — Spectre](https://wavesfactory.com/spectre)
- [Nail The Mix — 1176 into LA-2A](https://www.nailthemix.com/the-1176-into-la-2a-trick-how-to-stack-vocal-compressors)

## Sobre os presets de artistas

Os 130 presets de artista descrevem **características sonoras públicas**: quão duro é o autotune, se a voz é brilhante ou escura, se tem saturação, como são o delay e o reverb, a largura das dobras e a pressão da compressão. Partem da receita do estilo e somam ajustes. Isso dá a vibe, mas a voz de cada pessoa é única. Pra chegar mais perto, anexe uma música do artista como **Referência**: o plugin copia o equilíbrio, o brilho e o volume dela.
