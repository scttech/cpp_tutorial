#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// CAPSTONE: a text-processing pipeline made of interchangeable stages (a plugin architecture).
//
// A Pipeline is a recipe: an ordered list of Stage objects, owned through std::unique_ptr. Text goes in, line by line, through every stage; each
// stage can change a line, drop it, or reject it with an exception. New stages can be added without touching the Pipeline, even by code written
// after it, through the Registry. Everything here uses what the module taught: interfaces, virtual functions, clone(), composition, smart pointers.
//
// The tiers: Works (the stages, the pipeline, the registry), Robust (state, errors, copies, edge cases), Clean (no raw new/delete,
// `override` on every override, no dynamic_cast/typeid). Read the whole file first; the parts marked "given" are complete.

// ==== The interface of a stage (given) ===================================================================================================================
// A stage looks at ONE line. It returns the line to pass on (changed or not), or std::nullopt to DROP the line. It may throw (std::runtime_error is
// the convention) to say that the line is bad. A stage may keep state between lines (a counter, the previous line); clone() makes a copy of the REAL
// object, which is how a Pipeline is copied and how every run starts with fresh stages.
class Stage {
public:
    virtual ~Stage() = default;
    virtual std::string name() const = 0;
    virtual std::optional<std::string> process(std::string line) = 0;
    virtual std::unique_ptr<Stage> clone() const = 0;

protected:
    Stage() = default;
    Stage(const Stage&) = default;  // derived classes are copyable, so clone() can be `make_unique<Derived>(*this)`
    Stage& operator=(const Stage&) = default;
};

// One line that a stage rejected by throwing; `line` counts from 1, in the INPUT text.
struct StageError {
    std::size_t line;
    std::string stage;
    std::string message;
};

struct Result {
    std::vector<std::string> lines;  // the lines that came out of the last stage
    std::vector<StageError> errors;  // in input order

    std::string text() const {  // the lines joined by '\n' (no newline after the last one)
        std::string out;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            if (i) out += '\n';
            out += lines[i];
        }
        return out;
    }
};

// Small helpers (given). Whitespace here means: space, tab, \r, \f, \v. Only ASCII letters change case.
namespace detail {

inline constexpr std::string_view kSpace = " \t\r\f\v";

inline bool blank(std::string_view s) { return s.find_first_not_of(kSpace) == std::string_view::npos; }

inline std::string_view trimmed(std::string_view s) {
    const std::size_t first = s.find_first_not_of(kSpace);
    if (first == std::string_view::npos) return {};
    const std::size_t last = s.find_last_not_of(kSpace);
    return s.substr(first, last - first + 1);
}

inline char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }
inline char upper(char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c; }

}  // namespace detail

// ==== The stages =========================================================================================================================================
// TODO 1: every stage is a leaf class (final) that overrides name(), process() and clone() (`return std::make_unique<ThisClass>(*this);`).
//   Trim        "trim"        removes whitespace at both ends of the line.
//   Lowercase   "lower"       ASCII letters to lower case.        Uppercase  "upper"   to upper case.
//   DropBlank   "drop-blank"  drops lines that are empty or only whitespace; the others pass UNCHANGED (not trimmed).
//   Replace     "replace"     Replace(from, to): every non-overlapping occurrence of `from`, left to right, becomes `to` (the replacement is NOT
//                             scanned again: "a" -> "aa" doubles once). An empty `from`: std::invalid_argument in the constructor.
//   NumberLines "number"      NumberLines(start = 1), STATEFUL: "N: " in front of every line that reaches it; N starts at `start` and counts up.
//   Dedupe      "dedupe"      STATEFUL: drops a line that equals the previous line that reached it ("a a b a" -> "a b a").
//   Limit       "limit"       Limit(n), STATEFUL: passes the first n lines that reach it, drops all the others.
//   Reject      "reject"      Reject(needle): a line that contains `needle` makes it throw std::runtime_error; the others pass. An empty
//                             `needle`: std::invalid_argument in the constructor.
class Trim final : public Stage {
public:
    std::string name() const override { return ""; }
    std::optional<std::string> process(std::string line) override { return line; }
    std::unique_ptr<Stage> clone() const override { return nullptr; }
};

class Lowercase final : public Stage {
public:
    std::string name() const override { return ""; }
    std::optional<std::string> process(std::string line) override { return line; }
    std::unique_ptr<Stage> clone() const override { return nullptr; }
};

class Uppercase final : public Stage {
public:
    std::string name() const override { return ""; }
    std::optional<std::string> process(std::string line) override { return line; }
    std::unique_ptr<Stage> clone() const override { return nullptr; }
};

class DropBlank final : public Stage {
public:
    std::string name() const override { return ""; }
    std::optional<std::string> process(std::string line) override { return line; }
    std::unique_ptr<Stage> clone() const override { return nullptr; }
};

class Replace final : public Stage {
public:
    Replace(std::string from, std::string to) {
        (void)from;
        (void)to;
    }
    std::string name() const override { return ""; }
    std::optional<std::string> process(std::string line) override { return line; }
    std::unique_ptr<Stage> clone() const override { return nullptr; }
};

