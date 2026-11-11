#pragma once
// READ-ONLY. The public face of the engine: compile a pattern, then match. It uses your three files.
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "matcher.hpp"
#include "nfa.hpp"
#include "parser.hpp"
#include "types.hpp"

namespace rx {

class Regex {
public:
    explicit Regex(Nfa nfa) : nfa_(std::move(nfa)) {}

    // does the whole text match?
    bool full_match(std::string_view text) const { return nfa_full_match(nfa_, text); }

    // the leftmost match that starts at or after `from`; among those that start there, the longest
    std::optional<Match> search(std::string_view text, std::size_t from = 0) const { return nfa_search(nfa_, text, from); }

    // all matches, left to right, not overlapping. After a non-empty match the next search starts where it ended (an empty match
    // right there is possible); after an empty match it starts one byte later. "a*" in "baaac": [0,0) [1,4) [4,4) [5,5)
    std::vector<Match> find_all(std::string_view text) const {
        std::vector<Match> all;
        std::size_t from = 0;
        while (from <= text.size()) {
            const std::optional<Match> m = search(text, from);
            if (!m) break;
            all.push_back(*m);
            from = m->end > m->begin ? m->end : m->end + 1;
        }
        return all;
    }

    const Nfa& nfa() const { return nfa_; }

private:
    Nfa nfa_;
};

struct CompileResult {
    std::optional<Regex> regex;  // empty when there is an error
    CompileError error = CompileError::None;
    std::size_t pos = 0;
    std::string message;
    bool ok() const { return error == CompileError::None; }
};

inline CompileResult compile(std::string_view pattern) {
    CompileResult r;
    ParseResult parsed = parse(pattern);
    if (!parsed.ok()) {
        r.error = parsed.error;
        r.pos = parsed.pos;
        r.message = parsed.message;
        return r;
    }
    r.regex.emplace(build_nfa(*parsed.ast));
    return r;
}

}  // namespace rx
