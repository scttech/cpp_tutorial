#define ZTH_TRACK_ALLOCATIONS
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "pipeline.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;
using Lines = std::vector<std::string>;

// A stage written by the TEST (a "plugin"): it counts its objects, its clones and its calls, and appends "!" to every line.
struct Spy : Stage {
    static inline int alive = 0, calls = 0, clones = 0;
    S label;
    explicit Spy(S l = "spy") : label(std::move(l)) { ++alive; }
    Spy(const Spy& other) : Stage(other), label(other.label) { ++alive; }
    ~Spy() override { --alive; }
    S name() const override { return label; }
    std::optional<S> process(S line) override {
        ++calls;
        return line + "!";
    }
    std::unique_ptr<Stage> clone() const override {
        ++clones;
        return std::make_unique<Spy>(*this);
    }
};

// Throws a std::invalid_argument for the line "boom", passes everything else.
struct Boom final : Stage {
    S name() const override { return "boom"; }
    std::optional<S> process(S line) override {
        if (line == "boom") throw std::invalid_argument("it went boom");
        return line;
    }
    std::unique_ptr<Stage> clone() const override { return std::make_unique<Boom>(*this); }
};

Lines run_lines(const Pipeline& p, std::string_view text) { return p.run(text).lines; }

// The text of a line that a stage kept (by value: a reference into a temporary optional would dangle). "<dropped>" if it was dropped.
S val(std::optional<S> o) { return o ? std::move(*o) : S("<dropped>"); }

template <class St, class... A>
std::optional<S> one(S line, A&&... args) {
    St stage(std::forward<A>(args)...);
    return stage.process(std::move(line));
}

