// A tiny "grep-like" tool built on the argument parser. Read-only. Press Run.
#include <cstdio>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "cli.hpp"

int main() {
    cli::Parser parser("minigrep", "Search files for a pattern.");
    parser.add({"ignore-case", 'i', cli::Kind::Flag, "", "Ignore case.", false, false, std::nullopt});
    parser.add({"verbose", 'v', cli::Kind::Flag, "", "Print more (repeat for even more).", true, false, std::nullopt});
    parser.add({"max-count", 'm', cli::Kind::Value, "N", "Stop after N matches.", false, false, std::string("100")});
    parser.add({"exclude", 'x', cli::Kind::Value, "GLOB", "Skip files matching GLOB.", true, false, std::nullopt});
    parser.add({"color", 0, cli::Kind::Value, "WHEN", "auto, always or never.", false, false, std::string("auto")});
    parser.add({"pattern", 'e', cli::Kind::Value, "PATTERN", "The pattern to look for.", false, true, std::nullopt});
    parser.add(cli::Positional{"files", false, true, "Files to search."});
    if (const auto problem = parser.check()) {
        std::printf("bad definition: %s\n", problem->c_str());
        return 2;
    }
    std::printf("%s\n", parser.help().c_str());

    const std::vector<std::vector<std::string_view>> runs = {
        {"-ivv", "-e", "needle", "--max-count=5", "a.txt", "b.txt"},
        {"-eneedle", "--exc", "*.o", "--exc=*.tmp", "--", "-weird-file-name"},
        {"--color", "-i", "-e", "x"},          // a value may start with a dash
        {"-e", "x", "--max-count"},            // missing value
        {"--verbose=3", "-e", "x"},            // a flag takes no value
        {"-e", "x", "--max=1", "-m", "2"},     // "--max" is a unique prefix of --max-count, but the option is given twice
        {"-q"},
        {"--c", "-e", "x"},                    // "--c" is a unique prefix of --color, which takes "-e" as its value; then the required -e is missing
        {"a.txt"},                             // the required -e is missing
    };
    for (const auto& args : runs) {
        const cli::Result r = parser.parse(args);
        std::printf("args:");
        for (auto a : args) std::printf(" %s", std::string(a).c_str());
        if (r.error) std::printf("\n  -> error: %s\n", cli::describe(*r.error).c_str());
        else {
            std::printf("\n  -> pattern=%s ignore-case=%d verbose=%zu max-count=%s color=%s files=%zu excludes=%zu\n", std::string(r.parsed->value("pattern").value_or("?")).c_str(), r.parsed->flag("ignore-case"),
                        r.parsed->count("verbose"), std::string(r.parsed->value("max-count").value_or("?")).c_str(), std::string(r.parsed->value("color").value_or("?")).c_str(), r.parsed->positionals().size(),
                        r.parsed->values("exclude").size());
        }
    }
}
