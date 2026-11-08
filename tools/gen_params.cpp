// Gera ui/params.js a partir de source/Params.h (fonte única de verdade).
//   g++ -std=c++17 tools/gen_params.cpp -o gen_params && ./gen_params > ui/params.js
#include "../source/Params.h"
#include <cstdio>
#include <string>

static std::string esc (const char* s) { std::string o; for (; *s; ++s) { if (*s == '"' || *s == '\\') o += '\\'; o += *s; } return o; }

int main()
{
    std::printf ("// GERADO por tools/gen_params.cpp a partir de source/Params.h — não edite à mão.\n");
    std::printf ("window.CUPIDO_SPEC = [\n");
    for (int i = 0; i < cupido::kNumParams; ++i)
    {
        const auto& p = cupido::paramTable()[i];
        std::printf ("  {\"id\":\"%s\",\"label\":\"%s\",\"module\":\"%s\",\"min\":%g,\"max\":%g,\"def\":%g,\"step\":%g,\"skew\":%d,\"unit\":\"%s\",\"choices\":[",
                     p.id, esc (p.label).c_str(), p.module, p.min, p.max, p.def, p.step, p.skew, esc (p.unit).c_str());
        std::string ch = p.choices; size_t start = 0; bool first = true;
        if (! ch.empty())
            while (true)
            {
                size_t bar = ch.find ('|', start);
                std::printf ("%s\"%s\"", first ? "" : ",", esc (ch.substr (start, bar - start).c_str()).c_str()); first = false;
                if (bar == std::string::npos) break; start = bar + 1;
            }
        std::printf ("]}%s\n", i + 1 < cupido::kNumParams ? "," : "");
    }
    std::printf ("];\n");
}
