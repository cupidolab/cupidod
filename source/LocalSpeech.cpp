#include "LocalSpeech.h"
#if CUPIDO_HAS_WHISPER
 #include <whisper.h>
#endif

namespace cupido
{
LocalSpeech& LocalSpeech::instance() { static LocalSpeech s; return s; }

bool LocalSpeech::isAvailable()
{
   #if CUPIDO_HAS_WHISPER
    return true;
   #else
    return false;
   #endif
}

LocalSpeech::~LocalSpeech()
{
   #if CUPIDO_HAS_WHISPER
    if (ctx) whisper_free ((whisper_context*) ctx);
   #endif
}

juce::File LocalSpeech::modelsFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("CUPIDO").getChildFile ("Modelos");
    f.createDirectory(); return f;
}

juce::File LocalSpeech::modelFile (const juce::String& size) const
{
    const auto s = size == "tiny" || size == "small" ? size : juce::String ("base");
    return modelsFolder().getChildFile ("ggml-" + s + ".bin");
}

juce::String LocalSpeech::download (const juce::String& size)
{
    const auto target = modelFile (size);
    if (target.existsAsFile()) return {};
    downloading.store (true); progress.store (0);
    const auto name = target.getFileName();
    juce::URL url ("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/" + name);
    int status = 0; juce::StringPairArray headers;
    std::unique_ptr<juce::InputStream> in (url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                .withConnectionTimeoutMs (20000).withNumRedirectsToFollow (8).withStatusCode (&status).withResponseHeaders (&headers)));
    if (in == nullptr || status >= 400) { downloading.store (false); return "Não consegui baixar o modelo de voz (HTTP " + juce::String (status) + "). Verifique a internet."; }
    const auto total = in->getTotalLength();
    auto tmp = target.withFileExtension (".part"); tmp.deleteFile();
    {
        juce::FileOutputStream out (tmp);
        if (! out.openedOk()) { downloading.store (false); return "Sem permissão para salvar em " + modelsFolder().getFullPathName(); }
        juce::HeapBlock<char> buf (1 << 16); juce::int64 got = 0;
        while (! in->isExhausted())
        {
            const int n = in->read (buf.get(), 1 << 16);
            if (n <= 0) break;
            out.write (buf.get(), (size_t) n); got += n;
            if (total > 0) progress.store ((float) ((double) got / (double) total));
        }
        out.flush();
        if (total > 0 && got < total) { downloading.store (false); tmp.deleteFile(); return "Download interrompido. Tente de novo."; }
    }
    tmp.moveFileTo (target); progress.store (1.0f); downloading.store (false);
    return {};
}

juce::String LocalSpeech::transcribe (const std::vector<float>& pcm, const juce::String& size, const juce::String& language, juce::String& error)
{
   #if CUPIDO_HAS_WHISPER
    const juce::ScopedLock sl (lock);
    const auto file = modelFile (size);
    if (! file.existsAsFile()) { error = "Modelo de voz ainda não baixado."; return {}; }
    if (ctx == nullptr || ctxModel != file.getFullPathName())
    {
        if (ctx) whisper_free ((whisper_context*) ctx);
        auto cp = whisper_context_default_params(); cp.use_gpu = false;
        ctx = whisper_init_from_file_with_params (file.getFullPathName().toRawUTF8(), cp);
        ctxModel = file.getFullPathName();
        if (ctx == nullptr) { error = "Modelo de voz corrompido — apague " + file.getFullPathName() + " e tente de novo."; return {}; }
    }
    auto wp = whisper_full_default_params (WHISPER_SAMPLING_GREEDY);
    const juce::String lang = language.isEmpty() ? juce::String ("pt") : language;
    wp.language = lang.toRawUTF8();
    wp.n_threads = juce::jlimit (1, 8, juce::SystemStats::getNumCpus() - 1);
    wp.print_progress = false; wp.print_realtime = false; wp.print_timestamps = false; wp.print_special = false;
    wp.no_context = true; wp.suppress_blank = true; wp.translate = false;
    static const char* prompt = "Comandos de mixagem vocal em português: autotune, afinação, delay, reverb, eco, agudos, graves, médios, saturação, de-esser, compressor, "
                                "largura, dobra, volume, master, Matuê, Teto, Yunk Vino, Veigh, trap, drill, plug, funk, R&B, reggae, mixa pra mim, analisa o beat.";
    wp.initial_prompt = prompt;
    if (whisper_full ((whisper_context*) ctx, wp, pcm.data(), (int) pcm.size()) != 0) { error = "Falha ao transcrever."; return {}; }
    juce::String text;
    const int ns = whisper_full_n_segments ((whisper_context*) ctx);
    for (int i = 0; i < ns; ++i) text << juce::String (juce::CharPointer_UTF8 (whisper_full_get_segment_text ((whisper_context*) ctx, i)));
    text = text.trim();
    if (text.isEmpty()) error = "Não entendi o áudio — fala mais perto do mic.";
    return text;
   #else
    juce::ignoreUnused (pcm, size, language);
    error = "Esta versão foi compilada sem o reconhecimento de voz local.";
    return {};
   #endif
}
} // namespace cupido
