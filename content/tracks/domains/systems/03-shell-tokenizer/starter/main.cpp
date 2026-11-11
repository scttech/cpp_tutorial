// Split command lines into words and operators, the way a shell does. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "tokenizer.hpp"
#include "types.hpp"

int main() {
    const shell::Env env = {{"HOME", "/home/ada"}, {"NAME", "World"}};
    const char* lines[] = {
        R"(ls -la /tmp)",
        R"(echo "Hello, $NAME!" 'no $expansion here' > out.txt)",
        R"(grep -R "TODO: fix" src/ | sort -u && echo done # trailing comment)",
        R"(cp my\ file.txt "$HOME/backup dir"/ ; echo "")",
        R"(echo "never closed)",
        R"(echo ${HOME)",
    };
    for (const char* line : lines) {
        std::printf("%s\n", line);
        const shell::Result r = shell::tokenize(line, env);
        for (const shell::Token& t : r.tokens) std::printf("  %s %s  (at %zu)\n", t.kind == shell::TokenKind::Word ? "word    " : "operator", ("[" + t.text + "]").c_str(), t.pos);
        if (!r.ok()) std::printf("  error: %s at %zu\n", shell::describe(r.error), r.error_pos);
    }
}
