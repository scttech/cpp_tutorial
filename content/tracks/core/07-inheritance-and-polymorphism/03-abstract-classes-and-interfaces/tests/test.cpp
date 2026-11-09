#define ZTH_TRACK_ALLOCATIONS
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "logging.hpp"
#include "zth_test.hpp"

namespace {

using Lines = std::vector<std::string>;

// A sink written by the TEST: it only implements the two pure virtual functions, and counts its own destruction.
struct Spy : Sink {
    static inline int alive = 0;
    std::vector<std::pair<Level, std::string>> seen;
    Spy() { ++alive; }
    ~Spy() { --alive; }
    void write(Level level, std::string_view message) override { seen.emplace_back(level, std::string(message)); }
    std::string format(Level level, std::string_view message) const override { return Sink::format(level, message); }
};

// A flushable sink written by the test (two bases, from the learner's two interfaces)
struct FlushSpy : Sink, Flushable {
    int flushes = 0;
    void write(Level, std::string_view) override {}
    std::string format(Level level, std::string_view message) const override { return Sink::format(level, message); }
    void flush() override { ++flushes; }
};

// Is the interface member reachable? (False, not an error, when the class does not have it.)
template <class T>
constexpr bool can_flush = requires(T& t) { t.flush(); };

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "to_string of the levels") {
    ZTH_EXPECT_EQ(std::string(to_string(Level::Debug)), std::string("DEBUG"));
    ZTH_EXPECT_EQ(std::string(to_string(Level::Error)), std::string("ERROR"));
}
ZTH_TEST("works", "Sink::format is the default format: [LEVEL] message") {
    Spy s;
    ZTH_EXPECT_EQ(s.format(Level::Warn, "disk low"), std::string("[WARN] disk low"));
    ZTH_EXPECT_EQ(s.format(Level::Info, ""), std::string("[INFO] "));
}
ZTH_TEST("works", "MemorySink keeps the formatted lines in order") {
    MemorySink m;
    m.write(Level::Info, "started");
    m.write(Level::Error, "failed");
    ZTH_EXPECT_TRUE((m.lines() == Lines{"[INFO] started", "[ERROR] failed"}));
}
ZTH_TEST("works", "JsonSink formats one JSON object per line") {
    JsonSink j;
    j.write(Level::Warn, "disk low");
    ZTH_EXPECT_TRUE((j.lines() == Lines{"{\"level\":\"WARN\",\"message\":\"disk low\"}"}));
}
ZTH_TEST("works", "JsonSink escapes quotes, backslashes and newlines") {
    JsonSink j;
    j.write(Level::Info, "say \"hi\"\\ok\nnext");
    ZTH_EXPECT_EQ(j.lines().at(0), std::string("{\"level\":\"INFO\",\"message\":\"say \\\"hi\\\"\\\\ok\\nnext\"}"));
}
ZTH_TEST("works", "FilterSink forwards only levels at or above the minimum") {
    MemorySink target;
    FilterSink f(target, Level::Warn);
    f.write(Level::Debug, "d");
    f.write(Level::Info, "i");
    f.write(Level::Warn, "w");
    f.write(Level::Error, "e");
    ZTH_EXPECT_TRUE((target.lines() == Lines{"[WARN] w", "[ERROR] e"}));
}
ZTH_TEST("works", "BufferedSink holds messages back until flush") {
    MemorySink target;
    BufferedSink b(target);
    b.write(Level::Info, "one");
    b.write(Level::Info, "two");
    ZTH_EXPECT_EQ(b.pending(), std::size_t{2});
    ZTH_EXPECT_TRUE(target.lines().empty());
    b.flush();
    ZTH_EXPECT_EQ(b.pending(), std::size_t{0});
    ZTH_EXPECT_TRUE((target.lines() == Lines{"[INFO] one", "[INFO] two"}));
}
ZTH_TEST("works", "Logger writes every message to every sink, in the order they were added") {
    Logger log;
    auto& a = log.emplace<MemorySink>();
    auto& b = log.emplace<JsonSink>();
    log.log(Level::Warn, "x");
    log.info("y");
    ZTH_EXPECT_TRUE((a.lines() == Lines{"[WARN] x", "[INFO] y"}));
    ZTH_EXPECT_EQ(b.lines().size(), std::size_t{2});
    ZTH_EXPECT_EQ(log.size(), std::size_t{2});
}
ZTH_TEST("works", "debug, info, warn and error are shorthands for the four levels") {
    Logger log;
    auto& m = log.emplace<MemorySink>();
    log.debug("a");
    log.info("b");
    log.warn("c");
    log.error("d");
    ZTH_EXPECT_TRUE((m.lines() == Lines{"[DEBUG] a", "[INFO] b", "[WARN] c", "[ERROR] d"}));
}
ZTH_TEST("works", "add takes ownership of a sink written by someone else and returns it") {
    Logger log;
    auto spy = std::make_unique<Spy>();
    Spy* raw = spy.get();
    Sink& added = log.add(std::move(spy));
    ZTH_EXPECT_TRUE(&added == raw);
    log.error("boom");
    ZTH_EXPECT_EQ(raw->seen.size(), std::size_t{1});
    ZTH_EXPECT_TRUE(raw->seen[0].first == Level::Error);
    ZTH_EXPECT_EQ(raw->seen[0].second, std::string("boom"));
}
ZTH_TEST("works", "emplace returns the sink with its REAL type and passes the constructor arguments") {
    MemorySink target;
    Logger log;
    BufferedSink& b = log.emplace<BufferedSink>(target);
    FilterSink& f = log.emplace<FilterSink>(target, Level::Error);
    log.error("x");
    ZTH_EXPECT_EQ(b.pending(), std::size_t{1});  // a BufferedSink& (pending() exists only on the real type)
    ZTH_EXPECT_EQ(target.lines().size(), std::size_t{1});  // the filter let the error through
    (void)f;
}
ZTH_TEST("works", "flush_all flushes the Flushable sinks and says how many") {
    MemorySink target;
    Logger log;
    log.emplace<MemorySink>();
    auto& b = log.emplace<BufferedSink>(target);
    auto& fs = log.emplace<FlushSpy>();
    log.info("hello");
    ZTH_EXPECT_EQ(log.flush_all(), std::size_t{2});
    ZTH_EXPECT_EQ(b.pending(), std::size_t{0});
    ZTH_EXPECT_EQ(fs.flushes, 1);
    ZTH_EXPECT_TRUE((target.lines() == Lines{"[INFO] hello"}));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "Sink and Flushable are abstract interfaces; the concrete sinks are not") {
    ZTH_EXPECT_TRUE(std::is_abstract_v<Sink>);
    ZTH_EXPECT_TRUE(std::is_abstract_v<Flushable>);
    ZTH_EXPECT_FALSE(std::is_abstract_v<MemorySink>);
    ZTH_EXPECT_FALSE(std::is_abstract_v<JsonSink>);
    ZTH_EXPECT_FALSE(std::is_abstract_v<FilterSink>);
    ZTH_EXPECT_FALSE(std::is_abstract_v<BufferedSink>);
}
ZTH_TEST("robust", "write and format are BOTH pure virtual: implementing only one of them leaves a class abstract") {
    struct OnlyWrite : Sink {
        void write(Level, std::string_view) override {}
    };
    struct OnlyFormat : Sink {
        std::string format(Level, std::string_view) const override { return ""; }
    };
    ZTH_EXPECT_TRUE(std::is_abstract_v<OnlyWrite>);
    ZTH_EXPECT_TRUE(std::is_abstract_v<OnlyFormat>);
    ZTH_EXPECT_FALSE(std::is_abstract_v<Spy>);
}
ZTH_TEST("robust", "both interfaces have a virtual destructor") {
    ZTH_EXPECT_TRUE(std::has_virtual_destructor_v<Sink>);
    ZTH_EXPECT_TRUE(std::has_virtual_destructor_v<Flushable>);
}
ZTH_TEST("robust", "an interface is not copyable (no slicing), and neither are its sinks") {
    ZTH_EXPECT_FALSE(std::is_copy_constructible_v<Sink>);
    ZTH_EXPECT_FALSE(std::is_copy_assignable_v<Sink>);
    ZTH_EXPECT_FALSE(std::is_copy_constructible_v<MemorySink>);
    ZTH_EXPECT_FALSE(std::is_copy_constructible_v<BufferedSink>);
}
ZTH_TEST("robust", "interface segregation: only BufferedSink is Flushable; a Sink alone cannot flush") {
    ZTH_EXPECT_TRUE((std::is_base_of_v<Flushable, BufferedSink>));
    ZTH_EXPECT_FALSE((std::is_base_of_v<Flushable, MemorySink>));
    ZTH_EXPECT_FALSE((std::is_base_of_v<Flushable, JsonSink>));
    ZTH_EXPECT_FALSE((std::is_base_of_v<Flushable, FilterSink>));
    ZTH_EXPECT_FALSE((std::is_base_of_v<Flushable, Sink>));
    ZTH_EXPECT_FALSE((std::is_base_of_v<Sink, Flushable>));
    ZTH_EXPECT_FALSE(can_flush<Sink>);
    ZTH_EXPECT_FALSE(can_flush<MemorySink>);
    ZTH_EXPECT_TRUE(can_flush<BufferedSink>);
}
ZTH_TEST("robust", "JsonSink and BufferedSink are Sinks (public inheritance)") {
    ZTH_EXPECT_TRUE((std::is_convertible_v<JsonSink*, Sink*>));
    ZTH_EXPECT_TRUE((std::is_convertible_v<JsonSink*, MemorySink*>));
    ZTH_EXPECT_TRUE((std::is_convertible_v<BufferedSink*, Sink*>));
    ZTH_EXPECT_TRUE((std::is_convertible_v<BufferedSink*, Flushable*>));
    ZTH_EXPECT_TRUE((std::is_convertible_v<MemorySink*, Sink*>));
    ZTH_EXPECT_TRUE((std::is_convertible_v<FilterSink*, Sink*>));
}
ZTH_TEST("robust", "JsonSink is final") {
    ZTH_EXPECT_TRUE(std::is_final_v<JsonSink>);
}
ZTH_TEST("robust", "the Logger destroys the sinks it owns (virtual destructor), added or emplaced") {
    Spy::alive = 0;
    {
        Logger log;
        log.add(std::make_unique<Spy>());
        log.emplace<Spy>();
        log.add(std::make_unique<Spy>());
        ZTH_EXPECT_EQ(Spy::alive, 3);
    }
    ZTH_EXPECT_EQ(Spy::alive, 0);
}
ZTH_TEST("robust", "the Logger does not leak or double free") {
    zth::mem::Scope scope;
    {
        MemorySink target;
        Logger log;
        log.emplace<JsonSink>();
        log.emplace<BufferedSink>(target);
        log.add(std::make_unique<MemorySink>());
        log.info("x");
        log.flush_all();
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "add(nullptr) is std::invalid_argument and adds nothing") {
    Logger log;
    ZTH_EXPECT_THROWS(std::invalid_argument, log.add(nullptr));
    ZTH_EXPECT_EQ(log.size(), std::size_t{0});
}
ZTH_TEST("robust", "a sink written by someone else works with the decorators (the interface is all they need)") {
    Spy spy;
    FilterSink f(spy, Level::Info);
    BufferedSink b(f);
    b.write(Level::Debug, "no");
    b.write(Level::Info, "yes");
    b.flush();
    ZTH_EXPECT_EQ(spy.seen.size(), std::size_t{1});
    ZTH_EXPECT_EQ(spy.seen[0].second, std::string("yes"));
}
ZTH_TEST("robust", "FilterSink: exactly the minimum level passes, the one below does not") {
    Spy spy;
    FilterSink f(spy, Level::Info);
    f.write(Level::Debug, "d");
    ZTH_EXPECT_TRUE(spy.seen.empty());
    f.write(Level::Info, "i");
    ZTH_EXPECT_EQ(spy.seen.size(), std::size_t{1});
    FilterSink debug(spy, Level::Debug);
    debug.write(Level::Debug, "all");
    ZTH_EXPECT_EQ(spy.seen.size(), std::size_t{2});
}
ZTH_TEST("robust", "the decorators' format is the format of their target") {
    JsonSink target;
    FilterSink f(target, Level::Debug);
    BufferedSink b(target);
    ZTH_EXPECT_EQ(f.format(Level::Warn, "m"), target.format(Level::Warn, "m"));
    ZTH_EXPECT_EQ(b.format(Level::Warn, "m"), std::string("{\"level\":\"WARN\",\"message\":\"m\"}"));
    const Sink& as_sink = target;  // virtual: through a Sink reference too
    ZTH_EXPECT_EQ(as_sink.format(Level::Info, "m"), std::string("{\"level\":\"INFO\",\"message\":\"m\"}"));
}
ZTH_TEST("robust", "a decorator in front of a JsonSink gives JSON lines (format is used virtually by write)") {
    JsonSink target;
    FilterSink f(target, Level::Warn);
    f.write(Level::Error, "bad");
    ZTH_EXPECT_TRUE((target.lines() == Lines{"{\"level\":\"ERROR\",\"message\":\"bad\"}"}));
}
ZTH_TEST("robust", "BufferedSink copies the message: a string_view that dies before flush is fine") {
    MemorySink target;
    BufferedSink b(target);
    {
        std::string temp = "short-lived text that is long enough to not live in the small-string buffer";
        b.write(Level::Info, temp);
        temp.assign(temp.size(), 'X');  // the original characters are overwritten
    }
    b.flush();
    ZTH_EXPECT_EQ(target.lines().at(0), std::string("[INFO] short-lived text that is long enough to not live in the small-string buffer"));
}
ZTH_TEST("robust", "BufferedSink flushes when it is destroyed") {
    MemorySink target;
    {
        BufferedSink b(target);
        b.write(Level::Warn, "pending at the end");
        ZTH_EXPECT_TRUE(target.lines().empty());
    }
    ZTH_EXPECT_TRUE((target.lines() == Lines{"[WARN] pending at the end"}));
}
ZTH_TEST("robust", "flushing twice delivers each message once") {
    MemorySink target;
    BufferedSink b(target);
    b.write(Level::Info, "once");
    b.flush();
    b.flush();
    ZTH_EXPECT_EQ(target.lines().size(), std::size_t{1});
}
ZTH_TEST("robust", "flush_all does not count sinks that are not Flushable (also not through a decorator), and works with none") {
    Logger none;
    ZTH_EXPECT_EQ(none.flush_all(), std::size_t{0});
    MemorySink target;
    Logger log;
    log.emplace<MemorySink>();
    log.emplace<FilterSink>(target, Level::Debug);
    ZTH_EXPECT_EQ(log.flush_all(), std::size_t{0});
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "logging.hpp does not use new, delete, malloc or free: unique_ptr and make_unique own the sinks") {
    ZTH_EXPECT_NO_RAW_MEMORY("logging.hpp");
}
ZTH_TEST("clean", "every override is marked: logging.hpp uses `override` at least 9 times") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("logging.hpp"));
    const std::size_t n = ::zth::find_word(code, "override").size();
    if (n < 9) {
        ::zth::fail("logging.hpp uses `override` " + std::to_string(n) + " time(s): MemorySink (2), JsonSink (1), FilterSink (2) and BufferedSink (4: write, format, flush, the destructor) need 9");
        return;
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("clean", "`virtual` appears only in the two interfaces (5 times)") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("logging.hpp"));
    const std::size_t n = ::zth::find_word(code, "virtual").size();
    if (n != 5) {
        ::zth::fail("logging.hpp uses `virtual` " + std::to_string(n) + " time(s): Sink declares 3 (destructor, write, format), Flushable 2 (destructor, flush); derived classes only say `override`");
        return;
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("clean", "logging.hpp has no `using namespace`") {
    ZTH_EXPECT_FALSE(::zth::source_layout_of("logging.hpp").using_directive);
}
