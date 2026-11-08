#include "MediaLab.h"
#include "VoiceUtils.h"

namespace cupido
{
static constexpr double kMaxSeconds = 6.0 * 60.0;

MediaLab::MediaLab() { formats.registerBasicFormats(); }

juce::File MediaLab::mixesFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("CUPIDO").getChildFile ("Mixes");
    f.createDirectory(); return f;
}
juce::File MediaLab::mediaFolder() const { return mixesFolder(); }

juce::String MediaLab::decode (const juce::String& kind, juce::InputStream* in, const juce::String& name, const juce::String&)
{
    std::unique_ptr<juce::InputStream> owned (in);
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (std::move (owned)));
    if (reader == nullptr) return "Formato não suportado. Use WAV, MP3, FLAC, AIFF ou OGG.";
    const auto len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * kMaxSeconds));
    if (len < reader->sampleRate * 3) return "O áudio é curto demais (mínimo 3 segundos).";
    juce::AudioBuffer<float> buf ((int) std::min (2u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, buf.getNumChannels() > 1);

    auto clip = std::make_shared<AudioClip>();
    clip->kind = kind; clip->name = name; clip->sampleRate = reader->sampleRate;
    clip->L.assign (buf.getReadPointer (0), buf.getReadPointer (0) + len);
    if (buf.getNumChannels() > 1) clip->R.assign (buf.getReadPointer (1), buf.getReadPointer (1) + len);
    clip->features = analyseTrack (clip->L, clip->R, clip->sampleRate);
    const juce::ScopedLock sl (lock);
    clips[kind] = clip;
    return {};
}

juce::String MediaLab::load (const juce::String& kind, const juce::File& file)
{
    auto* in = file.createInputStream().release();
    if (in == nullptr) return "Não consegui abrir o arquivo.";
    return decode (kind, in, file.getFileName(), file.getFileExtension());
}

juce::String MediaLab::loadFromBase64 (const juce::String& kind, const juce::String& name, const juce::String& base64)
{
    juce::MemoryOutputStream mo;
    if (! juce::Base64::convertFromBase64 (mo, base64)) return "Arquivo corrompido no envio.";
    auto block = mo.getMemoryBlock();
    return decode (kind, new juce::MemoryInputStream (block, true), name, name.fromLastOccurrenceOf (".", true, false));
}

void MediaLab::clear (const juce::String& kind) { const juce::ScopedLock sl (lock); clips.erase (kind); }

std::shared_ptr<const AudioClip> MediaLab::get (const juce::String& kind) const
{
    const juce::ScopedLock sl (lock);
    auto it = clips.find (kind); return it == clips.end() ? nullptr : it->second;
}

juce::var MediaLab::describe (const juce::String& kind) const
{
    auto c = get (kind); if (c == nullptr) return {};
    const auto& f = c->features;
    auto* o = new juce::DynamicObject();
    o->setProperty ("kind", kind); o->setProperty ("name", c->name); o->setProperty ("valid", f.valid);
    o->setProperty ("seconds", f.seconds); o->setProperty ("sampleRate", c->sampleRate);
    o->setProperty ("bpm", f.bpm); o->setProperty ("bpmConfidence", f.bpmConfidence);
    o->setProperty ("key", f.key); o->setProperty ("keyName", juce::String (keyName (f.key))); o->setProperty ("minor", f.minor); o->setProperty ("keyConfidence", f.keyConfidence);
    o->setProperty ("lufs", f.lufs); o->setProperty ("rmsDb", f.rmsDb); o->setProperty ("peakDb", f.peakDb); o->setProperty ("crestDb", f.crestDb);
    o->setProperty ("noiseFloorDb", f.noiseFloorDb); o->setProperty ("sibilanceDb", f.sibilanceDb);
    o->setProperty ("centroidHz", f.centroidHz); o->setProperty ("subSustain", f.subSustain); o->setProperty ("hatRate", f.hatRate); o->setProperty ("kickRate", f.kickRate);
    o->setProperty ("tripletRatio", f.tripletRatio); o->setProperty ("tripletRatioHalf", f.tripletRatioHalf);
    o->setProperty ("offbeatRatio", f.offbeatRatio); o->setProperty ("offbeatRatioHalf", f.offbeatRatioHalf); o->setProperty ("stereoWidth", f.stereoWidth);
    juce::Array<juce::var> bands, spec, wave;
    for (float b : f.bands) bands.add (b);
    for (float s : f.spectrum) spec.add (juce::roundToInt (s * 10.0f) / 10.0);
    const size_t step = std::max<size_t> (1, c->L.size() / 160);
    for (size_t i = 0; i + step <= c->L.size(); i += step)
    { float pk = 0; for (size_t j = i; j < i + step; ++j) pk = std::max (pk, std::abs (c->L[j])); wave.add (juce::roundToInt (pk * 100.0f) / 100.0); }
    o->setProperty ("bands", bands); o->setProperty ("spectrum", spec); o->setProperty ("wave", wave);
    return juce::var (o);
}

