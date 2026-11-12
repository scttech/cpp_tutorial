#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <string_view>
#include <vector>

#include "cli.hpp"
#include "zth_test.hpp"

using namespace cli;

namespace {

using Args = std::vector<std::string_view>;

Option flag(std::string name, char s = 0, bool repeatable = false, std::string help = "") { return Option{std::move(name), s, Kind::Flag, "", std::move(help), repeatable, false, std::nullopt}; }
Option value(std::string name, char s = 0, std::string vn = "VALUE", bool repeatable = false, bool required = false, std::optional<std::string> def = std::nullopt, std::string help = "") {
    return Option{std::move(name), s, Kind::Value, std::move(vn), std::move(help), repeatable, required, std::move(def)};
}

// the parser used in most tests
Parser make() {
    Parser p("tool", "Does things.");
    p.add(flag("verbose", 'v', true, "Be loud."));
    p.add(flag("quiet", 'q'));
    p.add(flag("all", 'a'));
    p.add(value("output", 'o', "FILE", false, false, std::nullopt, "Where to write."));
    p.add(value("count", 'n', "N", false, false, std::string("1")));
    p.add(value("include", 'I', "DIR", true));
    p.add(value("color", 0, "WHEN"));
    p.add(value("max-count", 'm', "N"));
    p.add(value("max-depth", 0, "N"));
    p.add(Positional{"input", true, false, "The input."});
    p.add(Positional{"rest", false, true, "More."});
    return p;
}

Result run(const Parser& p, const Args& a) { return p.parse(std::span<const std::string_view>(a.data(), a.size())); }
bool is_error(const Result& r, ErrorKind k, const std::string& detail) { return r.error && !r.parsed && r.error->kind == k && r.error->detail == detail; }
std::string sv(std::optional<std::string_view> v) { return v ? std::string(*v) : std::string("<none>"); }
bool contains(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "flags, values and positionals in their simple forms") {
    const Parser p = make();
    const Result r = run(p, {"-v", "--output", "out.txt", "in.txt", "extra1", "extra2"});
    ZTH_EXPECT_TRUE(r.parsed.has_value() && !r.error);
    ZTH_EXPECT_TRUE(r.parsed->flag("verbose") && !r.parsed->flag("quiet"));
    ZTH_EXPECT_EQ(sv(r.parsed->value("output")), std::string("out.txt"));
    ZTH_EXPECT_EQ(r.parsed->positionals().size(), std::size_t{3});
    ZTH_EXPECT_EQ(r.parsed->positional("input").size(), std::size_t{1});
    ZTH_EXPECT_EQ(std::string(r.parsed->positional("input")[0]), std::string("in.txt"));
    ZTH_EXPECT_EQ(r.parsed->positional("rest").size(), std::size_t{2});
    ZTH_EXPECT_EQ(std::string(r.parsed->positional("rest")[1]), std::string("extra2"));
    ZTH_EXPECT_TRUE(r.parsed->positional("nothing").empty());
    ZTH_EXPECT_EQ(r.parsed->count("nothing"), std::size_t{0});
    ZTH_EXPECT_FALSE(r.parsed->value("nothing").has_value());
}
ZTH_TEST("works", "long options: --name value, --name=value (also empty), exact names") {
    const Parser p = make();
    const Result a = run(p, {"--output=a.txt", "in"});
    ZTH_EXPECT_EQ(sv(a.parsed->value("output")), std::string("a.txt"));
    const Result b = run(p, {"--output=", "in"});
    ZTH_EXPECT_TRUE(b.parsed.has_value() && b.parsed->value("output").has_value() && b.parsed->value("output")->empty());
    const Result c = run(p, {"--output", "a=b", "in"});
    ZTH_EXPECT_EQ(sv(c.parsed->value("output")), std::string("a=b"));
    const Result d = run(p, {"--output=a=b=c", "in"});
    ZTH_EXPECT_EQ(sv(d.parsed->value("output")), std::string("a=b=c"));   // only the first '=' splits
    const Result e = run(p, {"--verbose", "--quiet", "in"});
    ZTH_EXPECT_TRUE(e.parsed->flag("verbose") && e.parsed->flag("quiet"));
}
ZTH_TEST("works", "short options: -o file, -ofile, -o=x (the value is '=x'), clusters of flags, a value option at the end of a cluster") {
    const Parser p = make();
    ZTH_EXPECT_EQ(sv(run(p, {"-o", "f", "in"}).parsed->value("output")), std::string("f"));
    ZTH_EXPECT_EQ(sv(run(p, {"-of", "in"}).parsed->value("output")), std::string("f"));
    ZTH_EXPECT_EQ(sv(run(p, {"-o=x", "in"}).parsed->value("output")), std::string("=x"));
    const Result cluster = run(p, {"-qav", "in"});
    ZTH_EXPECT_TRUE(cluster.parsed->flag("quiet") && cluster.parsed->flag("all") && cluster.parsed->flag("verbose"));
    const Result tail = run(p, {"-qao", "file", "in"});
    ZTH_EXPECT_TRUE(tail.parsed->flag("quiet") && tail.parsed->flag("all"));
    ZTH_EXPECT_EQ(sv(tail.parsed->value("output")), std::string("file"));
    const Result glued = run(p, {"-qaofile", "in"});
    ZTH_EXPECT_EQ(sv(glued.parsed->value("output")), std::string("file"));
    const Result greedy = run(p, {"-ov", "in"});   // 'v' is the VALUE of -o here, not the verbose flag
    ZTH_EXPECT_EQ(sv(greedy.parsed->value("output")), std::string("v"));
    ZTH_EXPECT_FALSE(greedy.parsed->flag("verbose"));
}
ZTH_TEST("works", "a value may start with a dash; a lone '-' is a positional; so is an empty argument") {
    const Parser p = make();
    const Result a = run(p, {"--output", "-x", "in"});
    ZTH_EXPECT_EQ(sv(a.parsed->value("output")), std::string("-x"));
    const Result b = run(p, {"-o", "--quiet", "in"});
    ZTH_EXPECT_EQ(sv(b.parsed->value("output")), std::string("--quiet"));
    ZTH_EXPECT_FALSE(b.parsed->flag("quiet"));
    const Result c = run(p, {"-", "-"});
    ZTH_EXPECT_TRUE(c.parsed.has_value() && c.parsed->positionals().size() == 2 && c.parsed->positionals()[0] == "-");
    const Result d = run(p, {"", "x"});
    ZTH_EXPECT_TRUE(d.parsed.has_value() && d.parsed->positionals().size() == 2 && d.parsed->positionals()[0].empty());
}
ZTH_TEST("works", "'--' ends option processing; later dashed arguments are positionals; a second '--' is data") {
    const Parser p = make();
    const Result r = run(p, {"-v", "--", "-q", "--output", "--", "x"});
    ZTH_EXPECT_TRUE(r.parsed.has_value());
    ZTH_EXPECT_TRUE(r.parsed->flag("verbose") && !r.parsed->flag("quiet") && !r.parsed->value("output").has_value());
    ZTH_EXPECT_EQ(r.parsed->positionals().size(), std::size_t{4});
    ZTH_EXPECT_TRUE(r.parsed->positionals()[0] == "-q" && r.parsed->positionals()[1] == "--output" && r.parsed->positionals()[2] == "--" && r.parsed->positionals()[3] == "x");
    const Result only = run(p, {"--"});
    ZTH_EXPECT_TRUE(is_error(only, ErrorKind::MissingPositional, "input"));
}
ZTH_TEST("works", "negative numbers are positionals (unless the parser has a digit option)") {
    Parser p("calc", "");
    p.add(flag("verbose", 'v'));
    p.add(Positional{"numbers", true, true, ""});
    const Result r = run(p, {"-5", "-3.14", "-.5", "7", "-v"});
    ZTH_EXPECT_TRUE(r.parsed.has_value());
    ZTH_EXPECT_EQ(r.parsed->positionals().size(), std::size_t{4});
    ZTH_EXPECT_TRUE(r.parsed->positionals()[0] == "-5" && r.parsed->positionals()[1] == "-3.14" && r.parsed->positionals()[2] == "-.5" && r.parsed->flag("verbose"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"-1x"}), ErrorKind::UnknownOption, "-1"));   // not a number: treated as a cluster
    ZTH_EXPECT_TRUE(is_error(run(p, {"-5.5.5"}), ErrorKind::UnknownOption, "-5"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"-."}), ErrorKind::UnknownOption, "-."));
    Parser q("tool", "");
    q.add(flag("one", '1'));
    q.add(Positional{"rest", false, true, ""});
    ZTH_EXPECT_TRUE(is_error(run(q, {"-1", "-2"}), ErrorKind::UnknownOption, "-2"));   // with a digit option, "-2" is no longer a number but an unknown option
    const Result e = run(q, {"-1"});
    ZTH_EXPECT_TRUE(e.parsed.has_value() && e.parsed->flag("one") && e.parsed->positionals().empty());
}
ZTH_TEST("works", "unique prefixes of long options are accepted; exact names win; ambiguity is an error") {
    const Parser p = make();
    ZTH_EXPECT_EQ(sv(run(p, {"--outp=x", "in"}).parsed->value("output")), std::string("x"));
    ZTH_EXPECT_EQ(sv(run(p, {"--o", "x", "in"}).parsed->value("output")), std::string("x"));
    ZTH_EXPECT_EQ(sv(run(p, {"--max-c=3", "in"}).parsed->value("max-count")), std::string("3"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"--max=3", "in"}), ErrorKind::AmbiguousOption, "--max"));   // max-count and max-depth
    ZTH_EXPECT_TRUE(is_error(run(p, {"--c", "x", "in"}), ErrorKind::AmbiguousOption, "--c"));    // count and color
    ZTH_EXPECT_TRUE(is_error(run(p, {"--zzz", "in"}), ErrorKind::UnknownOption, "--zzz"));
    Parser q("t", "");
    q.add(flag("log", 'l'));
    q.add(flag("log-file", 'f'));
    ZTH_EXPECT_TRUE(run(q, {"--log"}).parsed->flag("log") && !run(q, {"--log"}).parsed->flag("log-file"));   // "--log" is an exact match even though it prefixes "--log-file"
    ZTH_EXPECT_TRUE(run(q, {"--log-"}).parsed->flag("log-file"));
    ZTH_EXPECT_TRUE(run(q, {"--"}).parsed.has_value());
    ZTH_EXPECT_TRUE(is_error(run(q, {"--=x"}), ErrorKind::UnknownOption, "--"));   // an empty name matches nothing
}
ZTH_TEST("works", "repeatable options collect every value in order; the last wins for value(); flags count") {
    const Parser p = make();
    const Result r = run(p, {"-I", "a", "-Ib", "--include=c", "--include", "d", "-vvv", "-v", "in"});
    ZTH_EXPECT_TRUE(r.parsed.has_value());
    const auto inc = r.parsed->values("include");
    ZTH_EXPECT_EQ(inc.size(), std::size_t{4});
    ZTH_EXPECT_TRUE(inc[0] == "a" && inc[1] == "b" && inc[2] == "c" && inc[3] == "d");
    ZTH_EXPECT_EQ(sv(r.parsed->value("include")), std::string("d"));
    ZTH_EXPECT_EQ(r.parsed->count("include"), std::size_t{4});
    ZTH_EXPECT_EQ(r.parsed->count("verbose"), std::size_t{4});
    ZTH_EXPECT_TRUE(r.parsed->values("output").empty());
}
ZTH_TEST("works", "defaults are reported when the option is absent, but never counted as given") {
    const Parser p = make();
    const Result r = run(p, {"in"});
    ZTH_EXPECT_EQ(sv(r.parsed->value("count")), std::string("1"));
    ZTH_EXPECT_EQ(r.parsed->count("count"), std::size_t{0});
    ZTH_EXPECT_TRUE(r.parsed->values("count").empty());
    ZTH_EXPECT_FALSE(r.parsed->flag("count"));
    const Result s = run(p, {"-n", "5", "in"});
    ZTH_EXPECT_EQ(sv(s.parsed->value("count")), std::string("5"));
    ZTH_EXPECT_FALSE(r.parsed->value("output").has_value());
}
ZTH_TEST("works", "errors: each kind, with the offending text as the detail") {
    const Parser p = make();
    ZTH_EXPECT_TRUE(is_error(run(p, {"--bogus", "in"}), ErrorKind::UnknownOption, "--bogus"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"--bogus=1", "in"}), ErrorKind::UnknownOption, "--bogus"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"-z", "in"}), ErrorKind::UnknownOption, "-z"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"-qz", "in"}), ErrorKind::UnknownOption, "-z"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"in", "--output"}), ErrorKind::MissingValue, "--output"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"in", "-o"}), ErrorKind::MissingValue, "-o"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"in", "-qo"}), ErrorKind::MissingValue, "-o"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"--verbose=yes", "in"}), ErrorKind::UnexpectedValue, "--verbose"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"--output", "a", "-o", "b", "in"}), ErrorKind::DuplicateOption, "--output"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"-q", "--quiet", "in"}), ErrorKind::DuplicateOption, "--quiet"));
    ZTH_EXPECT_TRUE(is_error(run(p, {}), ErrorKind::MissingPositional, "input"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"-v"}), ErrorKind::MissingPositional, "input"));
    Parser q("t", "");
    q.add(Positional{"a", true, false, ""});
    q.add(Positional{"b", false, false, ""});
    ZTH_EXPECT_TRUE(is_error(run(q, {"1", "2", "3", "4"}), ErrorKind::TooManyPositionals, "3"));
    ZTH_EXPECT_TRUE(run(q, {"1"}).parsed.has_value() && run(q, {"1"}).parsed->positional("b").empty());
}
ZTH_TEST("works", "required options and the first problem wins") {
    Parser p("t", "");
    p.add(value("pattern", 'e', "P", false, true));
    p.add(flag("verbose", 'v'));
    p.add(Positional{"file", true, false, ""});
    ZTH_EXPECT_TRUE(is_error(run(p, {"f"}), ErrorKind::MissingRequiredOption, "--pattern"));
    ZTH_EXPECT_TRUE(is_error(run(p, {}), ErrorKind::MissingRequiredOption, "--pattern"));   // before the missing positional
    ZTH_EXPECT_TRUE(run(p, {"-e", "x", "f"}).parsed.has_value());
    ZTH_EXPECT_TRUE(is_error(run(p, {"-z", "--bogus"}), ErrorKind::UnknownOption, "-z"));   // an option problem is found while scanning, before the final checks
    ZTH_EXPECT_TRUE(is_error(run(p, {"--verbose=1", "-z"}), ErrorKind::UnexpectedValue, "--verbose"));
}
ZTH_TEST("works", "describe() mentions what went wrong and the culprit") {
    for (ErrorKind k : {ErrorKind::UnknownOption, ErrorKind::AmbiguousOption, ErrorKind::MissingValue, ErrorKind::UnexpectedValue, ErrorKind::DuplicateOption, ErrorKind::MissingRequiredOption, ErrorKind::MissingPositional, ErrorKind::TooManyPositionals}) {
        const std::string text = describe(Error{k, "THE-CULPRIT"});
        ZTH_EXPECT_TRUE(contains(text, "THE-CULPRIT") && text.size() > 14);
        ZTH_EXPECT_TRUE(text.find('\n') == std::string::npos);
    }
    std::vector<std::string> texts;
    for (ErrorKind k : {ErrorKind::UnknownOption, ErrorKind::AmbiguousOption, ErrorKind::MissingValue, ErrorKind::UnexpectedValue, ErrorKind::DuplicateOption, ErrorKind::MissingRequiredOption, ErrorKind::MissingPositional, ErrorKind::TooManyPositionals}) texts.push_back(describe(Error{k, "x"}));
    for (std::size_t i = 0; i < texts.size(); ++i)
        for (std::size_t j = i + 1; j < texts.size(); ++j) ZTH_EXPECT_FALSE(texts[i] == texts[j]);   // every kind has its own wording
}
ZTH_TEST("works", "check() accepts a sound definition and names the problem in every kind of broken one") {
    ZTH_EXPECT_FALSE(make().check().has_value());
    auto problem = [](const Parser& p) { return p.check().value_or(std::string()); };
    Parser empty("t", "");
    empty.add(flag(""));
    ZTH_EXPECT_FALSE(problem(empty).empty());
    Parser dup("t", "");
    dup.add(flag("alpha"));
    dup.add(flag("alpha"));
    ZTH_EXPECT_TRUE(contains(problem(dup), "alpha"));
    Parser dup_short("t", "");
    dup_short.add(flag("alpha", 'a'));
    dup_short.add(flag("beta", 'a'));
    ZTH_EXPECT_TRUE(contains(problem(dup_short), "a"));
    Parser dashed("t", "");
    dashed.add(flag("-bad"));
    ZTH_EXPECT_TRUE(contains(problem(dashed), "-bad"));
    Parser eq("t", "");
    eq.add(flag("a=b"));
    ZTH_EXPECT_TRUE(contains(problem(eq), "a=b"));
    Parser sp("t", "");
    sp.add(flag("a b"));
    ZTH_EXPECT_TRUE(contains(problem(sp), "a b"));
    Parser bad_short("t", "");
    bad_short.add(flag("x", '-'));
    ZTH_EXPECT_FALSE(problem(bad_short).empty());
    Parser eq_short("t", "");
    eq_short.add(flag("x", '='));
    ZTH_EXPECT_FALSE(problem(eq_short).empty());
    Parser flag_default("t", "");
    flag_default.add(Option{"f", 0, Kind::Flag, "", "", false, false, std::string("yes")});
    ZTH_EXPECT_TRUE(contains(problem(flag_default), "f"));
    Parser required_default("t", "");
    required_default.add(value("r", 0, "V", false, true, std::string("d")));
    ZTH_EXPECT_TRUE(contains(problem(required_default), "r"));
    Parser variadic("t", "");
    variadic.add(Positional{"many", false, true, ""});
    variadic.add(Positional{"after", false, false, ""});
    ZTH_EXPECT_TRUE(contains(problem(variadic), "many"));
    Parser order("t", "");
    order.add(Positional{"opt", false, false, ""});
    order.add(Positional{"req", true, false, ""});
    ZTH_EXPECT_TRUE(contains(problem(order), "req"));
    Parser noname("t", "");
    noname.add(Positional{"", true, false, ""});
    ZTH_EXPECT_FALSE(problem(noname).empty());
    Parser two_variadics("t", "");
    two_variadics.add(Positional{"v1", false, true, ""});
    two_variadics.add(Positional{"v2", false, true, ""});
    ZTH_EXPECT_FALSE(problem(two_variadics).empty());
}
ZTH_TEST("works", "usage() and help() have the documented layout") {
    Parser p("minigrep", "Search files for a pattern.");
    p.add(flag("ignore-case", 'i', false, "Ignore case."));
    p.add(value("max-count", 'm', "N", false, false, std::string("100"), "Stop after N matches."));
    p.add(value("exclude", 0, "GLOB", true, false, std::nullopt, "Skip files."));
    p.add(value("pattern", 'e', "PATTERN", false, true, std::nullopt, "What to look for."));
    p.add(value("a-rather-long-option-name", 0, "VALUE", false, false, std::nullopt, "Wraps."));
    p.add(value("tag", 't', "TAG", true, false, std::string("x"), "Labels."));
    p.add(value("input-file", 'f', "FILE", true, true, std::nullopt, "Inputs."));
    p.add(value("nineteen-chars-opt1", 'k', "V", false, false, std::nullopt, "Twenty-nine columns."));
    p.add(value("eighteen-chars-opt", 'j', "V", false, false, std::nullopt, "Twenty-eight columns."));
    p.add(Positional{"dir", true, false, "Where."});
    p.add(Positional{"files", false, true, "Files to search."});
    ZTH_EXPECT_EQ(p.usage(), std::string("Usage: minigrep [options] <dir> [files]..."));
    const std::string expected =
        "Usage: minigrep [options] <dir> [files]...\n"
        "\n"
        "Search files for a pattern.\n"
        "\n"
        "Options:\n"
        "  -i, --ignore-case           Ignore case.\n"
        "  -m, --max-count N           Stop after N matches. [default: 100]\n"
        "      --exclude GLOB          Skip files. (repeatable)\n"
        "  -e, --pattern PATTERN       What to look for. (required)\n"
        "      --a-rather-long-option-name VALUE\n"
        "                              Wraps.\n"
        "  -t, --tag TAG               Labels. [default: x] (repeatable)\n"
        "  -f, --input-file FILE       Inputs. (repeatable) (required)\n"
        "  -k, --nineteen-chars-opt1 V\n"
        "                              Twenty-nine columns.\n"
        "  -j, --eighteen-chars-opt V  Twenty-eight columns.\n"
        "\n"
        "Arguments:\n"
        "  dir                         Where.\n"
        "  files                       Files to search.\n";
    ZTH_EXPECT_EQ(p.help(), expected);
    ZTH_EXPECT_EQ(Parser("solo", "Alone.").usage(), std::string("Usage: solo"));
    Parser q("x", "D.");
    q.add(Positional{"must", true, true, ""});
    ZTH_EXPECT_EQ(q.usage(), std::string("Usage: x <must>..."));
    ZTH_EXPECT_EQ(Parser("solo", "Alone.").help(), std::string("Usage: solo\n\nAlone.\n"));
}
ZTH_TEST("works", "to_int: whole decimal integers within a range, nothing else") {
    ZTH_EXPECT_EQ(to_int("42", 0, 100).value_or(-1), 42LL);
    ZTH_EXPECT_EQ(to_int("-7", -10, 10).value_or(0), -7LL);
    ZTH_EXPECT_EQ(to_int("0", 0, 0).value_or(-1), 0LL);
    ZTH_EXPECT_EQ(to_int("100", 0, 100).value_or(-1), 100LL);
    ZTH_EXPECT_FALSE(to_int("101", 0, 100).has_value());
    ZTH_EXPECT_FALSE(to_int("-1", 0, 100).has_value());
    for (const char* bad : {"", "+5", " 5", "5 ", "5x", "x5", "0x10", "1e3", "5.0", "--5", "-", "+", "1,000", "٣"}) ZTH_EXPECT_FALSE(to_int(bad, -1000000, 1000000).has_value());
    ZTH_EXPECT_EQ(to_int("007", 0, 100).value_or(-1), 7LL);
    ZTH_EXPECT_EQ(to_int("9223372036854775807", 0, 9223372036854775807LL).value_or(0), 9223372036854775807LL);
    ZTH_EXPECT_FALSE(to_int("9223372036854775808", -9223372036854775807LL - 1, 9223372036854775807LL).has_value());   // overflow, not wrap-around
    ZTH_EXPECT_FALSE(to_int("99999999999999999999999", 0, 9223372036854775807LL).has_value());
    ZTH_EXPECT_EQ(to_int("-9223372036854775808", -9223372036854775807LL - 1, 9223372036854775807LL).value_or(0), -9223372036854775807LL - 1);
}
ZTH_TEST("works", "to_double: finite numbers only, whole text") {
    ZTH_EXPECT_TRUE(to_double("2.5").has_value() && *to_double("2.5") == 2.5);
    ZTH_EXPECT_TRUE(to_double("-0.125").has_value() && *to_double("-0.125") == -0.125);
    ZTH_EXPECT_TRUE(to_double("1e3").has_value() && *to_double("1e3") == 1000.0);
    ZTH_EXPECT_TRUE(to_double(".5").has_value() && *to_double(".5") == 0.5);
    ZTH_EXPECT_TRUE(to_double("3").has_value() && *to_double("3") == 3.0);
    for (const char* bad : {"", "+1", " 1", "1 ", "1x", "abc", "inf", "-inf", "nan", "infinity", "1e999", "-1e999", "0x10", "1,5", "--1", "-"}) ZTH_EXPECT_FALSE(to_double(bad).has_value());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "4000 random argument lists: exactly one of result and error, and the parsed object is always consistent") {
    zth::Lcg rng(2024);
    const Parser p = make();
    const std::vector<std::string> pool = {"-v", "-q", "-a", "-qa", "-vv", "-o", "-ofile", "-o=x", "--output", "--output=o", "--verbose", "--quiet", "--quiet=1", "--max=1", "--max-c=2", "--max-count", "--ou", "--outputs",
                                           "-n", "-n5", "--count=3", "-I", "-Idir", "--include", "--include=d", "--", "-", "x", "y", "file.txt", "-5", "-1.5", "--unknown", "-z", "-qz", "--col", "--color=red", "", "=", "-=", "--="};
    for (int round = 0; round < 4000; ++round) {
        Args args;
        std::vector<std::string> storage;
        const int n = rng.below(8);
        storage.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) storage.push_back(pool[static_cast<std::size_t>(rng.below(static_cast<int>(pool.size())))]);
        for (const std::string& s : storage) args.push_back(s);
        const Result r = run(p, args);
        ZTH_EXPECT_TRUE(r.parsed.has_value() != r.error.has_value());
        if (r.parsed) {
            // consistency: counts match values, positionals are all assigned, every positional comes from the arguments
            ZTH_EXPECT_EQ(r.parsed->positionals().size(), r.parsed->positional("input").size() + r.parsed->positional("rest").size());
            ZTH_EXPECT_EQ(r.parsed->count("include"), r.parsed->values("include").size());
            ZTH_EXPECT_EQ(r.parsed->count("output") <= 1, true);
            ZTH_EXPECT_EQ(r.parsed->positional("input").size(), std::size_t{1});
        } else {
            ZTH_EXPECT_FALSE(r.error->detail.empty());
        }
    }
}
ZTH_TEST("robust", "every well-formed combination of clusters and values gives the same parse as the spelled-out long form") {
    const Parser p = make();
    const Result a = run(p, {"-vqaofile", "-n", "7", "-Ix", "-I", "y", "in", "r1"});
    const Result b = run(p, {"--verbose", "--quiet", "--all", "--output=file", "--count", "7", "--include=x", "--include", "y", "in", "r1"});
    ZTH_EXPECT_TRUE(a.parsed.has_value() && b.parsed.has_value());
    for (const char* name : {"verbose", "quiet", "all", "output", "count", "include", "color", "max-count"}) {
        ZTH_EXPECT_EQ(a.parsed->count(name), b.parsed->count(name));
        ZTH_EXPECT_EQ(sv(a.parsed->value(name)), sv(b.parsed->value(name)));
        ZTH_EXPECT_TRUE(a.parsed->values(name) == b.parsed->values(name));
    }
    ZTH_EXPECT_TRUE(a.parsed->positionals() == b.parsed->positionals());
}
ZTH_TEST("robust", "the first problem is reported, and options are processed left to right (a missing value swallows the next argument)") {
    const Parser p = make();
    ZTH_EXPECT_TRUE(is_error(run(p, {"--bogus", "-z", "--output"}), ErrorKind::UnknownOption, "--bogus"));
    ZTH_EXPECT_TRUE(is_error(run(p, {"-q", "-q", "--bogus"}), ErrorKind::DuplicateOption, "--quiet"));
    const Result swallowed = run(p, {"-o", "--bogus", "in"});   // "--bogus" is the VALUE of -o
    ZTH_EXPECT_TRUE(swallowed.parsed.has_value() && sv(swallowed.parsed->value("output")) == "--bogus");
    ZTH_EXPECT_TRUE(is_error(run(p, {"a", "b", "--output"}), ErrorKind::MissingValue, "--output"));
}
ZTH_TEST("robust", "unusual but legal definitions: only options, only positionals, many positionals, a variadic that gets nothing") {
    Parser flags_only("t", "");
    flags_only.add(flag("x", 'x'));
    ZTH_EXPECT_TRUE(run(flags_only, {"-x"}).parsed.has_value());
    ZTH_EXPECT_TRUE(is_error(run(flags_only, {"stray"}), ErrorKind::TooManyPositionals, "stray"));
    Parser positional_only("t", "");
    positional_only.add(Positional{"a", true, false, ""});
    positional_only.add(Positional{"b", true, false, ""});
    positional_only.add(Positional{"c", false, true, ""});
    ZTH_EXPECT_TRUE(is_error(run(positional_only, {"1"}), ErrorKind::MissingPositional, "b"));
    const Result three = run(positional_only, {"1", "2", "3", "4", "5"});
    ZTH_EXPECT_TRUE(three.parsed.has_value() && three.parsed->positional("c").size() == 3);
    const Result none = run(positional_only, {"1", "2"});
    ZTH_EXPECT_TRUE(none.parsed.has_value() && none.parsed->positional("c").empty());
    ZTH_EXPECT_EQ(Parser("t", "").parse(std::span<const std::string_view>()).parsed.has_value(), true);
}
ZTH_TEST("robust", "numbers: random integers round-trip, near-misses are rejected, the extremes are exact") {
    zth::Lcg rng(9);
    for (int i = 0; i < 2000; ++i) {
        const long long v = (static_cast<long long>(rng.next()) << 31 ^ rng.next()) * (rng.below(2) ? 1 : -1);
        const std::string s = std::to_string(v);
        ZTH_EXPECT_EQ(to_int(s, -9223372036854775807LL - 1, 9223372036854775807LL).value_or(0), v);
        ZTH_EXPECT_FALSE(to_int(s + "0x", -9223372036854775807LL - 1, 9223372036854775807LL).has_value());
        ZTH_EXPECT_FALSE(to_int(" " + s, -9223372036854775807LL - 1, 9223372036854775807LL).has_value());
        ZTH_EXPECT_FALSE(to_int("+" + s, -9223372036854775807LL - 1, 9223372036854775807LL).has_value());
        if (v > -1000000 && v < 1000000) ZTH_EXPECT_FALSE(to_int(s, v + 1, v + 5).has_value());
    }
    for (int i = 0; i < 1000; ++i) {
        const double v = (static_cast<int>(rng.next() % 2000001) - 1000000) / 64.0;
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.10g", v);
        const auto got = to_double(buf);
        ZTH_EXPECT_TRUE(got.has_value() && std::fabs(*got - v) <= 1e-6 * (1 + std::fabs(v)));
    }
}
ZTH_TEST("robust", "very long argument lists and very long arguments") {
    const Parser p = make();
    std::vector<std::string> storage;
    for (int i = 0; i < 20000; ++i) storage.push_back(i % 3 == 0 ? "-v" : i % 3 == 1 ? "-Idir" + std::to_string(i) : "file" + std::to_string(i));
    Args args;
    for (const std::string& s : storage) args.push_back(s);
    const Result r = run(p, args);
    ZTH_EXPECT_TRUE(r.parsed.has_value());
    ZTH_EXPECT_EQ(r.parsed->count("verbose"), std::size_t{6667});
    ZTH_EXPECT_EQ(r.parsed->count("include"), std::size_t{6667});
    ZTH_EXPECT_EQ(r.parsed->positionals().size(), std::size_t{6666});
    const std::string huge(1000000, 'x');
    const Result h = run(p, {"--output", huge, huge});
    ZTH_EXPECT_TRUE(h.parsed.has_value() && h.parsed->value("output")->size() == 1000000 && h.parsed->positionals()[0].size() == 1000000);
    const std::string long_name = "--" + std::string(100000, 'q');
    ZTH_EXPECT_TRUE(is_error(run(p, {long_name}), ErrorKind::UnknownOption, long_name));
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "parsing 200000 arguments takes time proportional to their number (no quadratic prefix search, no re-scanning)") {
    const Parser p = make();
    std::vector<std::string> storage;
    for (int i = 0; i < 200000; ++i) storage.push_back(i % 4 == 0 ? "--verbose" : i % 4 == 1 ? "--include=somewhere/" + std::to_string(i) : i % 4 == 2 ? "-qx" : "file" + std::to_string(i));
    storage[2] = "--";   // everything after the first "--" is positional, so errors cannot end the run early
    Args args;
    for (const std::string& s : storage) args.push_back(s);
    const Result r = run(p, args);
    ZTH_EXPECT_TRUE(r.parsed.has_value());
    ZTH_EXPECT_TRUE(r.parsed->positionals().size() > 199000);
}
ZTH_TEST("fast", "a typical command line allocates only what it must: a bounded number of allocations for 30 arguments") {
    const Parser p = make();
    Args args = {"-v", "-o", "out.txt", "--count=3", "-Ia", "-Ib", "--include=c", "in.txt"};
    for (int i = 0; i < 22; ++i) args.push_back("file.txt");
    zth::mem::Scope scope;
    const Result r = run(p, args);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 120);
    ZTH_EXPECT_TRUE(r.parsed.has_value());
    zth::mem::Scope s2;
    std::size_t total = 0;
    for (int i = 0; i < 1000; ++i) total += r.parsed->count("include") + r.parsed->value("output")->size() + r.parsed->flag("quiet") + (r.parsed->value("count") ? 1 : 0);
    ZTH_EXPECT_ALLOCS_AT_MOST(s2, 0);   // looking things up allocates nothing
    ZTH_EXPECT_TRUE(total > 0);
}
ZTH_TEST_TIMEOUT("fast", "help() and check() on a definition with 500 options", 10) {
    Parser big("big", "Many options.");
    for (int i = 0; i < 500; ++i) big.add(value("option-number-" + std::to_string(i), 0, "V", true, false, std::nullopt, "Help text " + std::to_string(i)));
    ZTH_EXPECT_FALSE(big.check().has_value());
    const std::string h = big.help();
    ZTH_EXPECT_TRUE(h.size() > 500 * 40);
    Args a = {"--option-number-499=x", "--option-number-4=7", "--option-number-4=8"};
    const Result r = run(big, a);
    ZTH_EXPECT_TRUE(r.parsed.has_value() && r.parsed->count("option-number-4") == 2);   // exact names win over the many prefix matches
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("cli.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("cli.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "numbers are parsed with from_chars: no stoi/stod/strtol/atoi/sscanf/istringstream, no regex, no getopt") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("cli.hpp"));
    for (const char* word : {"stoi", "stol", "stoll", "stod", "stof", "strtol", "strtoll", "strtod", "atoi", "atol", "atof", "sscanf", "istringstream", "stringstream", "regex", "getopt", "getopt_long", "boost"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "from_chars").empty());
}
