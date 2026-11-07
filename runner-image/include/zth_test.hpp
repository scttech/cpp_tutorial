// zth_test.hpp - tiny single-header test harness for "CPP: From Zero to Hero".
//
// Included by a lesson's hidden tests/test.cpp (exactly one translation unit). It defines main().
// Results are printed as protocol lines the runner parses (see spec section 4.2):
//   @@ZTH {"event":"test","name":"...","tier":"works","status":"pass","ms":0.3}
//
// Each test runs in a forked child process, so a crash (segfault, abort) or an infinite loop in one
// test is reported as "error"/"timeout" for that test only, and the remaining tests still run.
//
// Tests that need the learner's main(): the runner compiles the learner's files with
// `-Dmain=zth_user_main`; call it with ZTH_RUN_USER_MAIN("stdin text"). Only std::cin/std::cout
// are captured (C stdio such as printf is not).
#pragma once

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

extern "C" int zth_user_main();  // the learner's main(), renamed at link time; only linked if used

namespace zth {

struct TestCase {
    const char* tier;
    const char* name;
    void (*fn)();
    unsigned timeout_s;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

// State for the test currently running (each test runs in its own process, so a global is fine).
inline bool g_failed = false;
inline std::string g_message;

struct Registrar {
    Registrar(const char* tier, const char* name, void (*fn)(), unsigned timeout_s = 2) {
        registry().push_back({tier, name, fn, timeout_s});
    }
};

inline void fail(std::string msg) {
    if (!g_failed) {  // keep the first failure: it is the most useful one
        g_failed = true;
        g_message = std::move(msg);
    }
}

// ---- value formatting for failure messages ---------------------------------------------------
template <typename T, typename = void>
struct is_streamable : std::false_type {};
template <typename T>
struct is_streamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <typename T, typename = void>
struct is_range : std::false_type {};
template <typename T>
struct is_range<T, std::void_t<decltype(std::begin(std::declval<const T&>())), decltype(std::end(std::declval<const T&>()))>>
    : std::true_type {};

template <typename T>
std::string show(const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    } else if constexpr (std::is_convertible_v<const T&, std::string_view> && !std::is_pointer_v<T> &&
                         !std::is_array_v<T>) {
        return "\"" + std::string(std::string_view(v)) + "\"";
    } else if constexpr (std::is_same_v<std::decay_t<T>, const char*> || std::is_same_v<std::decay_t<T>, char*>) {
        return v ? "\"" + std::string(v) + "\"" : "nullptr";
    } else if constexpr (std::is_array_v<T> && std::is_same_v<std::remove_extent_t<T>, char>) {
        return "\"" + std::string(v) + "\"";
    } else if constexpr (std::is_enum_v<T>) {
        return std::to_string(static_cast<long long>(v));
    } else if constexpr (is_range<T>::value) {  // containers: {1, 2, 3}
        std::string out = "{";
        std::size_t n = 0;
        for (const auto& e : v) {
            if (n++ > 0) out += ", ";
            if (n > 16) {
                out += "...";
                break;
            }
            out += show(e);
        }
        return out + "}";
    } else if constexpr (is_streamable<T>::value) {
        std::ostringstream os;
        os << v;
        return os.str();
    } else {
        return "<unprintable>";
    }
}

inline std::string json_escape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) {
                    char b[8];
                    std::snprintf(b, sizeof b, "\\u%04x", c);
                    o += b;
                } else {
                    o += static_cast<char>(c);
                }
        }
    }
    return o;
}

