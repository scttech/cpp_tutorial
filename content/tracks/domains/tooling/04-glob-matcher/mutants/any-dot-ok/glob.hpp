// MUTANT: ? matches a leading dot
#pragma once
// YOUR GLOB MATCHER. namespace glob. A pattern is matched against a whole path (forward slashes separate segments). Bytes, not characters: '?' matches one BYTE.
//
// PATTERN SYNTAX
//   ?          any one character except '/'
//   *          any run of characters (also none) except '/'                       "*.cpp" matches "a.cpp" but not "dir/a.cpp"
//   **         when it is a WHOLE path segment (nothing but '**' between two '/' or the ends of the pattern): matches across segments
//                "**"       the whole pattern: everything, any number of characters including '/'
//                "**/x"     at the start: "x" in any directory (zero or more leading "segment/" pieces):  matches "x", "d/x", "d/e/x"
//                "a/**/x"   in the middle: zero or more whole directories between:                       matches "a/x", "a/d/x", "a/d/e/x"
//                "a/**"     at the end: ONE OR MORE characters after "a/":                                matches "a/x", "a/d/x", but neither "a" nor "a/"
//              '**' that is not a whole segment ("a**b", "**x") is just '*'.
//   [abc] [a-z] [!abc] [^abc]   one character from the set (or, with ! or ^, not from it). A ']' right after '[' (or after the negation) is a member of the set; '-' as the first or last member is itself.
//                A set never matches '/', not even a negated one or one that lists it. Backslash escapes inside a set. "[]" is not a set (the ']' is a member, so it is unterminated).
//   {a,b,c}    alternatives, each an arbitrary pattern (also empty, "{,x}", also with wildcards and nested braces); a ',' or '}' outside braces is an ordinary character.
//   \x         the character x, literally (\* \? \[ \{ \\ \,)
//   anything else matches itself. '/' matches only '/'.
// OPTIONS   Options { bool dot = false; bool ignore_case = false; }
//   dot == false: a path segment that starts with '.' (the first character of the path, or the one after a '/') can only be matched by a '.' written in the pattern: the wildcards * ? [..] and ** do not
//                 match it ("*" does not match ".git", ".*" does, "**/x" does not look inside ".hidden/"). dot == true: they do.
//   ignore_case: ASCII letters compare case-insensitively, in literals and in sets (ranges too: [a-c] matches 'B').
// check(pattern) -> optional<Error>      nullopt if the pattern is valid. Error { size_t position; std::string message }; position is a byte offset:
//     unterminated set: the '[' ; reversed range ("[z-a]"): the range's first character; unterminated brace: the '{' ; trailing backslash: the backslash ; braces nested deeper than 32: the 33rd '{'.
// Glob::compile(pattern, options) -> optional<Glob>     nullopt for an invalid pattern.      Glob::matches(path) const -> bool      the whole path must match.
// match(pattern, path, options = {}) -> bool            compile + matches; an invalid pattern matches nothing.
//
// REQUIREMENTS   matches() must run in O(length of path x size of pattern) time, whatever the pattern: simulate the pattern as an NFA (all possible positions at once), never backtrack.
//                ("*a*a*a*a*a*a*a*a*a*b" against 200 'a's must not take longer than a blink.) No recursion over the path or over a long pattern: a pattern of a million characters must not overflow the stack
//                (only nesting of { } is recursive, and it is limited to 32). matches() makes a bounded number of heap allocations, independent of the path length.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace glob {

struct Options {
    bool dot = false;
    bool ignore_case = false;
};

struct Error {
    std::size_t position;
    std::string message;
};

namespace detail {

enum class Op : std::uint8_t { Char, Any, AnyNoSlash, AnyPath, Set, Split, Jmp, Guard, Match };

struct Inst {
    Op op;
    unsigned char c = 0;
    int x = 0, y = 0;
};

using SetTable = std::array<bool, 256>;

inline unsigned char lower(unsigned char c) { return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c - 'A' + 'a') : c; }
inline unsigned char upper(unsigned char c) { return (c >= 'a' && c <= 'z') ? static_cast<unsigned char>(c - 'a' + 'A') : c; }

class Compiler {
public:
    Compiler(std::string_view p, Options o) : p_(p), opt_(o) {}

