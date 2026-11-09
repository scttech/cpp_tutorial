// MUTANT: n + 1.
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
class Trim final : public Stage {
public:
    std::string name() const override { return "trim"; }
    std::optional<std::string> process(std::string line) override { return std::string(detail::trimmed(line)); }
    std::unique_ptr<Stage> clone() const override { return std::make_unique<Trim>(*this); }
};

class Lowercase final : public Stage {
public:
    std::string name() const override { return "lower"; }
    std::optional<std::string> process(std::string line) override {
        for (char& c : line) c = detail::lower(c);
        return line;
    }
    std::unique_ptr<Stage> clone() const override { return std::make_unique<Lowercase>(*this); }
};

class Uppercase final : public Stage {
public:
    std::string name() const override { return "upper"; }
    std::optional<std::string> process(std::string line) override {
        for (char& c : line) c = detail::upper(c);
        return line;
    }
    std::unique_ptr<Stage> clone() const override { return std::make_unique<Uppercase>(*this); }
};

// Drops lines that are empty or only whitespace; the others pass UNCHANGED (not trimmed).
class DropBlank final : public Stage {
public:
    std::string name() const override { return "drop-blank"; }
    std::optional<std::string> process(std::string line) override {
        if (detail::blank(line)) return std::nullopt;
        return line;
    }
    std::unique_ptr<Stage> clone() const override { return std::make_unique<DropBlank>(*this); }
};

// Every non-overlapping occurrence of `from`, left to right, becomes `to` (the replacement is not scanned again).
class Replace final : public Stage {
public:
    Replace(std::string from, std::string to) : from_(std::move(from)), to_(std::move(to)) {
        if (from_.empty()) throw std::invalid_argument("replace: the text to find must not be empty");
    }
    std::string name() const override { return "replace"; }
    std::optional<std::string> process(std::string line) override {
        std::string out;
        std::size_t pos = 0;
        for (std::size_t hit; (hit = line.find(from_, pos)) != std::string::npos; pos = hit + from_.size()) {
            out.append(line, pos, hit - pos);
            out += to_;
        }
        out.append(line, pos, std::string::npos);
        return out;
    }
    std::unique_ptr<Stage> clone() const override { return std::make_unique<Replace>(*this); }

private:
    std::string from_;
    std::string to_;
};

// Stateful: "N: " in front of every line that reaches it; N starts at `start` and counts up.
class NumberLines final : public Stage {
public:
    explicit NumberLines(int start = 1) : next_(start) {}
    std::string name() const override { return "number"; }
    std::optional<std::string> process(std::string line) override { return std::to_string(next_++) + ": " + line; }
    std::unique_ptr<Stage> clone() const override { return std::make_unique<NumberLines>(*this); }

private:
    int next_;
};

// Stateful: drops a line that is equal to the previous line that reached it.
class Dedupe final : public Stage {
public:
    std::string name() const override { return "dedupe"; }
    std::optional<std::string> process(std::string line) override {
        const bool same = previous_ && *previous_ == line;
        previous_ = line;
        if (same) return std::nullopt;
        return line;
    }
    std::unique_ptr<Stage> clone() const override { return std::make_unique<Dedupe>(*this); }

private:
    std::optional<std::string> previous_;
};

// Stateful: passes the first n lines that reach it and drops all the others.
class Limit final : public Stage {
public:
    explicit Limit(std::size_t n) : remaining_(n + 1) {}
    std::string name() const override { return "limit"; }
    std::optional<std::string> process(std::string line) override {
        if (remaining_ == 0) return std::nullopt;
        --remaining_;
        return line;
    }
    std::unique_ptr<Stage> clone() const override { return std::make_unique<Limit>(*this); }

private:
    std::size_t remaining_;
};