inline void emit(const char* tier, const char* name, const char* status, const std::string& msg, double ms) {
    std::string line = "@@ZTH {\"event\":\"test\",\"name\":\"" + json_escape(name) + "\",\"tier\":\"" + tier +
                       "\",\"status\":\"" + status + "\"";
    // Keep protocol lines small even when an expectation compares huge strings.
    constexpr std::size_t kMaxMsg = 1500;
    const std::string shown = msg.size() > kMaxMsg ? msg.substr(0, kMaxMsg) + "... (message truncated)" : msg;
    if (!shown.empty()) line += ",\"msg\":\"" + json_escape(shown) + "\"";
    char b[48];
    std::snprintf(b, sizeof b, ",\"ms\":%.3f}", ms);
    line += b;
    std::fputs(line.c_str(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

// ---- running the learner's main() with captured I/O ------------------------------------------
struct Captured {
    std::string out;
    int exit_code;
    bool truncated;  // the program printed more than the capture limit
};

// Output sink that keeps at most kLimit bytes. A runaway program (e.g. an infinite loop that prints)
// then neither exhausts the container's memory nor gets reported as a confusing "Killed"; it simply
// runs into the per-test time limit.
class CappedBuf : public std::streambuf {
public:
    static constexpr std::size_t kLimit = 1u << 20;  // 1 MiB
    std::string data;
    bool truncated = false;

protected:
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
        if (data.size() >= kLimit) {
            truncated = true;
            return traits_type::eof();
        }
        data.push_back(traits_type::to_char_type(ch));
        return ch;
    }
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        const std::size_t room = data.size() < kLimit ? kLimit - data.size() : 0;
        const std::size_t take = static_cast<std::size_t>(n) < room ? static_cast<std::size_t>(n) : room;
        data.append(s, take);
        if (take < static_cast<std::size_t>(n)) truncated = true;
        return static_cast<std::streamsize>(take);
    }
};

inline Captured run_main(int (*fn)(), const std::string& input = "") {
    std::istringstream in(input);
    CappedBuf out;
    auto* old_in = std::cin.rdbuf(in.rdbuf());
    auto* old_out = std::cout.rdbuf(&out);
    std::cin.clear();
    std::cout.clear();
    int code = fn();
    std::cout.flush();
    std::cin.rdbuf(old_in);
    std::cout.rdbuf(old_out);
    std::cout.clear();  // a truncated capture leaves cout in a failed state: reset it for later calls
    return {out.data, code, out.truncated};
}

// ---- instrumented types ----------------------------------------------------------------------
// Counts how it is constructed, copied, moved and destroyed. Tests use it to observe what the
// learner's code (or std::vector) does with their objects. Call Tracked::reset() at the start of a
// test; each test runs in its own process, so counters never leak between tests.
struct Tracked {
    static inline int defaults = 0, value_ctors = 0, copies = 0, moves = 0;
    static inline int copy_assigns = 0, move_assigns = 0, destroyed = 0;
    int value;

    static void reset() { defaults = value_ctors = copies = moves = copy_assigns = move_assigns = destroyed = 0; }
    static int constructed() { return defaults + value_ctors + copies + moves; }
    static int live() { return constructed() - destroyed; }  // objects currently alive (since reset)

    Tracked() : value(0) { ++defaults; }
    Tracked(int v) : value(v) { ++value_ctors; }  // NOLINT: implicit on purpose, so tests can write {1, 2, 3}
    Tracked(const Tracked& o) : value(o.value) { ++copies; }
    Tracked(Tracked&& o) noexcept : value(o.value) {
        o.value = -1;  // visibly "moved from"
        ++moves;
    }
    Tracked& operator=(const Tracked& o) {
        value = o.value;
        ++copy_assigns;
        return *this;
    }
    Tracked& operator=(Tracked&& o) noexcept {
        value = o.value;
        o.value = -1;
        ++move_assigns;
        return *this;
    }
    ~Tracked() { ++destroyed; }
    bool operator==(const Tracked& o) const { return value == o.value; }
};
inline std::ostream& operator<<(std::ostream& os, const Tracked& t) { return os << t.value; }

}  // namespace zth

// ---- allocation tracking ---------------------------------------------------------------------
// Opt-in: a lesson's test.cpp does `#define ZTH_TRACK_ALLOCATIONS` before including this header.
// That replaces global operator new/delete (for the learner's code too, since it is one program) with
// versions that count blocks and detect: leaks, double frees, frees of pointers that did not come from
// new, and new[]/delete mismatches. Freed blocks are poisoned (0xDD) and held back for a while, so a
// use-after-free reads obvious garbage instead of silently "working".
#ifdef ZTH_TRACK_ALLOCATIONS
#include <atomic>
#include <new>