    bool run() {
        if (!seq(0)) return false;
        emit({Op::Match});
        return true;
    }
    std::vector<Inst>& program() { return prog_; }
    std::vector<SetTable>& sets() { return sets_; }
    const Error& error() const { return err_; }

private:
    int pc() const { return static_cast<int>(prog_.size()); }
    int emit(Inst in) {
        prog_.push_back(in);
        return pc() - 1;
    }
    bool fail(std::size_t at, std::string msg) {
        err_ = Error{at, std::move(msg)};
        return false;
    }
    void literal(unsigned char c) {
        Inst in{Op::Char};
        in.c = opt_.ignore_case ? lower(c) : c;
        emit(in);
    }

    // a loop that consumes any run of characters of one kind: L: split(body, out); body; jmp L
    void star_loop(Op body) {
        const int l = emit({Op::Split});
        prog_[static_cast<std::size_t>(l)].x = pc();
        emit({body});
        Inst back{Op::Jmp};
        back.x = l;
        emit(back);
        prog_[static_cast<std::size_t>(l)].y = pc();
    }

    bool seq(int depth) {
        while (i_ < p_.size()) {
            const char c = p_[i_];
            if (depth > 0 && (c == ',' || c == '}')) break;
            if (c == '\\') {
                if (i_ + 1 >= p_.size()) return fail(i_, "a pattern cannot end with a backslash");
                literal(static_cast<unsigned char>(p_[i_ + 1]));
                i_ += 2;
            } else if (c == '?') {
                emit({Op::Any});
                ++i_;
            } else if (c == '*') {
                if (!star()) return false;
            } else if (c == '[') {
                if (!set()) return false;
            } else if (c == '{') {
                if (!alternatives(depth)) return false;
            } else {
                literal(static_cast<unsigned char>(c));
                ++i_;
            }
        }
        return true;
    }

    bool star() {
        const bool double_star = i_ + 1 < p_.size() && p_[i_ + 1] == '*';
        if (double_star) {
            const bool left_ok = i_ == 0 || p_[i_ - 1] == '/';
            const bool at_end = i_ + 2 == p_.size();
            const bool before_slash = i_ + 2 < p_.size() && p_[i_ + 2] == '/';
            if (left_ok && at_end) {
                if (i_ == 0) {
                    star_loop(Op::AnyPath);   // "**": everything
                } else {   // "a/**": one or more characters
                    const int first = emit({Op::AnyPath});
                    Inst again{Op::Split};
                    again.x = first;
                    again.y = pc() + 1;
                    emit(again);
                }
                i_ += 2;
                return true;
            }
            if (left_ok && before_slash) {   // "**/": zero or more "segment/"
                const int l = emit({Op::Split});
                prog_[static_cast<std::size_t>(l)].x = pc();
                emit({Op::Guard});
                star_loop(Op::AnyNoSlash);
                Inst slash{Op::Char};
                slash.c = '/';
                emit(slash);
                Inst back{Op::Jmp};
                back.x = l;
                emit(back);
                prog_[static_cast<std::size_t>(l)].y = pc();
                i_ += 3;
                return true;
            }
        }
        while (i_ < p_.size() && p_[i_] == '*') ++i_;   // a run of stars is one star
        emit({Op::Guard});
        star_loop(Op::AnyNoSlash);
        return true;
    }

    bool set() {
        const std::size_t open = i_;
        std::size_t j = i_ + 1;
        bool negate = false;
        if (j < p_.size() && (p_[j] == '!' || p_[j] == '^')) {
            negate = true;
            ++j;
        }
        SetTable t{};
        bool first = true;
        for (;;) {
            if (j >= p_.size()) return fail(open, "the set is never closed");
            unsigned char c = static_cast<unsigned char>(p_[j]);
            if (c == ']' && !first) break;
            first = false;
            if (c == '\\') {
                if (j + 1 >= p_.size()) return fail(open, "the set is never closed");
                c = static_cast<unsigned char>(p_[++j]);
            }
            if (j + 2 < p_.size() && p_[j + 1] == '-' && p_[j + 2] != ']') {
                const std::size_t range_start = j;
                unsigned char hi = static_cast<unsigned char>(p_[j + 2]);
                std::size_t next = j + 3;
                if (hi == '\\') {
                    if (j + 3 >= p_.size()) return fail(open, "the set is never closed");
                    hi = static_cast<unsigned char>(p_[j + 3]);
                    next = j + 4;
                }
                if (hi < c) return fail(range_start, "the range is reversed");
                for (unsigned k = c; k <= hi; ++k) t[k] = true;
                j = next;
            } else {
                t[c] = true;
                ++j;
            }
        }
        if (opt_.ignore_case)
            for (unsigned k = 0; k < 256; ++k)
                if (t[k]) {
                    t[lower(static_cast<unsigned char>(k))] = true;
                    t[upper(static_cast<unsigned char>(k))] = true;
                }
        if (negate)
            for (bool& b : t) b = !b;
        t[static_cast<unsigned char>('/')] = false;
        sets_.push_back(t);
        Inst in{Op::Set};
        in.x = static_cast<int>(sets_.size()) - 1;
        emit(in);
        i_ = j + 1;
        return true;
    }