// Throws std::runtime_error for a line that contains `needle`; the others pass unchanged.
class Reject final : public Stage {
public:
    explicit Reject(std::string needle) : needle_(std::move(needle)) {
        if (needle_.empty()) throw std::invalid_argument("reject: the text to look for must not be empty");
    }
    std::string name() const override { return "reject"; }
    std::optional<std::string> process(std::string line) override {
        if (line.find(needle_) != std::string::npos) throw std::runtime_error("line contains '" + needle_ + "'");
        return line;
    }
    std::unique_ptr<Stage> clone() const override { return std::make_unique<Reject>(*this); }

private:
    std::string needle_;
};

// ==== The pipeline ======================================================================================================================================
// A recipe: an ordered list of stages. run() never changes the pipeline: it works on fresh clones of the stages, so two runs are independent.
class Pipeline {
public:
    Pipeline() = default;
    Pipeline(const Pipeline& other) { copy_stages_from(other); }
    Pipeline(Pipeline&&) noexcept = default;
    Pipeline& operator=(const Pipeline& other) {
        Pipeline copy(other);  // copy first, then swap: an exception leaves *this unchanged
        stages_.swap(copy.stages_);
        return *this;
    }
    Pipeline& operator=(Pipeline&&) noexcept = default;

    // Takes ownership; a null pointer is std::invalid_argument. Returns *this to chain calls.
    Pipeline& add(std::unique_ptr<Stage> stage) {
        if (!stage) throw std::invalid_argument("a pipeline cannot hold a null stage");
        stages_.push_back(std::move(stage));
        return *this;
    }
    template <std::derived_from<Stage> S, class... Args>
    Pipeline& emplace(Args&&... args) {
        return add(std::make_unique<S>(std::forward<Args>(args)...));
    }

    std::size_t size() const { return stages_.size(); }
    std::vector<std::string> names() const {
        std::vector<std::string> out;
        out.reserve(stages_.size());
        for (const auto& s : stages_) out.push_back(s->name());
        return out;
    }

    // Splits the text at '\n' (a final '\n' does not start another line) and sends every line through the stages in order. A stage that returns
    // nothing drops the line; a stage that throws std::exception drops it too and adds a StageError. The other lines carry on.
    Result run(std::string_view text) const {
        std::vector<std::unique_ptr<Stage>> fresh;
        fresh.reserve(stages_.size());
        for (const auto& s : stages_) fresh.push_back(s->clone());

        Result result;
        std::size_t line_number = 0;
        for (std::size_t pos = 0; pos < text.size();) {
            std::size_t end = text.find('\n', pos);
            if (end == std::string_view::npos) end = text.size();
            std::string line(text.substr(pos, end - pos));
            pos = end + 1;
            ++line_number;

            std::optional<std::string> current = std::move(line);
            for (const auto& stage : fresh) {
                try {
                    current = stage->process(std::move(*current));
                } catch (const std::exception& e) {
                    result.errors.push_back({line_number, stage->name(), e.what()});
                    current.reset();
                }
                if (!current) break;
            }
            if (current) result.lines.push_back(std::move(*current));
        }
        return result;
    }

private:
    void copy_stages_from(const Pipeline& other) {
        stages_.reserve(other.stages_.size());
        for (const auto& s : other.stages_) stages_.push_back(s->clone());
    }

    std::vector<std::unique_ptr<Stage>> stages_;
};

// ==== The registry: stages by name (the plugin point) ====================================================================================================
// "trim", "lower", "upper", "drop-blank", "dedupe"      no argument
// "number" or "number:5"                                 the first number (default 1)
// "limit:3"                                              how many lines pass
// "replace:from=>to"                                     split at the FIRST "=>"; `from` must not be empty
// "reject:text"                                          the text must not be empty
// Anything else, a missing or malformed argument, an argument for a stage that takes none: std::invalid_argument.
class Registry {
public:
    using Factory = std::function<std::unique_ptr<Stage>(std::string_view arg)>;

