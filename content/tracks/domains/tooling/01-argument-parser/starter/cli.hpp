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

inline std::string describe(const Error& e) { return e.detail; }   // TODO

class Parsed {
public:
    bool flag(std::string_view) const { return false; }
    std::size_t count(std::string_view) const { return 0; }
    std::optional<std::string_view> value(std::string_view) const { return std::nullopt; }
    std::vector<std::string_view> values(std::string_view) const { return {}; }
    const std::vector<std::string>& positionals() const { return positionals_; }
    std::vector<std::string_view> positional(std::string_view) const { return {}; }

private:
    std::vector<std::string> positionals_;
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
    std::optional<std::string> check() const { return std::nullopt; }   // TODO
    Result parse(std::span<const std::string_view>) const { return Result{}; }   // TODO
    std::string usage() const { return ""; }   // TODO
    std::string help() const { return ""; }   // TODO

private:
    std::string program_, description_;
    std::vector<Option> options_;
    std::vector<Positional> positionals_;
};

inline std::optional<long long> to_int(std::string_view, long long, long long) { return std::nullopt; }   // TODO
inline std::optional<double> to_double(std::string_view) { return std::nullopt; }   // TODO

}  // namespace cli