class NumberLines final : public Stage {
public:
    explicit NumberLines(int start = 1) { (void)start; }
    std::string name() const override { return ""; }
    std::optional<std::string> process(std::string line) override { return line; }
    std::unique_ptr<Stage> clone() const override { return nullptr; }
};

class Dedupe final : public Stage {
public:
    std::string name() const override { return ""; }
    std::optional<std::string> process(std::string line) override { return line; }
    std::unique_ptr<Stage> clone() const override { return nullptr; }
};

class Limit final : public Stage {
public:
    explicit Limit(std::size_t n) { (void)n; }
    std::string name() const override { return ""; }
    std::optional<std::string> process(std::string line) override { return line; }
    std::unique_ptr<Stage> clone() const override { return nullptr; }
};

class Reject final : public Stage {
public:
    explicit Reject(std::string needle) { (void)needle; }
    std::string name() const override { return ""; }
    std::optional<std::string> process(std::string line) override { return line; }
    std::unique_ptr<Stage> clone() const override { return nullptr; }
};

// ==== The pipeline ======================================================================================================================================
// TODO 2: a Pipeline is a recipe: an ordered list of stages, std::vector<std::unique_ptr<Stage>>. It is COPYABLE (a deep copy: clone every stage)
//         and movable; run() never changes it.
//   add(stage)   takes ownership; a null pointer is std::invalid_argument; returns *this so calls chain.
//   emplace<S>(args...)   builds an S in place and adds it (S must derive from Stage: the signature says so); returns *this.
//   size(), names()       the number of stages, and their name() values in order.
//   run(text)             splits the text at '\n' (a final '\n' does not start another line: "a\n" is one line, "" is none, "\n" is one empty line)
//                         and sends every line through fresh CLONES of the stages in order (so that stateful stages start over on every run
//                         and two runs are independent). A stage that returns nothing drops the line (the later stages never see it). A stage that
//                         throws a std::exception drops the line too and adds a StageError (input line number, stage name, what()); the other
//                         lines carry on.
class Pipeline {
public:
    Pipeline() = default;
    Pipeline(const Pipeline& other) { (void)other; }  // TODO: a deep copy (clone every stage); the compiler cannot copy a vector of unique_ptr
    Pipeline(Pipeline&&) noexcept = default;
    Pipeline& operator=(const Pipeline& other) {  // TODO: copy-and-swap
        (void)other;
        return *this;
    }
    Pipeline& operator=(Pipeline&&) noexcept = default;

    Pipeline& add(std::unique_ptr<Stage> stage) {
        (void)stage;
        return *this;
    }
    template <std::derived_from<Stage> S, class... Args>
    Pipeline& emplace(Args&&... args) {
        (void)sizeof...(args);
        return *this;
    }

    std::size_t size() const { return 0; }
    std::vector<std::string> names() const { return {}; }
    Result run(std::string_view text) const {
        (void)text;
        return {};
    }

private:
    std::vector<std::unique_ptr<Stage>> stages_;
};

// ==== The registry: stages by name (the plugin point) ====================================================================================================
// TODO 3: a Registry maps a stage name to a FACTORY: a function that builds the stage from the text after the colon.
//   add(name, factory)   an empty name, an empty factory, or a name that is already registered: std::invalid_argument.
//   contains(name)       is this name registered?
//   make(spec)           spec is "name" or "name:arg" (split at the FIRST ':'); the factory gets the arg ("" for the first form). An unknown
//                        name is std::invalid_argument; so is a factory that returns a null pointer.
//   with_builtins()      a Registry with the nine stages above:
//       "trim", "lower", "upper", "drop-blank", "dedupe"   take NO argument (an argument is std::invalid_argument)
//       "number" or "number:5"                              the first number (default 1; negative numbers are fine)
//       "limit:3"                                           how many lines pass (a whole number is required)
//       "replace:from=>to"                                  split at the FIRST "=>"; `from` must not be empty
//       "reject:text"                                       the text must not be empty
//     A number must be ALL digits (with an optional leading '-' for number): "number:5x" is std::invalid_argument. (std::from_chars + check that
//     it consumed everything.)
class Registry {
public:
    using Factory = std::function<std::unique_ptr<Stage>(std::string_view arg)>;

    void add(std::string name, Factory factory) {
        (void)name;
        (void)factory;
    }
    bool contains(std::string_view name) const {
        (void)name;
        return false;
    }
    std::unique_ptr<Stage> make(std::string_view spec) const {
        (void)spec;
        return nullptr;
    }
    static Registry with_builtins() { return Registry{}; }

private:
    std::map<std::string, Factory, std::less<>> factories_;
};

// TODO 4: build_pipeline("trim | lower | number:5"): the specs separated by '|', blanks around each one ignored, each built by the registry.
//         A blank recipe is an empty pipeline. An empty spec between two bars ("trim||lower", or a trailing bar) is std::invalid_argument,
//         and so is anything the registry rejects.
inline Pipeline build_pipeline(const Registry& registry, std::string_view recipe) {
    (void)registry;
    (void)recipe;
    return Pipeline{};
}
