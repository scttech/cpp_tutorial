// MUTANT: a long name with a space is accepted
#pragma once
// YOUR COMMAND-LINE PARSER, following the GNU/POSIX conventions people expect (getopt_long). namespace cli.
//
// struct Option { std::string long_name; char short_name; Kind kind; std::string value_name; std::string help; bool repeatable; bool required; std::optional<std::string> default_value; }
//     long_name: what follows "--" (no leading dashes). short_name: the letter after "-" (0 = none). kind: Kind::Flag (no value) or Kind::Value.
//     repeatable: may appear more than once. required: must appear at least once. default_value: what value() reports when the option is absent (only for Kind::Value, only when not required).
// struct Positional { std::string name; bool required; bool variadic; std::string help; }     variadic: swallows all remaining positionals (must be the last one)
//
// Parser(program, description);  add(Option), add(Positional) in order. Positionals are filled in the order they were added.
// check() -> optional<string>      nullopt if the definition is sound, otherwise a one-line message that names the culprit:
//     an empty long name; a long or short name used twice; a long name containing '=' or a space or starting with '-'; a short name that is '-' or a space or '='; a Flag with a default or a value_name;
//     a required option with a default; a variadic positional that is not last; a required positional after an optional one; an empty positional name.
//
// parse(args) -> Result { optional<Parsed> parsed; optional<Error> error; }      args do NOT include the program name. Exactly one of the two is set.
//   The rules, in the order they are applied to each argument:
//   1. After a "--" argument everything is a positional (the "--" itself is dropped). A second "--" is then just a positional.
//   2. "-" alone is a positional (the convention for stdin). An empty argument is a positional. An argument that does not start with '-' is a positional.
//   3. "-5", "-3.14", "-.5": a '-' followed by a number is a positional (a negative number), unless the parser has a short option that is a digit.
//   4. "--name": exact match on a long name wins; otherwise a UNIQUE prefix of long names is accepted ("--max" for "--max-count"); several candidates -> AmbiguousOption; none -> UnknownOption.
//      "--name=value" gives a Value option its value (the value may be empty); on a Flag it is UnexpectedValue. "--name" for a Value option takes the NEXT argument as its value, whatever it looks like
//      ("--output -x" gives the value "-x"); if there is no next argument: MissingValue.
//   5. "-abc": a cluster of short options. Flags are consumed one by one; the first Value option takes the REST of the cluster as its value ("-ofile" is -o file, "-o=x" has the value "=x"),
//      or, if nothing is left, the next argument ("-vo file"). Unknown letter -> UnknownOption (detail "-x"). No next argument -> MissingValue.
//   6. An option that is not repeatable and appears twice -> DuplicateOption. A repeatable flag counts its occurrences ("-vvv" is 3).
//   After all arguments: a required option that never appeared -> MissingRequiredOption; then positionals are assigned: too few for the required ones -> MissingPositional (detail = the name);
//   too many -> TooManyPositionals (detail = the first extra argument). The FIRST problem found is reported.
//   Error { ErrorKind kind; std::string detail }: detail is the argument as the user typed it up to any '=' ("--colour", "-x"); for DuplicateOption, MissingRequiredOption: "--" + the long name; MissingPositional: the name.
// describe(Error) -> one human-readable line, e.g.  unknown option '--colour'  /  option '--output' needs a value  /  missing required option '--pattern'  (any wording that contains the detail is fine)
//
// Parsed:  flag(name) (count > 0), count(name), value(name) -> optional<string_view> (the LAST given value, else the default, else nullopt), values(name) -> vector<string_view> (every given value in order),
//          positionals() -> const vector<string>& (all positionals in order), positional(name) -> vector<string_view> (those assigned to that declared positional: 0 or 1 entries, or all for the variadic one).
//          name is the long name. Unknown names give count 0 / nullopt / empty.
//
// usage() -> "Usage: <program> [options] <req> [opt] <rest>..." ("[options]" only if there are options; a required positional is <name>, an optional [name], a variadic one gets "..." appended: <files>... / [files]...)
// help()  -> usage line, a blank line, the description, a blank line, "Options:", one line per option, in the order added. Each option line is:
//            "  " + ("-x, " or four spaces) + "--long" + (" VALUE_NAME" for Value options), padded with spaces to column 30, then the help text, then " [default: D]" if it has a default,
//            " (repeatable)" if repeatable, " (required)" if required. If the left part is longer than 28 characters the help goes on the next line, indented by 30 spaces.
//            A Positionals section ("Arguments:") follows after a blank line when there are positionals: "  name" padded to column 30, then the help text.
//
// to_int(text, min, max) -> optional<long long>      the whole text is a decimal integer with an optional leading '-' (no '+', no spaces, no "0x", no trailing junk, no empty string) in [min, max]
// to_double(text)        -> optional<double>         the whole text is a finite number as std::from_chars reads it ("1e3", ".5", "-2.5"); no '+', no spaces, no "inf"/"nan", nothing after it
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cli {

enum class Kind { Flag, Value };

struct Option {
    std::string long_name;
    char short_name = 0;
    Kind kind = Kind::Flag;
    std::string value_name;
    std::string help;
    bool repeatable = false;
    bool required = false;
    std::optional<std::string> default_value;
};

struct Positional {
    std::string name;
    bool required = true;
    bool variadic = false;
    std::string help;
};

enum class ErrorKind { UnknownOption, AmbiguousOption, MissingValue, UnexpectedValue, DuplicateOption, MissingRequiredOption, MissingPositional, TooManyPositionals };

struct Error {
    ErrorKind kind;
    std::string detail;
};

inline std::string describe(const Error& e) {
    switch (e.kind) {
        case ErrorKind::UnknownOption: return "unknown option '" + e.detail + "'";
        case ErrorKind::AmbiguousOption: return "option '" + e.detail + "' is ambiguous";
        case ErrorKind::MissingValue: return "option '" + e.detail + "' needs a value";
        case ErrorKind::UnexpectedValue: return "option '" + e.detail + "' does not take a value";
        case ErrorKind::DuplicateOption: return "option '" + e.detail + "' was given more than once";
        case ErrorKind::MissingRequiredOption: return "missing required option '" + e.detail + "'";
        case ErrorKind::MissingPositional: return "missing argument '" + e.detail + "'";
        case ErrorKind::TooManyPositionals: return "unexpected argument '" + e.detail + "'";
    }
    return e.detail;
}

class Parsed {
public:
    bool flag(std::string_view name) const { return count(name) > 0; }
    std::size_t count(std::string_view name) const {
        const Slot* s = find(name);
        return s ? s->values.size() : 0;
    }
    std::optional<std::string_view> value(std::string_view name) const {
        const Slot* s = find(name);
        if (!s) return std::nullopt;
        if (!s->values.empty()) return std::string_view(s->values.back());
        if (s->default_value) return std::string_view(*s->default_value);
        return std::nullopt;
    }
    std::vector<std::string_view> values(std::string_view name) const {
        std::vector<std::string_view> out;
        if (const Slot* s = find(name))
            for (const std::string& v : s->values) out.push_back(v);
        return out;
    }
    const std::vector<std::string>& positionals() const { return positionals_; }
    std::vector<std::string_view> positional(std::string_view name) const {
        std::vector<std::string_view> out;
        for (const auto& [n, vals] : assigned_)
            if (n == name)
                for (const std::string& v : vals) out.push_back(v);
        return out;
    }

private:
    friend class Parser;
    struct Slot {
        std::string name;
        std::optional<std::string> default_value;
        std::vector<std::string> values;   // one entry per occurrence (flags store an empty string)
    };
    const Slot* find(std::string_view name) const {
        for (const Slot& s : slots_)
            if (s.name == name) return &s;
        return nullptr;
    }
    std::vector<Slot> slots_;
    std::vector<std::string> positionals_;
    std::vector<std::pair<std::string, std::vector<std::string>>> assigned_;
};

struct Result {
    std::optional<Parsed> parsed;
    std::optional<Error> error;
};

class Parser {
public:
    Parser(std::string program, std::string description) : program_(std::move(program)), description_(std::move(description)) {}
    Parser& add(Option o) {
        options_.push_back(std::move(o));
        return *this;
    }
    Parser& add(Positional p) {
        positionals_.push_back(std::move(p));
        return *this;
    }

    std::optional<std::string> check() const {
        for (std::size_t i = 0; i < options_.size(); ++i) {
            const Option& o = options_[i];
            if (o.long_name.empty()) return "an option has an empty long name";
            if (o.long_name.front() == '-' || o.long_name.find('=') != std::string::npos ) return "the long name '" + o.long_name + "' must not start with '-' or contain '=' or spaces";
            if (o.short_name == '-' || o.short_name == ' ' || o.short_name == '=') return "the short name of '" + o.long_name + "' is not allowed";
            if (o.kind == Kind::Flag && (o.default_value || !o.value_name.empty())) return "the flag '" + o.long_name + "' has a default or a value name";
            if (o.required && o.default_value) return "the required option '" + o.long_name + "' has a default";
            for (std::size_t j = 0; j < i; ++j) {
                if (options_[j].long_name == o.long_name) return "the long name '" + o.long_name + "' is used twice";
                if (o.short_name != 0 && options_[j].short_name == o.short_name) return std::string("the short name '") + o.short_name + "' is used twice";
            }
        }
        bool seen_optional = false;
        for (std::size_t i = 0; i < positionals_.size(); ++i) {
            const Positional& p = positionals_[i];
            if (p.name.empty()) return "a positional argument has an empty name";
            if (p.variadic && i + 1 != positionals_.size()) return "the variadic positional '" + p.name + "' is not the last one";
            if (p.required && seen_optional) return "the required positional '" + p.name + "' follows an optional one";
            if (!p.required) seen_optional = true;
        }
        return std::nullopt;
    }

    Result parse(std::span<const std::string_view> args) const {
        Parsed out;
        for (const Option& o : options_) out.slots_.push_back({o.long_name, o.default_value, {}});
        bool only_positional = false;
        const bool digit_short = std::any_of(options_.begin(), options_.end(), [](const Option& o) { return o.short_name >= '0' && o.short_name <= '9'; });
        auto fail = [](ErrorKind k, std::string d) { return Result{std::nullopt, Error{k, std::move(d)}}; };

        // records one occurrence; returns an error if the option may not repeat
        auto record = [&](std::size_t idx, std::string value) -> std::optional<Error> {
            Parsed::Slot& slot = out.slots_[idx];
            if (!options_[idx].repeatable && !slot.values.empty()) return Error{ErrorKind::DuplicateOption, "--" + options_[idx].long_name};
            slot.values.push_back(std::move(value));
            return std::nullopt;
        };

        for (std::size_t i = 0; i < args.size(); ++i) {
            const std::string_view a = args[i];
            if (only_positional || a.size() < 2 || a[0] != '-' || (looks_numeric(a) && !digit_short)) {
                out.positionals_.emplace_back(a);
                continue;
            }
            if (a == "--") {
                only_positional = true;
                continue;
            }
            if (a[1] == '-') {   // --long[=value]
                const std::size_t eq = a.find('=');
                const std::string_view typed = a.substr(0, eq);
                const std::string_view name = typed.substr(2);
                bool ambiguous = false;
                const std::optional<std::size_t> idx = find_long(name, ambiguous);
                if (ambiguous) return fail(ErrorKind::AmbiguousOption, std::string(typed));
                if (!idx) return fail(ErrorKind::UnknownOption, std::string(typed));
                if (options_[*idx].kind == Kind::Flag) {
                    if (eq != std::string_view::npos) return fail(ErrorKind::UnexpectedValue, std::string(typed));
                    if (auto e = record(*idx, "")) return Result{std::nullopt, *e};
                } else if (eq != std::string_view::npos) {
                    if (auto e = record(*idx, std::string(a.substr(eq + 1)))) return Result{std::nullopt, *e};
                } else {
                    if (i + 1 >= args.size()) return fail(ErrorKind::MissingValue, std::string(typed));
                    if (auto e = record(*idx, std::string(args[++i]))) return Result{std::nullopt, *e};
                }
                continue;
            }
            for (std::size_t k = 1; k < a.size(); ++k) {   // -abc
                const std::optional<std::size_t> idx = find_short(a[k]);
                if (!idx) return fail(ErrorKind::UnknownOption, std::string("-") + a[k]);
                if (options_[*idx].kind == Kind::Flag) {
                    if (auto e = record(*idx, "")) return Result{std::nullopt, *e};
                    continue;
                }
                std::string value;
                if (k + 1 < a.size()) value = std::string(a.substr(k + 1));
                else if (i + 1 < args.size()) value = std::string(args[++i]);
                else return fail(ErrorKind::MissingValue, std::string("-") + a[k]);
                if (auto e = record(*idx, std::move(value))) return Result{std::nullopt, *e};
                break;
            }
        }

        for (std::size_t i = 0; i < options_.size(); ++i)
            if (options_[i].required && out.slots_[i].values.empty()) return fail(ErrorKind::MissingRequiredOption, "--" + options_[i].long_name);

        std::size_t next = 0;
        for (const Positional& p : positionals_) {
            std::vector<std::string> mine;
            if (p.variadic) {
                while (next < out.positionals_.size()) mine.push_back(out.positionals_[next++]);
            } else if (next < out.positionals_.size()) {
                mine.push_back(out.positionals_[next++]);
            }
            if (mine.empty() && p.required) return fail(ErrorKind::MissingPositional, p.name);
            out.assigned_.emplace_back(p.name, std::move(mine));
        }
        if (next < out.positionals_.size()) return fail(ErrorKind::TooManyPositionals, out.positionals_[next]);
        return Result{std::move(out), std::nullopt};
    }

    std::string usage() const {
        std::string s = "Usage: " + program_;
        if (!options_.empty()) s += " [options]";
        for (const Positional& p : positionals_) {
            s += ' ';
            s += p.required ? "<" + p.name + ">" : "[" + p.name + "]";
            if (p.variadic) s += "...";
        }
        return s;
    }

    std::string help() const {
        std::string s = usage() + "\n\n" + description_ + "\n";
        if (!options_.empty()) {
            s += "\nOptions:\n";
            for (const Option& o : options_) {
                std::string left = "  ";
                left += o.short_name != 0 ? std::string("-") + o.short_name + ", " : "    ";
                left += "--" + o.long_name;
                if (o.kind == Kind::Value) left += " " + o.value_name;
                std::string text = o.help;
                if (o.default_value) text += " [default: " + *o.default_value + "]";
                if (o.repeatable) text += " (repeatable)";
                if (o.required) text += " (required)";
                s += column(left, text);
            }
        }
        if (!positionals_.empty()) {
            s += "\nArguments:\n";
            for (const Positional& p : positionals_) s += column("  " + p.name, p.help);
        }
        return s;
    }

private:
    static std::string column(const std::string& left, const std::string& text) {
        if (left.size() > 28) return left + "\n" + std::string(30, ' ') + text + "\n";
        return left + std::string(30 - left.size(), ' ') + text + "\n";
    }

    // '-' followed by a number: digits with an optional fraction ("-5", "-3.14", "-.5")
    static bool looks_numeric(std::string_view a) {
        std::size_t i = 1;
        std::size_t digits = 0;
        while (i < a.size() && a[i] >= '0' && a[i] <= '9') ++i, ++digits;
        if (i < a.size() && a[i] == '.') {
            ++i;
            while (i < a.size() && a[i] >= '0' && a[i] <= '9') ++i, ++digits;
        }
        return digits > 0 && i == a.size();
    }

    std::optional<std::size_t> find_short(char c) const {
        for (std::size_t i = 0; i < options_.size(); ++i)
            if (options_[i].short_name != 0 && options_[i].short_name == c) return i;
        return std::nullopt;
    }

    std::optional<std::size_t> find_long(std::string_view name, bool& ambiguous) const {
        std::optional<std::size_t> found;
        for (std::size_t i = 0; i < options_.size(); ++i)
            if (options_[i].long_name == name) return i;   // an exact match wins over any prefix match
        for (std::size_t i = 0; i < options_.size(); ++i) {
            if (!name.empty() && options_[i].long_name.compare(0, name.size(), name) == 0) {
                if (found) {
                    ambiguous = true;
                    return std::nullopt;
                }
                found = i;
            }
        }
        return found;
    }

    std::string program_, description_;
    std::vector<Option> options_;
    std::vector<Positional> positionals_;
};

inline std::optional<long long> to_int(std::string_view text, long long min, long long max) {
    if (text.empty() || text.front() == '+') return std::nullopt;
    long long v = 0;
    const auto r = std::from_chars(text.data(), text.data() + text.size(), v, 10);
    if (r.ec != std::errc() || r.ptr != text.data() + text.size()) return std::nullopt;
    if (v < min || v > max) return std::nullopt;
    return v;
}

inline std::optional<double> to_double(std::string_view text) {
    if (text.empty() || text.front() == '+') return std::nullopt;
    double v = 0;
    const auto r = std::from_chars(text.data(), text.data() + text.size(), v);
    if (r.ec != std::errc() || r.ptr != text.data() + text.size() || !std::isfinite(v)) return std::nullopt;
    return v;
}

}  // namespace cli
