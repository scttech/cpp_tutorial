// Reads one expression per line from the input box (samples if it is empty) and shows each stage. Read-only. Press Run.
// The variables are x = 2, y = 3 and pi = 3.14159265.
#include <iostream>
#include <string>

#include "eval.hpp"
#include "lexer.hpp"
#include "optimize.hpp"
#include "parser.hpp"

int main() {
    expr::Env env;
    env.set("x", 2);
    env.set("y", 3);
    env.set("pi", 3.14159265);

    std::string line;
    int lines = 0;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        ++lines;
        std::cout << "> " << line << "\n";
        const expr::TokenizeResult t = expr::tokenize(line);
        if (!t.ok()) {
            std::cout << "  error " << expr::name(t.error) << " at " << t.pos << ": " << t.message << "\n";
            continue;
        }
        expr::ParseResult p = expr::parse(t.tokens);
        if (!p.ok()) {
            std::cout << "  error " << expr::name(p.error) << " at " << p.pos << ": " << p.message << "\n";
            continue;
        }
        std::cout << "  tree:      " << expr::to_string(*p.ast) << "  (" << expr::node_count(*p.ast) << " nodes)\n";
        expr::NodePtr o = expr::optimize(std::move(p.ast));
        std::cout << "  optimized: " << expr::to_string(*o) << "  (" << expr::node_count(*o) << " nodes)\n";
        const expr::EvalResult r = expr::evaluate(*o, env);
        if (r.ok()) std::cout << "  value:     " << r.value << "\n";
        else std::cout << "  error " << expr::name(r.error) << (r.detail.empty() ? "" : " (" + r.detail + ")") << "\n";
    }
    if (lines == 0) {
        std::cout << "(type expressions in the input box, one per line, for example:  2 * (3 + 4) - x ^ 2 )\n";
    }
}