namespace zth::mem {

inline std::atomic<std::size_t> g_allocs{0}, g_frees{0}, g_live_blocks{0}, g_live_bytes{0}, g_bad_frees{0};
inline std::atomic<std::size_t> g_quarantined{0};
inline const char* g_problem = nullptr;  // first problem seen, e.g. "double free"

constexpr std::uint32_t kLive = 0xA110CA7Eu, kFreed = 0xDEADF4EEu;
constexpr std::size_t kQuarantineLimit = 16u << 20;  // beyond this, freed blocks are really released

struct alignas(16) Header {
    std::size_t size;
    std::uint32_t magic;
    std::uint32_t array;  // 1 if allocated with new[]
};
static_assert(sizeof(Header) == 16);

inline void problem(const char* what) {
    g_bad_frees.fetch_add(1, std::memory_order_relaxed);
    if (!g_problem) g_problem = what;
}

inline void* alloc(std::size_t n, std::uint32_t array) {
    void* raw = std::malloc(n + sizeof(Header));
    if (!raw) return nullptr;
    auto* h = static_cast<Header*>(raw);
    h->size = n;
    h->magic = kLive;
    h->array = array;
    g_allocs.fetch_add(1, std::memory_order_relaxed);
    g_live_blocks.fetch_add(1, std::memory_order_relaxed);
    g_live_bytes.fetch_add(n, std::memory_order_relaxed);
    return h + 1;
}

inline void release(void* p, std::uint32_t array) {
    if (!p) return;
    auto* h = static_cast<Header*>(p) - 1;
    if (h->magic == kFreed) return problem("a block was freed twice (double free)");
    if (h->magic != kLive) return problem("delete was given a pointer that new never returned");
    if (h->array != array) problem(array ? "memory from new was released with delete[]" : "memory from new[] was released with delete (use delete[])");
    h->magic = kFreed;
    g_frees.fetch_add(1, std::memory_order_relaxed);
    g_live_blocks.fetch_sub(1, std::memory_order_relaxed);
    g_live_bytes.fetch_sub(h->size, std::memory_order_relaxed);
    std::memset(p, 0xDD, h->size);
    if (g_quarantined.load(std::memory_order_relaxed) + h->size + sizeof(Header) <= kQuarantineLimit) {
        g_quarantined.fetch_add(h->size + sizeof(Header), std::memory_order_relaxed);  // keep it: never reused, so the poison stays visible
    } else {
        std::free(h);
    }
}

// What happened since the scope was created. Create one at the start of a test body, after any set-up
// that must not count, then ask it questions at the end.
class Scope {
    std::size_t allocs0_ = g_allocs, frees0_ = g_frees, blocks0_ = g_live_blocks, bytes0_ = g_live_bytes, bad0_ = g_bad_frees;

public:
    std::size_t allocs() const { return g_allocs - allocs0_; }
    std::size_t frees() const { return g_frees - frees0_; }
    std::size_t leaked_blocks() const { return g_live_blocks - blocks0_; }
    std::size_t leaked_bytes() const { return g_live_bytes - bytes0_; }
    std::size_t bad_frees() const { return g_bad_frees - bad0_; }
    const char* problem() const { return g_problem; }
};

}  // namespace zth::mem

void* operator new(std::size_t n) {
    void* p = zth::mem::alloc(n, 0);
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t n) {
    void* p = zth::mem::alloc(n, 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return zth::mem::alloc(n, 0); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return zth::mem::alloc(n, 1); }
void operator delete(void* p) noexcept { zth::mem::release(p, 0); }
void operator delete[](void* p) noexcept { zth::mem::release(p, 1); }
void operator delete(void* p, std::size_t) noexcept { zth::mem::release(p, 0); }
void operator delete[](void* p, std::size_t) noexcept { zth::mem::release(p, 1); }
void operator delete(void* p, const std::nothrow_t&) noexcept { zth::mem::release(p, 0); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { zth::mem::release(p, 1); }
#endif  // ZTH_TRACK_ALLOCATIONS

namespace zth {

// ---- driver ----------------------------------------------------------------------------------
inline int run_all() {
    int passed = 0, failed = 0;
    for (const auto& t : registry()) {
        std::fflush(stdout);
        std::cout.flush();
        pid_t pid = fork();
        if (pid == 0) {  // child: run one test
            alarm(t.timeout_s);
            auto t0 = std::chrono::steady_clock::now();
            t.fn();
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            emit(t.tier, t.name, g_failed ? "fail" : "pass", g_message, ms);
            std::cout.flush();
            _exit(g_failed ? 1 : 0);
        }
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            if (sig == SIGALRM) {
                emit(t.tier, t.name, "timeout", "test did not finish in " + std::to_string(t.timeout_s) + "s (infinite loop?)", 0);
            } else {
                emit(t.tier, t.name, "error", std::string("crashed: ") + strsignal(sig), 0);
            }
            ++failed;
        } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            ++passed;
        } else {
            ++failed;
        }
    }
    std::printf("@@ZTH {\"event\":\"summary\",\"passed\":%d,\"failed\":%d}\n", passed, failed);
    std::fflush(stdout);
    return failed == 0 ? 0 : 1;
}

}  // namespace zth

#define ZTH_CAT_(a, b) a##b
#define ZTH_CAT(a, b) ZTH_CAT_(a, b)

// ZTH_TEST("works", "reverses a normal string") { ... }
#define ZTH_TEST(tier, name)                                                          \
    static void ZTH_CAT(zth_fn_, __LINE__)();                                         \
    static ::zth::Registrar ZTH_CAT(zth_reg_, __LINE__)(tier, name, &ZTH_CAT(zth_fn_, __LINE__)); \
    static void ZTH_CAT(zth_fn_, __LINE__)()