    bool alternatives(int depth) {
        const std::size_t open = i_;
        if (depth >= 32) return fail(open, "braces are nested too deeply");
        ++i_;
        std::vector<int> ends;
        for (;;) {
            const int split = emit({Op::Split});
            prog_[static_cast<std::size_t>(split)].x = pc();
            if (!seq(depth + 1)) return false;
            ends.push_back(emit({Op::Jmp}));
            if (i_ >= p_.size()) return fail(open, "the brace is never closed");
            if (p_[i_] == ',') {
                prog_[static_cast<std::size_t>(split)].y = pc();
                ++i_;
                continue;
            }
            prog_[static_cast<std::size_t>(split)].y = split + 1;   // the last alternative: no further choice
            ++i_;
            break;
        }
        for (int e : ends) prog_[static_cast<std::size_t>(e)].x = pc();
        return true;
    }

    std::string_view p_;
    Options opt_;
    std::size_t i_ = 0;
    std::vector<Inst> prog_;
    std::vector<SetTable> sets_;
    Error err_{0, ""};
};

}  // namespace detail

inline std::optional<Error> check(std::string_view pattern) {
    detail::Compiler c(pattern, Options{});
    if (c.run()) return std::nullopt;
    return c.error();
}

class Glob {
public:
    static std::optional<Glob> compile(std::string_view pattern, Options options = {}) {
        detail::Compiler c(pattern, options);
        if (!c.run()) return std::nullopt;
        Glob g;
        g.prog_ = std::move(c.program());
        g.sets_ = std::move(c.sets());
        g.opt_ = options;
        return g;
    }

    bool matches(std::string_view s) const {
        using detail::Op;
        std::vector<int> cur, next, stack;
        std::vector<std::uint32_t> mark(prog_.size(), 0);
        std::uint32_t gen = 0;
        const auto segment_start = [&](std::size_t i) { return i == 0 || s[i - 1] == '/'; };
        const auto dot_blocked = [&](std::size_t i) { return !opt_.dot && i < s.size() && s[i] == '.' && segment_start(i); };
        const auto add = [&](std::vector<int>& list, int start, std::size_t pos) {
            stack.push_back(start);
            while (!stack.empty()) {
                const int pc = stack.back();
                stack.pop_back();
                if (mark[static_cast<std::size_t>(pc)] == gen) continue;
                mark[static_cast<std::size_t>(pc)] = gen;
                const detail::Inst& in = prog_[static_cast<std::size_t>(pc)];
                switch (in.op) {
                    case Op::Jmp: stack.push_back(in.x); break;
                    case Op::Split:
                        stack.push_back(in.y);
                        stack.push_back(in.x);
                        break;
                    case Op::Guard:
                        if (!dot_blocked(pos)) stack.push_back(pc + 1);
                        break;
                    default: list.push_back(pc);
                }
            }
        };
        ++gen;
        add(cur, 0, 0);
        for (std::size_t pos = 0; pos < s.size() && !cur.empty(); ++pos) {
            const unsigned char c = static_cast<unsigned char>(s[pos]);
            ++gen;
            next.clear();
            for (int pc : cur) {
                const detail::Inst& in = prog_[static_cast<std::size_t>(pc)];
                bool ok = false;
                switch (in.op) {
                    case Op::Char: ok = (opt_.ignore_case ? detail::lower(c) : c) == in.c; break;
                    case Op::Any: ok = c != '/'; break;
                    case Op::AnyNoSlash: ok = c != '/'; break;
                    case Op::AnyPath: ok = !dot_blocked(pos); break;
                    case Op::Set: ok = sets_[static_cast<std::size_t>(in.x)][c] && !dot_blocked(pos); break;
                    default: break;
                }
                if (ok) add(next, pc + 1, pos + 1);
            }
            cur.swap(next);
        }
        for (int pc : cur)
            if (prog_[static_cast<std::size_t>(pc)].op == Op::Match) return true;
        return false;
    }

private:
    std::vector<detail::Inst> prog_;
    std::vector<detail::SetTable> sets_;
    Options opt_;
};

inline bool match(std::string_view pattern, std::string_view path, Options options = {}) {
    const auto g = Glob::compile(pattern, options);
    return g && g->matches(path);
}

}  // namespace glob