    // A new kind of stage. An empty name, an empty factory or a name that is already registered: std::invalid_argument.
    void add(std::string name, Factory factory) {
        if (name.empty() || !factory) throw std::invalid_argument("a stage needs a name and a factory");
        if (!factories_.emplace(std::move(name), std::move(factory)).second) throw std::invalid_argument("this stage name is already registered");
    }
    bool contains(std::string_view name) const { return factories_.find(name) != factories_.end(); }

    // "name" or "name:arg" (split at the first ':'). The factory gets "" for the first form.
    std::unique_ptr<Stage> make(std::string_view spec) const {
        const std::size_t colon = spec.find(':');
        const std::string_view name = spec.substr(0, colon);
        const std::string_view arg = colon == std::string_view::npos ? std::string_view() : spec.substr(colon + 1);
        const auto it = factories_.find(name);
        if (it == factories_.end()) throw std::invalid_argument("unknown stage: " + std::string(name));
        std::unique_ptr<Stage> stage = it->second(arg);
        if (!stage) throw std::invalid_argument("the factory of '" + std::string(name) + "' returned nothing");
        return stage;
    }

    static Registry with_builtins() {
        Registry r;
        r.add("trim", no_argument<Trim>("trim"));
        r.add("lower", no_argument<Lowercase>("lower"));
        r.add("upper", no_argument<Uppercase>("upper"));
        r.add("drop-blank", no_argument<DropBlank>("drop-blank"));
        r.add("dedupe", no_argument<Dedupe>("dedupe"));
        r.add("number", [](std::string_view arg) -> std::unique_ptr<Stage> {
            if (arg.empty()) return std::make_unique<NumberLines>();
            return std::make_unique<NumberLines>(whole_number<int>(arg, "number"));
        });
        r.add("limit", [](std::string_view arg) -> std::unique_ptr<Stage> { return std::make_unique<Limit>(whole_number<std::size_t>(arg, "limit")); });
        r.add("replace", [](std::string_view arg) -> std::unique_ptr<Stage> {
            const std::size_t arrow = arg.find("=>");
            if (arrow == std::string_view::npos) throw std::invalid_argument("replace needs from=>to");
            return std::make_unique<Replace>(std::string(arg.substr(0, arrow)), std::string(arg.substr(arrow + 2)));
        });
        r.add("reject", [](std::string_view arg) -> std::unique_ptr<Stage> { return std::make_unique<Reject>(std::string(arg)); });
        return r;
    }

private:
    template <class S>
    static Factory no_argument(std::string name) {
        return [name = std::move(name)](std::string_view arg) -> std::unique_ptr<Stage> {
            if (!arg.empty()) throw std::invalid_argument(name + " takes no argument");
            return std::make_unique<S>();
        };
    }
    template <class T>
    static T whole_number(std::string_view text, const char* what) {
        T value{};
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (text.empty() || ec != std::errc{} || end != text.data() + text.size()) throw std::invalid_argument(std::string(what) + " needs a whole number");
        return value;
    }

    std::map<std::string, Factory, std::less<>> factories_;
};

// "trim | lower | number:5": the specs separated by '|', blanks around each one ignored. A blank recipe is an empty pipeline;
// an empty spec between two bars ("trim||lower", or a trailing bar) is std::invalid_argument, and so is anything the registry rejects.
inline Pipeline build_pipeline(const Registry& registry, std::string_view recipe) {
    Pipeline pipeline;
    if (detail::blank(recipe)) return pipeline;
    for (std::size_t pos = 0;;) {
        std::size_t bar = recipe.find('|', pos);
        const bool last = bar == std::string_view::npos;
        if (last) bar = recipe.size();
        const std::string_view spec = detail::trimmed(recipe.substr(pos, bar - pos));
        if (spec.empty()) throw std::invalid_argument("an empty stage in the recipe");
        pipeline.add(registry.make(spec));
        if (last) break;
        pos = bar + 1;
    }
    return pipeline;
}
