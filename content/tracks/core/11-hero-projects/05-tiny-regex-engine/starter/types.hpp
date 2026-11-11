#pragma once
// READ-ONLY. The result types, the syntax tree, the NFA, and the counter that the Fast tests read.
#include <bitset>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace rx {

namespace stats {
inline unsigned long long state_visits = 0;  // how many times a matcher looked at an NFA state (Nfa::at)
inline void reset() { state_visits = 0; }
}  // namespace stats

struct Match {
    std::size_t begin = 0;
    std::size_t end = 0;  // one past the last matched byte; begin == end is an empty match
    friend bool operator==(const Match& a, const Match& b) { return a.begin == b.begin && a.end == b.end; }
};

enum class CompileError {
    None,
    UnmatchedParen,  // a ) without a ( (position: the ")"), or a ( never closed (position: that "(", the innermost one)
    NothingToRepeat, // * + ? at the start, after ( or |, or after ^ or $ (position: the quantifier)
    BadEscape,       // \ followed by something that is not an escape, or at the end (position: the backslash)
    BadClass,        // [ never closed, an empty class [] or [^], a range out of order [z-a] (position: the "[", or the start of the range)
    TooDeep,         // more than kMaxDepth nested parentheses, or kMaxDepth stacked quantifiers (position: the offending ( or quantifier)
};
inline constexpr int kMaxDepth = 100;

inline const char* name(CompileError e) {
    switch (e) {
        case CompileError::None: return "None";
        case CompileError::UnmatchedParen: return "UnmatchedParen";
        case CompileError::NothingToRepeat: return "NothingToRepeat";
        case CompileError::BadEscape: return "BadEscape";
        case CompileError::BadClass: return "BadClass";
        case CompileError::TooDeep: return "TooDeep";
    }
    return "?";
}

// ---- the syntax tree (what parser.hpp produces) --------------------------------------------------
struct Node;
using NodePtr = std::unique_ptr<Node>;

struct Empty {};                              // matches the empty string
struct Literal { unsigned char ch; };         // one byte
struct AnyChar {};                            // any byte except '\n'
struct CharSet { std::bitset<256> bits; };    // any byte in the set ([abc], [^a], \d ...: negation is already applied)
struct LineStart {};                          // ^  (only at the start of the text)
struct LineEnd {};                            // $  (only at the end of the text)
struct Concat { std::vector<NodePtr> parts; };    // two or more parts, one after the other
struct Alt { std::vector<NodePtr> options; };     // two or more alternatives
struct Repeat { char kind; NodePtr child; };      // kind is '*', '+' or '?'

struct Node {
    std::variant<Empty, Literal, AnyChar, CharSet, LineStart, LineEnd, Concat, Alt, Repeat> v;
    template <class T>
    explicit Node(T x) : v(std::move(x)) {}
};

inline NodePtr make_empty() { return std::make_unique<Node>(Empty{}); }
inline NodePtr make_literal(unsigned char c) { return std::make_unique<Node>(Literal{c}); }
inline NodePtr make_any() { return std::make_unique<Node>(AnyChar{}); }
inline NodePtr make_set(const std::bitset<256>& bits) { return std::make_unique<Node>(CharSet{bits}); }
inline NodePtr make_line_start() { return std::make_unique<Node>(LineStart{}); }
inline NodePtr make_line_end() { return std::make_unique<Node>(LineEnd{}); }
inline NodePtr make_concat(std::vector<NodePtr> parts) { return std::make_unique<Node>(Concat{std::move(parts)}); }
inline NodePtr make_alt(std::vector<NodePtr> options) { return std::make_unique<Node>(Alt{std::move(options)}); }
inline NodePtr make_repeat(char kind, NodePtr child) { return std::make_unique<Node>(Repeat{kind, std::move(child)}); }

// The shape of a tree as text: a b (cat a b c) (alt a b) (* a) (+ a) (? a) () ^ $ . set(3) \.  \x20
inline std::string describe(const Node& n) {
    if (std::holds_alternative<Empty>(n.v)) return "()";
    if (const auto* lit = std::get_if<Literal>(&n.v)) {
        const char c = static_cast<char>(lit->ch);
        if (lit->ch <= 0x20 || lit->ch >= 0x7f) {
            static const char* hex = "0123456789abcdef";
            return std::string("\\x") + hex[lit->ch >> 4] + hex[lit->ch & 15];
        }
        const std::string meta = "\\.*+?|()[]^$";
        return (meta.find(c) != std::string::npos ? "\\" : "") + std::string(1, c);
    }
    if (std::holds_alternative<AnyChar>(n.v)) return ".";
    if (const auto* set = std::get_if<CharSet>(&n.v)) return "set(" + std::to_string(set->bits.count()) + ")";
    if (std::holds_alternative<LineStart>(n.v)) return "^";
    if (std::holds_alternative<LineEnd>(n.v)) return "$";
    if (const auto* cat = std::get_if<Concat>(&n.v)) {
        std::string s = "(cat";
        for (const NodePtr& p : cat->parts) s += " " + describe(*p);
        return s + ")";
    }
    if (const auto* alt = std::get_if<Alt>(&n.v)) {
        std::string s = "(alt";
        for (const NodePtr& p : alt->options) s += " " + describe(*p);
        return s + ")";
    }
    const auto& rep = std::get<Repeat>(n.v);
    return std::string("(") + rep.kind + " " + describe(*rep.child) + ")";
}

struct ParseResult {
    NodePtr ast;  // null when there is an error
    CompileError error = CompileError::None;
    std::size_t pos = 0;   // 0-based offset in the pattern
    std::string message;   // never empty when there is an error
    bool ok() const { return error == CompileError::None; }
};

// ---- the NFA (what nfa.hpp builds and matcher.hpp runs) --------------------------------------------
//   Char   consume the byte `ch`, go to `out`            Any    consume any byte except '\n', go to `out`
//   Set    consume a byte in sets[set], go to `out`      Split  no input consumed: go to `out` AND to `out1`
//   Jump   no input consumed: go to `out`                Begin  no input: go to `out` only at the start of the text
//   End    no input: go to `out` only at the end         Match  the regular expression has matched
enum class Op { Char, Any, Set, Split, Jump, Begin, End, Match };
inline constexpr std::size_t kNone = static_cast<std::size_t>(-1);

struct State {
    Op op = Op::Match;
    unsigned char ch = 0;
    std::size_t set = 0;
    std::size_t out = kNone;
    std::size_t out1 = kNone;
};

class Nfa {
public:
    // building
    std::size_t add(State s) {
        states_.push_back(s);
        return states_.size() - 1;
    }
    std::size_t add_set(const std::bitset<256>& bits) {
        sets_.push_back(bits);
        return sets_.size() - 1;
    }
    State& edit(std::size_t i) { return states_[i]; }  // for patching `out` later; not counted
    void set_start(std::size_t s) { start_ = s; }

    // matching: every look at a state is counted
    const State& at(std::size_t i) const {
        ++stats::state_visits;
        return states_[i];
    }
    const std::bitset<256>& set(std::size_t i) const { return sets_[i]; }
    std::size_t start() const { return start_; }
    std::size_t size() const { return states_.size(); }

private:
    std::vector<State> states_;
    std::vector<std::bitset<256>> sets_;
    std::size_t start_ = kNone;
};

}  // namespace rx