juce::var MediaLab::describeAll() const
{
    auto* o = new juce::DynamicObject();
    for (auto k : { "beat", "voice", "reference" }) if (get (k)) o->setProperty (k, describe (k));
    return juce::var (o);
}

// -----------------------------------------------------------------------------
//  Render da mix
// -----------------------------------------------------------------------------
static float lufsOf (const std::vector<float>& L, const std::vector<float>& R, double sr)
{
    LoudnessMeter m; m.prepare (sr);
    for (size_t s = 0; s < L.size(); s += 4096) { const int n = (int) std::min<size_t> (4096, L.size() - s); m.process (L.data() + s, R.data() + s, n); }
    return m.integrated;
}

juce::var MediaLab::renderMix (const ParamValues& params, double hostBpm)
{
    auto res = new juce::DynamicObject(); juce::var out (res);
    auto voice = get ("voice"), beat = get ("beat");
    if (voice == nullptr) { res->setProperty ("ok", false); res->setProperty ("error", "Anexa a tua voz (acapella) pra eu montar a mix."); return out; }
    const double sr = voice->sampleRate; const size_t N = voice->L.size();

    // voz processada pela cadeia atual
    VocalChain chain; chain.prepare (sr, 1024, false);
    const double bpm = hostBpm > 0 ? hostBpm : (beat && beat->features.bpm > 0 ? beat->features.bpm : 120.0);
    chain.setAnalysedBpm (bpm); chain.setParams (params, bpm);
    const int lat = chain.getLatency();
    std::vector<float> vL (N + (size_t) lat + 1024, 0.0f), vR (vL.size(), 0.0f);
    for (size_t i = 0; i < N; ++i) { vL[i] = voice->L[i]; vR[i] = voice->R.empty() ? voice->L[i] : voice->R[i]; }
    for (size_t s = 0; s < vL.size(); s += 1024) { const int n = (int) std::min<size_t> (1024, vL.size() - s); chain.process (vL.data() + s, vR.data() + s, nullptr, nullptr, n); }
    vL.erase (vL.begin(), vL.begin() + lat); vR.erase (vR.begin(), vR.begin() + lat); vL.resize (N); vR.resize (N);
    std::vector<float> rawL (voice->L), rawR (voice->R.empty() ? voice->L : voice->R);

    // beat na mesma taxa
    std::vector<float> bL, bR;
    if (beat)
    {
        bL = beat->sampleRate == sr ? beat->L : resampleTo (beat->L, beat->sampleRate, sr);
        const auto& br = beat->R.empty() ? beat->L : beat->R;
        bR = beat->sampleRate == sr ? br : resampleTo (br, beat->sampleRate, sr);
    }
    const size_t len = std::max (N, bL.size());
    auto mixDown = [&] (const std::vector<float>& xl, const std::vector<float>& xr, float vGain, float bGain, std::vector<float>& ol, std::vector<float>& orr)
    {
        ol.assign (len, 0.0f); orr.assign (len, 0.0f);
        for (size_t i = 0; i < N; ++i) { ol[i] += xl[i] * vGain; orr[i] += xr[i] * vGain; }
        for (size_t i = 0; i < bL.size(); ++i) { ol[i] += bL[i] * bGain; orr[i] += bR[i] * bGain; }
    };
    // equilíbrio voz x beat: beat ~2 dB abaixo da voz (padrão de rap/trap moderno)
    const float vLufs = lufsOf (vL, vR, sr), rawLufs = lufsOf (rawL, rawR, sr);
    const float bLufs = beat ? lufsOf (bL, bR, sr) : -70.0f;
    const float bGainAfter = beat ? dbToGain (clampf ((vLufs - 2.0f) - bLufs, -24.0f, 12.0f)) : 0.0f;
    const float bGainBefore = beat ? dbToGain (clampf ((rawLufs - 2.0f) - bLufs, -24.0f, 12.0f)) : 0.0f;

    auto master = [&] (std::vector<float>& l, std::vector<float>& r, float target)
    {
        for (int pass = 0; pass < 2; ++pass)
        {
            const float now = lufsOf (l, r, sr); if (now < -69.0f) return now;
            const float g = dbToGain (clampf (target - now, -24.0f, 24.0f));
            Limiter lim; lim.prepare (sr, (int) (0.0015 * sr)); lim.setParams (0.0f, -1.0f, 80.0f);
            for (auto& v : l) v *= g; for (auto& v : r) v *= g;
            std::vector<float> tl (l.size() + (size_t) lim.latency()), tr (tl.size());
            std::copy (l.begin(), l.end(), tl.begin()); std::copy (r.begin(), r.end(), tr.begin());
            for (size_t s = 0; s < tl.size(); s += 1024) { const int n = (int) std::min<size_t> (1024, tl.size() - s); lim.process (tl.data() + s, tr.data() + s, n); }
            std::copy (tl.begin() + lim.latency(), tl.end(), l.begin()); std::copy (tr.begin() + lim.latency(), tr.end(), r.begin());
        }
        return lufsOf (l, r, sr);
    };
    const float target = params[P::LufsTarget];
    std::vector<float> aL, aR, pL, pR;
    mixDown (vL, vR, 1.0f, bGainAfter, aL, aR); const float lufsAfter = master (aL, aR, target);
    mixDown (rawL, rawR, 1.0f, bGainBefore, pL, pR); const float lufsBefore = master (pL, pR, target);

    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
    const auto afterName = "CUPIDO-mix-" + stamp + ".wav", beforeName = "CUPIDO-antes-" + stamp + ".wav";
    auto write = [&] (const juce::String& nm, const std::vector<float>& l, const std::vector<float>& r)
    {
        auto bytes = encodeWavStereo24 (l, r, (int) std::lround (sr));
        return mixesFolder().getChildFile (nm).replaceWithData (bytes.data(), bytes.size());
    };
    const bool ok = write (afterName, aL, aR) && write (beforeName, pL, pR);
    // espectro da voz processada e do beat (para o diagnóstico de mascaramento)
    auto spec = [&] (const std::vector<float>& l, const std::vector<float>& r)
    { auto f = analyseTrack (l, r, sr); juce::Array<juce::var> a; for (float s : f.spectrum) a.add (juce::roundToInt (s * 10.0f) / 10.0); return a; };
    res->setProperty ("ok", ok); res->setProperty ("error", ok ? juce::String() : juce::String ("Não consegui salvar em Documentos/CUPIDO/Mixes."));
    res->setProperty ("after", afterName); res->setProperty ("before", beforeName);
    res->setProperty ("folder", mixesFolder().getFullPathName());
    res->setProperty ("lufsAfter", lufsAfter); res->setProperty ("lufsBefore", lufsBefore);
    res->setProperty ("voiceSpectrum", spec (vL, vR));
    if (beat) { juce::Array<juce::var> bs; for (float s : beat->features.spectrum) bs.add (juce::roundToInt (s * 10.0f) / 10.0); res->setProperty ("beatSpectrum", bs);
                res->setProperty ("beatGainDb", gainToDb (bGainAfter)); }
    return out;
}