// Expectations stop the current test at the first failure.
#define ZTH_EXPECT_TRUE(cond)                                              \
    do {                                                                   \
        if (!(cond)) {                                                     \
            ::zth::fail("expected " #cond " to be true");                  \
            return;                                                        \
        }                                                                  \
    } while (0)

#define ZTH_EXPECT_FALSE(cond)                                             \
    do {                                                                   \
        if ((cond)) {                                                      \
            ::zth::fail("expected " #cond " to be false");                 \
            return;                                                        \
        }                                                                  \
    } while (0)

#define ZTH_EXPECT_EQ(actual, expected)                                                         \
    do {                                                                                        \
        const auto& zth_a = (actual);                                                           \
        const auto& zth_e = (expected);                                                         \
        if (!(zth_a == zth_e)) {                                                                \
            ::zth::fail("expected " + ::zth::show(zth_e) + ", got " + ::zth::show(zth_a));      \
            return;                                                                             \
        }                                                                                       \
    } while (0)

#define ZTH_EXPECT_NE(actual, unexpected)                                                       \
    do {                                                                                        \
        const auto& zth_a = (actual);                                                           \
        const auto& zth_u = (unexpected);                                                       \
        if (zth_a == zth_u) {                                                                   \
            ::zth::fail("expected a value other than " + ::zth::show(zth_u));                   \
            return;                                                                             \
        }                                                                                       \
    } while (0)

#define ZTH_EXPECT_NEAR(actual, expected, eps)                                                  \
    do {                                                                                        \
        const double zth_a = (actual), zth_e = (expected);                                      \
        if (!(zth_a >= zth_e - (eps) && zth_a <= zth_e + (eps))) {                              \
            ::zth::fail("expected " + ::zth::show(zth_e) + " +/- " + ::zth::show(double(eps)) + \
                        ", got " + ::zth::show(zth_a));                                         \
            return;                                                                             \
        }                                                                                       \
    } while (0)

// Substring check, handy for program output.
#define ZTH_EXPECT_CONTAINS(haystack, needle)                                                   \
    do {                                                                                        \
        const std::string zth_h = (haystack);                                                   \
        const std::string zth_n = (needle);                                                     \
        if (zth_h.find(zth_n) == std::string::npos) {                                           \
            ::zth::fail("expected output to contain " + ::zth::show(zth_n) + ", got " +         \
                        ::zth::show(zth_h));                                                    \
            return;                                                                             \
        }                                                                                       \
    } while (0)

#ifdef ZTH_TRACK_ALLOCATIONS
// Memory expectations take a zth::mem::Scope created earlier in the test.
// Reports double frees and new/delete mismatches first (the more serious bug), then leaks.
#define ZTH_EXPECT_NO_MEMORY_BUGS(scope)                                                        \
    do {                                                                                        \
        /* read every counter BEFORE building strings: the strings allocate too */              \
        const std::size_t zth_bad = (scope).bad_frees(), zth_blocks = (scope).leaked_blocks(); \
        const std::size_t zth_bytes = (scope).leaked_bytes();                                   \
        const char* zth_prob = (scope).problem();                                               \
        if (zth_bad > 0) {                                                                      \
            ::zth::fail(std::string("memory misuse: ") + (zth_prob ? zth_prob : "bad free"));   \
            return;                                                                             \
        }                                                                                       \
        if (zth_blocks > 0) {                                                                   \
            ::zth::fail("memory leak: " + std::to_string(zth_blocks) + " allocation(s) (" +     \
                        std::to_string(zth_bytes) + " bytes) were never freed");                \
            return;                                                                             \
        }                                                                                       \
    } while (0)

// At most `n` calls to operator new since the scope began.
#define ZTH_EXPECT_ALLOCS_AT_MOST(scope, n)                                                     \
    do {                                                                                        \
        const std::size_t zth_n = (scope).allocs();                                             \
        if (zth_n > static_cast<std::size_t>(n)) {                                              \
            ::zth::fail("expected at most " + std::to_string(n) + " heap allocation(s), but there were " + \
                        std::to_string(zth_n));                                                 \
            return;                                                                             \
        }                                                                                       \
    } while (0)
#endif

// Run the learner's main() with the given stdin; returns zth::Captured {out, exit_code}.
#define ZTH_RUN_USER_MAIN(...) ::zth::run_main(&zth_user_main, ##__VA_ARGS__)

#ifndef ZTH_NO_MAIN
int main() { return ::zth::run_all(); }
#endif