template <class F>
bool throws_invalid(F f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

Pipeline recipe(std::string_view r) { return build_pipeline(Registry::with_builtins(), r); }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "Trim removes whitespace at both ends and nothing inside") {
    ZTH_EXPECT_EQ(val(one<Trim>("  a b  ")), S("a b"));
    ZTH_EXPECT_EQ(val(one<Trim>("\t x\r")), S("x"));
    ZTH_EXPECT_EQ(val(one<Trim>("x")), S("x"));
    ZTH_EXPECT_EQ(val(one<Trim>("   ")), S(""));
    ZTH_EXPECT_EQ(val(one<Trim>("")), S(""));
}
ZTH_TEST("works", "Lowercase and Uppercase change ASCII letters only") {
    ZTH_EXPECT_EQ(val(one<Lowercase>("Hello, WORLD 42")), S("hello, world 42"));
    ZTH_EXPECT_EQ(val(one<Uppercase>("Hello, world 42")), S("HELLO, WORLD 42"));
    ZTH_EXPECT_EQ(val(one<Uppercase>("\xC3\xA9")), S("\xC3\xA9"));  // bytes above 127 are left alone
    ZTH_EXPECT_EQ(val(one<Lowercase>("\xC3\x89")), S("\xC3\x89"));
    ZTH_EXPECT_EQ(val(one<Uppercase>("az AZ")), S("AZ AZ"));
    ZTH_EXPECT_EQ(val(one<Lowercase>("az AZ")), S("az az"));
}
ZTH_TEST("works", "DropBlank drops empty and whitespace-only lines and leaves the others untouched") {
    ZTH_EXPECT_FALSE(one<DropBlank>("").has_value());
    ZTH_EXPECT_FALSE(one<DropBlank>("  \t ").has_value());
    ZTH_EXPECT_EQ(val(one<DropBlank>("  x ")), S("  x "));
}
ZTH_TEST("works", "Replace replaces every occurrence, left to right") {
    ZTH_EXPECT_EQ(val(one<Replace>("a-b-c", "-", "+")), S("a+b+c"));
    ZTH_EXPECT_EQ(val(one<Replace>("none here", "xyz", "!")), S("none here"));
    ZTH_EXPECT_EQ(val(one<Replace>("hello world", "world", "there")), S("hello there"));
    ZTH_EXPECT_EQ(val(one<Replace>("abcabc", "abc", "")), S(""));
    ZTH_EXPECT_EQ(val(one<Replace>("", "a", "b")), S(""));
}
ZTH_TEST("works", "Replace: occurrences do not overlap, and the replacement is not scanned again") {
    ZTH_EXPECT_EQ(val(one<Replace>("aaa", "aa", "b")), S("ba"));
    ZTH_EXPECT_EQ(val(one<Replace>("aba", "a", "aa")), S("aabaa"));
    ZTH_EXPECT_EQ(val(one<Replace>("aaaa", "aa", "a")), S("aa"));
}
ZTH_TEST("works", "NumberLines numbers the lines that reach it, from 1 or from a start value") {
    NumberLines n;
    ZTH_EXPECT_EQ(val(n.process("a")), S("1: a"));
    ZTH_EXPECT_EQ(val(n.process("b")), S("2: b"));
    NumberLines m(10);
    ZTH_EXPECT_EQ(val(m.process("x")), S("10: x"));
    NumberLines neg(-1);
    ZTH_EXPECT_EQ(val(neg.process("p")), S("-1: p"));
    ZTH_EXPECT_EQ(val(neg.process("q")), S("0: q"));
}
ZTH_TEST("works", "Dedupe drops a line that equals the previous one that reached it") {
    Dedupe d;
    ZTH_EXPECT_EQ(val(d.process("a")), S("a"));
    ZTH_EXPECT_FALSE(d.process("a").has_value());
    ZTH_EXPECT_FALSE(d.process("a").has_value());
    ZTH_EXPECT_EQ(val(d.process("b")), S("b"));
    ZTH_EXPECT_EQ(val(d.process("a")), S("a"));  // not adjacent to the earlier a: kept
}
ZTH_TEST("works", "Limit passes the first n lines and drops the rest") {
    Limit l(2);
    ZTH_EXPECT_TRUE(l.process("a").has_value());
    ZTH_EXPECT_TRUE(l.process("b").has_value());
    ZTH_EXPECT_FALSE(l.process("c").has_value());
    ZTH_EXPECT_FALSE(l.process("d").has_value());
}
ZTH_TEST("works", "Reject throws std::runtime_error for a line that contains the text") {
    Reject r("bad");
    ZTH_EXPECT_EQ(val(r.process("all good")), S("all good"));
    try {
        r.process("this is bad news");
        ::zth::fail("Reject did not throw");
        return;
    } catch (const std::runtime_error& e) {
        ZTH_EXPECT_CONTAINS(S(e.what()), "bad");
    }
}
ZTH_TEST("works", "every stage has its name") {
    Pipeline p;
    p.emplace<Trim>().emplace<Lowercase>().emplace<Uppercase>().emplace<DropBlank>().emplace<Replace>("a", "b").emplace<NumberLines>().emplace<Dedupe>().emplace<Limit>(1).emplace<Reject>("x");
    ZTH_EXPECT_TRUE((p.names() == Lines{"trim", "lower", "upper", "drop-blank", "replace", "number", "dedupe", "limit", "reject"}));
    ZTH_EXPECT_EQ(p.size(), std::size_t{9});
}
ZTH_TEST("works", "a Pipeline runs its stages in order") {
    Pipeline p;
    p.emplace<Trim>().emplace<Uppercase>();
    ZTH_EXPECT_TRUE((run_lines(p, "  a  \n b ") == Lines{"A", "B"}));
    Pipeline q;
    q.emplace<Replace>("A", "x").emplace<Uppercase>();  // the order matters: replace first, then upper
    ZTH_EXPECT_TRUE((run_lines(q, "aA") == Lines{"AX"}));
    Pipeline r;
    r.emplace<Uppercase>().emplace<Replace>("A", "x");  // upper first: both letters are A when replace looks
    ZTH_EXPECT_TRUE((run_lines(r, "aA") == Lines{"xx"}));
}
ZTH_TEST("works", "run splits the text at newlines: a final newline starts no new line, empty lines count") {
    const Pipeline none;
    ZTH_EXPECT_TRUE((run_lines(none, "a\nb\nc") == Lines{"a", "b", "c"}));
    ZTH_EXPECT_TRUE((run_lines(none, "a\nb\n") == Lines{"a", "b"}));
    ZTH_EXPECT_TRUE((run_lines(none, "a\n\nb") == Lines{"a", "", "b"}));
    ZTH_EXPECT_TRUE((run_lines(none, "").empty()));
    ZTH_EXPECT_TRUE((run_lines(none, "\n") == Lines{""}));
    ZTH_EXPECT_TRUE((run_lines(none, "\n\n") == Lines{"", ""}));
    ZTH_EXPECT_TRUE((run_lines(none, "one") == Lines{"one"}));
}
ZTH_TEST("works", "a dropped line never reaches the later stages") {
    Spy::calls = 0;
    Pipeline p;
    p.emplace<DropBlank>().emplace<Spy>();
    const Result r = p.run("a\n\nb\n  \n");
    ZTH_EXPECT_EQ(Spy::calls, 2);
    ZTH_EXPECT_TRUE((r.lines == Lines{"a!", "b!"}));
}
ZTH_TEST("works", "a stage that throws drops the line and records an error; the other lines carry on") {
    Pipeline p;
    p.emplace<Boom>().emplace<Uppercase>();
    const Result r = p.run("one\nboom\nthree");
    ZTH_EXPECT_TRUE((r.lines == Lines{"ONE", "THREE"}));
    ZTH_EXPECT_EQ(r.errors.size(), std::size_t{1});
    ZTH_EXPECT_EQ(r.errors[0].line, std::size_t{2});
    ZTH_EXPECT_EQ(r.errors[0].stage, S("boom"));
    ZTH_EXPECT_EQ(r.errors[0].message, S("it went boom"));
}
ZTH_TEST("works", "add takes ownership and returns the pipeline, so calls chain") {
    Pipeline p;
    Pipeline& same = p.add(std::make_unique<Trim>()).add(std::make_unique<Lowercase>());
    ZTH_EXPECT_TRUE(&same == &p);
    ZTH_EXPECT_EQ(p.size(), std::size_t{2});
}
ZTH_TEST("works", "a stage written by someone else plugs in with emplace and add") {
    Spy::calls = 0;
    Pipeline p;
    p.emplace<Spy>("first").add(std::make_unique<Spy>("second"));
    ZTH_EXPECT_TRUE((p.names() == Lines{"first", "second"}));
    ZTH_EXPECT_TRUE((run_lines(p, "x") == Lines{"x!!"}));
}
ZTH_TEST("works", "Registry::with_builtins knows the nine stages") {
    const Registry r = Registry::with_builtins();
    for (const char* n : {"trim", "lower", "upper", "drop-blank", "dedupe", "number", "limit", "replace", "reject"}) ZTH_EXPECT_TRUE(r.contains(n));
    ZTH_EXPECT_FALSE(r.contains("nope"));
    ZTH_EXPECT_FALSE(r.contains(""));
}
ZTH_TEST("works", "Registry::make builds a stage from its spec") {
    const Registry r = Registry::with_builtins();
    ZTH_EXPECT_EQ(r.make("trim")->name(), S("trim"));
    ZTH_EXPECT_EQ(r.make("upper")->name(), S("upper"));
    auto n = r.make("number:7");
    ZTH_EXPECT_EQ(val(n->process("a")), S("7: a"));
    auto num = r.make("number");
    ZTH_EXPECT_EQ(val(num->process("a")), S("1: a"));
    auto lim = r.make("limit:1");
    ZTH_EXPECT_TRUE(lim->process("a").has_value());
    ZTH_EXPECT_FALSE(lim->process("b").has_value());
    auto rep = r.make("replace:cat=>dog");
    ZTH_EXPECT_EQ(val(rep->process("a cat")), S("a dog"));
    auto rej = r.make("reject:no");
    ZTH_EXPECT_THROWS(std::runtime_error, rej->process("no way"));
}
ZTH_TEST("works", "a plugin: register a new kind of stage and build it by name") {
    Registry r = Registry::with_builtins();
    r.add("spy", [](std::string_view arg) { return std::make_unique<Spy>(S(arg)); });
    ZTH_EXPECT_TRUE(r.contains("spy"));
    ZTH_EXPECT_EQ(r.make("spy:mine")->name(), S("mine"));
    const Pipeline p = build_pipeline(r, "trim | spy:s1 | upper");
    ZTH_EXPECT_TRUE((p.names() == Lines{"trim", "s1", "upper"}));
    ZTH_EXPECT_TRUE((run_lines(p, " a ") == Lines{"A!"}));
}
ZTH_TEST("works", "build_pipeline builds a pipeline from a recipe") {
    const Pipeline p = recipe("trim | lower | drop-blank | number");
    ZTH_EXPECT_TRUE((p.names() == Lines{"trim", "lower", "drop-blank", "number"}));
    ZTH_EXPECT_TRUE((run_lines(p, "  Hello \n\n WORLD") == Lines{"1: hello", "2: world"}));
}
ZTH_TEST("works", "Result::text joins the lines with newlines") {
    Result r;
    ZTH_EXPECT_EQ(r.text(), S(""));
    r.lines = {"a", "b"};
    ZTH_EXPECT_EQ(r.text(), S("a\nb"));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "every run starts with fresh stages: a numbering stage starts again, and the pipeline itself is unchanged") {
    Pipeline p;
    p.emplace<NumberLines>();
    ZTH_EXPECT_TRUE((run_lines(p, "a\nb") == Lines{"1: a", "2: b"}));
    ZTH_EXPECT_TRUE((run_lines(p, "a\nb") == Lines{"1: a", "2: b"}));
    ZTH_EXPECT_TRUE((run_lines(p, "x") == Lines{"1: x"}));
}
ZTH_TEST("robust", "stateful stages restart on every run: Dedupe, Limit") {
    Pipeline p;
    p.emplace<Dedupe>().emplace<Limit>(2);
    ZTH_EXPECT_TRUE((run_lines(p, "a\na\nb\nc") == Lines{"a", "b"}));
    ZTH_EXPECT_TRUE((run_lines(p, "a\na\nb\nc") == Lines{"a", "b"}));
}
ZTH_TEST("robust", "stage order with state: Limit before NumberLines and after it") {
    Pipeline a = recipe("limit:2 | number");
    Pipeline b = recipe("number | limit:2");
    ZTH_EXPECT_TRUE((run_lines(a, "x\ny\nz") == Lines{"1: x", "2: y"}));
    ZTH_EXPECT_TRUE((run_lines(b, "x\ny\nz") == Lines{"1: x", "2: y"}));
    Pipeline c = recipe("drop-blank | number");
    ZTH_EXPECT_TRUE((run_lines(c, "x\n\ny") == Lines{"1: x", "2: y"}));  // the dropped line takes no number
    Pipeline d = recipe("number | drop-blank");
    ZTH_EXPECT_TRUE((run_lines(d, "x\n\ny") == Lines{"1: x", "2: ", "3: y"}));  // a number makes the empty line non-blank
}
ZTH_TEST("robust", "clone makes a copy of the REAL stage with its current state") {
    NumberLines n(1);
    n.process("a");
    n.process("b");
    auto c = n.clone();
    ZTH_EXPECT_EQ(c->name(), S("number"));
    ZTH_EXPECT_EQ(val(c->process("x")), S("3: x"));  // the clone continues from the state of the original at the time of the copy
    ZTH_EXPECT_EQ(val(n.process("y")), S("3: y"));   // and the original is independent of it
    ZTH_EXPECT_EQ(val(c->process("z")), S("4: z"));
}
ZTH_TEST("robust", "clone of Dedupe and Limit keeps their state, independently") {
    Dedupe d;
    d.process("a");
    auto dc = d.clone();
    ZTH_EXPECT_FALSE(dc->process("a").has_value());
    ZTH_EXPECT_EQ(val(dc->process("b")), S("b"));
    ZTH_EXPECT_FALSE(d.process("a").has_value());  // the original still remembers "a"
    Limit l(1);
    l.process("a");
    auto lc = l.clone();
    ZTH_EXPECT_FALSE(lc->process("b").has_value());
    ZTH_EXPECT_FALSE(l.process("b").has_value());
}
ZTH_TEST("robust", "clone of the stateless and configured stages gives the same behavior") {
    Replace rep("a", "b");
    ZTH_EXPECT_EQ(val(rep.clone()->process("banana")), S("bbnbnb"));
    Reject rej("x");
    ZTH_EXPECT_THROWS(std::runtime_error, rej.clone()->process("axb"));
    ZTH_EXPECT_EQ(val(Trim().clone()->process(" t ")), S("t"));
    ZTH_EXPECT_EQ(val(Lowercase().clone()->process("AB")), S("ab"));
    ZTH_EXPECT_EQ(val(Uppercase().clone()->process("ab")), S("AB"));
    ZTH_EXPECT_FALSE(DropBlank().clone()->process(" ").has_value());
}
ZTH_TEST("robust", "a copy of a Pipeline is deep: the two are independent") {
    Pipeline a;
    a.emplace<Trim>();
    Pipeline b = a;
    b.emplace<Uppercase>();
    ZTH_EXPECT_EQ(a.size(), std::size_t{1});
    ZTH_EXPECT_EQ(b.size(), std::size_t{2});
    ZTH_EXPECT_TRUE((run_lines(a, " x ") == Lines{"x"}));
    ZTH_EXPECT_TRUE((run_lines(b, " x ") == Lines{"X"}));
}
ZTH_TEST("robust", "copy assignment replaces the stages; self-assignment is harmless") {
    Pipeline a = recipe("upper");
    Pipeline b = recipe("lower | number");
    b = a;
    ZTH_EXPECT_TRUE((b.names() == Lines{"upper"}));
    ZTH_EXPECT_TRUE((run_lines(b, "x") == Lines{"X"}));
    Pipeline& self = b;
    b = self;
    ZTH_EXPECT_TRUE((b.names() == Lines{"upper"}));
    ZTH_EXPECT_TRUE((run_lines(b, "y") == Lines{"Y"}));
}
ZTH_TEST("robust", "a Pipeline can be moved") {
    Pipeline a = recipe("trim | upper");
    Pipeline b = std::move(a);
    ZTH_EXPECT_TRUE((run_lines(b, " q ") == Lines{"Q"}));
    Pipeline c;
    c = std::move(b);
    ZTH_EXPECT_TRUE((c.names() == Lines{"trim", "upper"}));
    std::vector<Pipeline> v;
    v.push_back(recipe("lower"));
    v.push_back(std::move(c));
    ZTH_EXPECT_EQ(v.size(), std::size_t{2});
    ZTH_EXPECT_TRUE((run_lines(v[1], "  Z") == Lines{"Z"}));
}
ZTH_TEST("robust", "the stages are owned by the pipeline: destroyed with it, cloned for each run and for each copy") {
    Spy::alive = Spy::clones = 0;
    {
        Pipeline p;
        p.emplace<Spy>("a").emplace<Spy>("b");
        ZTH_EXPECT_EQ(Spy::alive, 2);
        p.run("x\ny\nz");
        ZTH_EXPECT_EQ(Spy::alive, 2);   // the clones made for the run are gone again
        ZTH_EXPECT_EQ(Spy::clones, 2);  // one clone per stage per run, not per line
        Pipeline copy = p;
        ZTH_EXPECT_EQ(Spy::alive, 4);
    }
    ZTH_EXPECT_EQ(Spy::alive, 0);
}
ZTH_TEST("robust", "no leaks or double frees through copies, moves, runs and a registry") {
    zth::mem::Scope scope;
    {
        Registry r = Registry::with_builtins();
        r.add("spy", [](std::string_view) { return std::make_unique<Spy>(); });
        Pipeline p = build_pipeline(r, "trim | spy | number | dedupe | limit:5");
        Pipeline q = p;
        Pipeline moved = std::move(q);
        p.run("a\nb\nc");
        moved.run("a\nb\nc");
        p = moved;
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "errors: the line number counts INPUT lines (also the dropped ones), in input order, with the stage that threw") {
    Pipeline p = recipe("drop-blank | reject:bad | upper");
    const Result r = p.run("fine\n\nbad one\nfine too\nbad two");
    ZTH_EXPECT_TRUE((r.lines == Lines{"FINE", "FINE TOO"}));
    ZTH_EXPECT_EQ(r.errors.size(), std::size_t{2});
    ZTH_EXPECT_EQ(r.errors[0].line, std::size_t{3});
    ZTH_EXPECT_EQ(r.errors[1].line, std::size_t{5});
    ZTH_EXPECT_EQ(r.errors[0].stage, S("reject"));
    ZTH_EXPECT_EQ(r.errors[0].message, S("line contains 'bad'"));
}
ZTH_TEST("robust", "a line that throws stops at that stage: the later stages never see it") {
    Spy::calls = 0;
    Pipeline p;
    p.emplace<Boom>().emplace<Spy>();
    const Result r = p.run("a\nboom\nb\nboom");
    ZTH_EXPECT_EQ(Spy::calls, 2);
    ZTH_EXPECT_EQ(r.errors.size(), std::size_t{2});
}
ZTH_TEST("robust", "any std::exception counts, not only std::runtime_error") {
    Pipeline p;
    p.emplace<Boom>();  // throws std::invalid_argument
    const Result r = p.run("boom");
    ZTH_EXPECT_TRUE(r.lines.empty());
    ZTH_EXPECT_EQ(r.errors.size(), std::size_t{1});
}
ZTH_TEST("robust", "an empty pipeline passes the lines through; an empty text gives nothing and no errors") {
    const Pipeline p;
    const Result r = p.run("a\n\nb");
    ZTH_EXPECT_TRUE((r.lines == Lines{"a", "", "b"}));
    ZTH_EXPECT_TRUE(r.errors.empty());
    ZTH_EXPECT_TRUE(p.run("").lines.empty());
    ZTH_EXPECT_EQ(p.size(), std::size_t{0});
    ZTH_EXPECT_TRUE(p.names().empty());
}
ZTH_TEST("robust", "add(nullptr) and invalid stage arguments are std::invalid_argument") {
    Pipeline p;
    ZTH_EXPECT_TRUE(throws_invalid([&] { p.add(nullptr); }));
    ZTH_EXPECT_EQ(p.size(), std::size_t{0});
    ZTH_EXPECT_TRUE(throws_invalid([] { Replace r("", "x"); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Reject r(""); }));
    ZTH_EXPECT_EQ(Replace("a", "").process("a")->size(), std::size_t{0});
}
ZTH_TEST("robust", "Registry::add rejects an empty name, an empty factory and a duplicate") {
    Registry r = Registry::with_builtins();
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.add("", [](std::string_view) { return std::make_unique<Trim>(); }); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.add("x", Registry::Factory{}); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.add("trim", [](std::string_view) { return std::make_unique<Trim>(); }); }));
    ZTH_EXPECT_FALSE(r.contains("x"));
    ZTH_EXPECT_EQ(r.make("trim")->name(), S("trim"));  // the original is still there
}
ZTH_TEST("robust", "Registry::make: unknown names and a factory that returns nothing") {
    Registry r = Registry::with_builtins();
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("nope"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make(""); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make(":x"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("Trim"); }));  // names are case sensitive
    r.add("null", [](std::string_view) -> std::unique_ptr<Stage> { return nullptr; });
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("null"); }));
}
ZTH_TEST("robust", "Registry::make splits at the FIRST colon; the factory gets the rest, colons included") {
    Registry r;
    S seen = "unset";
    r.add("probe", [&seen](std::string_view arg) {
        seen = S(arg);
        return std::make_unique<Spy>();
    });
    r.make("probe");
    ZTH_EXPECT_EQ(seen, S(""));
    r.make("probe:a:b");
    ZTH_EXPECT_EQ(seen, S("a:b"));
    r.make("probe:x=>y");
    ZTH_EXPECT_EQ(seen, S("x=>y"));
}
ZTH_TEST("robust", "stages without an argument refuse one") {
    const Registry r = Registry::with_builtins();
    for (const char* spec : {"trim:x", "lower:1", "upper:u", "drop-blank:x", "dedupe:2"}) ZTH_EXPECT_TRUE(throws_invalid([&] { r.make(spec); }));
}
ZTH_TEST("robust", "number and limit need whole numbers, and nothing else after the digits") {
    const Registry r = Registry::with_builtins();
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("number:5x"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("number:x"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("number: 5"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("limit"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("limit:"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("limit:-1"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("limit:2x"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("limit:1.5"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("limit:99999999999999999999999"); }));
    ZTH_EXPECT_EQ(val(r.make("number:-3")->process("a")), S("-3: a"));
    ZTH_EXPECT_EQ(val(r.make("number:0")->process("a")), S("0: a"));
    ZTH_EXPECT_FALSE(r.make("limit:0")->process("a").has_value());
}
ZTH_TEST("robust", "replace needs from=>to, splits at the first =>, and from must not be empty") {
    const Registry r = Registry::with_builtins();
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("replace"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("replace:abc"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("replace:=>x"); }));
    ZTH_EXPECT_EQ(val(r.make("replace:a=>")->process("banana")), S("bnn"));
    ZTH_EXPECT_EQ(val(r.make("replace:a=>b=>c")->process("a")), S("b=>c"));
    ZTH_EXPECT_EQ(val(r.make("replace:a:b=>X")->process("1 a:b 2")), S("1 X 2"));
    ZTH_EXPECT_EQ(val(r.make("replace: => ")->process("a b")), S("a b"));  // from = " ", to = " ": a space is a fine argument
}
ZTH_TEST("robust", "reject needs a non-empty text; a space is allowed inside it") {
    const Registry r = Registry::with_builtins();
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("reject"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { r.make("reject:"); }));
    ZTH_EXPECT_THROWS(std::runtime_error, r.make("reject:a b")->process("xa by"));
    ZTH_EXPECT_EQ(val(r.make("reject:a b")->process("ab")), S("ab"));
}
ZTH_TEST("robust", "build_pipeline: blanks around the specs are ignored, a blank recipe is an empty pipeline") {
    ZTH_EXPECT_EQ(recipe("").size(), std::size_t{0});
    ZTH_EXPECT_EQ(recipe("   ").size(), std::size_t{0});
    ZTH_EXPECT_TRUE((recipe("  trim|upper  ").names() == Lines{"trim", "upper"}));
    ZTH_EXPECT_TRUE((recipe("\ttrim \t|\t upper").names() == Lines{"trim", "upper"}));
    ZTH_EXPECT_TRUE((recipe("trim").names() == Lines{"trim"}));
    ZTH_EXPECT_TRUE((recipe("replace:a b=>c d | upper").names() == Lines{"replace", "upper"}));
}
ZTH_TEST("robust", "build_pipeline: an empty spec or an unknown or malformed one is std::invalid_argument") {
    const Registry r = Registry::with_builtins();
    ZTH_EXPECT_TRUE(throws_invalid([&] { build_pipeline(r, "trim||upper"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { build_pipeline(r, "|trim"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { build_pipeline(r, "trim|"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { build_pipeline(r, "trim | | upper"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { build_pipeline(r, "trim | nope"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { build_pipeline(r, "trim | limit:x"); }));
    ZTH_EXPECT_TRUE(throws_invalid([&] { build_pipeline(r, "|"); }));
}
ZTH_TEST("robust", "bytes above 127, \\r and a very long line go through the stages") {
    Pipeline p = recipe("trim | upper");
    ZTH_EXPECT_TRUE((run_lines(p, "  caf\xC3\xA9 \r\nok\r\n") == Lines{"CAF\xC3\xA9", "OK"}));
    const S big(100000, 'a');
    ZTH_EXPECT_EQ(run_lines(p, big)[0].size(), std::size_t{100000});
}
ZTH_TEST("robust", "a large text runs quickly through several stages") {
    S text;
    for (int i = 0; i < 20000; ++i) text += "  Line Number " + std::to_string(i % 50) + "  \n";
    Pipeline p = recipe("trim | lower | replace:line=>row | dedupe | number");
    const Result r = p.run(text);
    ZTH_EXPECT_EQ(r.lines.size(), std::size_t{20000});
    ZTH_EXPECT_EQ(r.lines.front(), S("1: row number 0"));
    ZTH_EXPECT_EQ(r.lines.back(), S("20000: row number 49"));
}
ZTH_TEST("robust", "the stages are leaf classes") {
    ZTH_EXPECT_TRUE(std::is_final_v<Trim> && std::is_final_v<Lowercase> && std::is_final_v<Uppercase> && std::is_final_v<DropBlank>);
    ZTH_EXPECT_TRUE(std::is_final_v<Replace> && std::is_final_v<NumberLines> && std::is_final_v<Dedupe> && std::is_final_v<Limit> && std::is_final_v<Reject>);
    ZTH_EXPECT_TRUE(std::is_abstract_v<Stage>);
    ZTH_EXPECT_TRUE(std::has_virtual_destructor_v<Stage>);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "pipeline.hpp does not use new, delete, malloc or free: unique_ptr and make_unique own everything") {
    ZTH_EXPECT_NO_RAW_MEMORY("pipeline.hpp");
}
ZTH_TEST("clean", "every override is marked override: at least 27 times (nine stages, three functions each)") {
    const S code = ::zth::strip_comments_and_literals(::zth::read_file("pipeline.hpp"));
    const std::size_t n = ::zth::find_word(code, "override").size();
    if (n < 27) {
        ::zth::fail("pipeline.hpp uses `override` " + std::to_string(n) + " time(s): the nine stages each override name, process and clone (27)");
        return;
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("clean", "`virtual` appears only in the Stage interface (4 times)") {
    const S code = ::zth::strip_comments_and_literals(::zth::read_file("pipeline.hpp"));
    const std::size_t n = ::zth::find_word(code, "virtual").size();
    if (n != 4) {
        ::zth::fail("pipeline.hpp uses `virtual` " + std::to_string(n) + " time(s): the Stage interface declares 4, the stages only say `override`");
        return;
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("clean", "no dynamic_cast or typeid: virtual functions and clone() are enough") {
    const S code = ::zth::strip_comments_and_literals(::zth::read_file("pipeline.hpp"));
    ZTH_EXPECT_EQ(::zth::find_word(code, "dynamic_cast").size(), std::size_t{0});
    ZTH_EXPECT_EQ(::zth::find_word(code, "typeid").size(), std::size_t{0});
}
ZTH_TEST("clean", "pipeline.hpp has no `using namespace`") {
    ZTH_EXPECT_FALSE(::zth::source_layout_of("pipeline.hpp").using_directive);
}
ZTH_TEST("robust", "Trim and DropBlank treat \f and \v as whitespace too") {
    ZTH_EXPECT_EQ(val(one<Trim>("\f\vx\v\f")), S("x"));
    ZTH_EXPECT_FALSE(one<DropBlank>("\f\v").has_value());
    ZTH_EXPECT_EQ(val(one<Trim>("\rx")), S("x"));
}