// -----------------------------------------------------------------------------
//  Espectrograma (PNG em base64) para o Claude
// -----------------------------------------------------------------------------
juce::String MediaLab::spectrogramPngBase64() const
{
    std::vector<std::pair<juce::String, std::shared_ptr<const AudioClip>>> list;
    for (auto k : { "beat", "voice", "reference" }) if (auto c = get (k)) list.push_back ({ k, c });
    if (list.empty()) return {};
    const int W = 1000, H = 260 * (int) list.size();
    juce::Image img (juce::Image::RGB, W, H, true);
    juce::Graphics g (img); g.fillAll (juce::Colours::black);
    int row = 0;
    for (auto& [kind, c] : list)
    {
        const int y0 = row * 260; ++row;
        // mono, até 40 s do meio da faixa
        const size_t want = (size_t) (c->sampleRate * 40), start = c->L.size() > want ? (c->L.size() - want) / 2 : 0;
        const size_t n = std::min (want, c->L.size() - start);
        constexpr int order = 11, N = 1 << order; FFT fft; fft.prepare (order); std::vector<float> win; makeHann (win, N);
        std::vector<std::complex<float>> buf ((size_t) N);
        const int cols = W, rows = 240;
        for (int x = 0; x < cols; ++x)
        {
            const size_t s0 = start + (size_t) ((double) x / cols * (double) (n > N ? n - N : 0));
            for (int i = 0; i < N; ++i) { const size_t j = s0 + (size_t) i; const float v = j < c->L.size() ? (c->R.empty() ? c->L[j] : 0.5f * (c->L[j] + c->R[j])) : 0.0f; buf[(size_t) i] = { v * win[(size_t) i], 0.0f }; }
            fft.forward (buf.data());
            for (int yy = 0; yy < rows; ++yy)
            {
                const double f = 30.0 * std::pow (16000.0 / 30.0, (double) (rows - 1 - yy) / rows);
                const int k = std::min (N / 2 - 1, (int) (f * N / c->sampleRate));
                const float db = 20.0f * std::log10 (std::abs (buf[(size_t) k]) * 4.0f / N + 1e-9f);
                const float t = clampf ((db + 90.0f) / 80.0f, 0.0f, 1.0f);
                img.setPixelAt (x, y0 + 18 + yy, juce::Colour::fromHSV (0.7f - 0.7f * t, 0.9f, t, 1.0f));
            }
        }
        g.setColour (juce::Colours::white); g.setFont (14.0f);
        const auto label = (kind == juce::String ("beat") ? "BEAT" : kind == juce::String ("voice") ? "VOZ" : "REFERENCIA") + juce::String (" - ") + c->name
                         + "  (eixo vertical: 30 Hz embaixo -> 16 kHz em cima, escala log; cor: intensidade)";
        g.drawText (label, 6, y0, W - 12, 16, juce::Justification::left);
    }
    juce::MemoryOutputStream mo; juce::PNGImageFormat png; png.writeImageToStream (img, mo);
    return juce::Base64::toBase64 (mo.getData(), mo.getDataSize());
}
} // namespace cupido
