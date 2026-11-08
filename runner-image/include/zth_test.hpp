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
#ifndef __wasi__
#include <csignal>
#endif
#include <cctype>
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

#ifndef __wasi__  // the in-browser (WASI) build has no fork(); see run_all() below
#include <sys/wait.h>
#include <unistd.h>
#endif

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

// ---- inspecting the learner's source and memory -----------------------------------------------
// The learner's files are in the current directory when the tests run.
inline std::string read_file(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return "";
    std::string s;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    std::fclose(f);
    return s;
}

// Blanks out comments and string/char literals so that a search for `new` does not find a word inside
// a comment or a message. Line structure is kept.
inline std::string strip_comments_and_literals(const std::string& src) {
    std::string out = src;
    enum { Code, Line, Block, Str, Chr } st = Code;
    for (std::size_t i = 0; i < src.size(); ++i) {
        const char c = src[i], d = i + 1 < src.size() ? src[i + 1] : '\0';
        switch (st) {
            case Code:
                if (c == '/' && d == '/') { st = Line; out[i] = ' '; }
                else if (c == '/' && d == '*') { st = Block; out[i] = ' '; }
                else if (c == '"') { st = Str; out[i] = ' '; }
                else if (c == '\'') { st = Chr; out[i] = ' '; }
                break;
            case Line:
                if (c == '\n') st = Code; else out[i] = ' ';
                break;
            case Block:
                if (c == '*' && d == '/') { out[i] = out[i + 1] = ' '; ++i; st = Code; }
                else if (c != '\n') out[i] = ' ';
                break;
            case Str:
            case Chr:
                if (c == '\\' && i + 1 < src.size()) { out[i] = ' '; if (src[i + 1] != '\n') out[i + 1] = ' '; ++i; }
                else if ((st == Str && c == '"') || (st == Chr && c == '\'')) { out[i] = ' '; st = Code; }
                else if (c != '\n') out[i] = ' ';
                break;
        }
    }
    return out;
}

// Whole-identifier search ("new" does not match "renew" or "news").
inline std::vector<std::size_t> find_word(const std::string& code, const std::string& word) {
    std::vector<std::size_t> hits;
    auto ident = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
    for (std::size_t p = code.find(word); p != std::string::npos; p = code.find(word, p + 1)) {
        const bool left = p == 0 || !ident(code[p - 1]);
        const bool right = p + word.size() >= code.size() || !ident(code[p + word.size()]);
        if (left && right) hits.push_back(p);
    }
    return hits;
}

// Which raw memory-management tools does this source use? Returns e.g. "new", "delete", "malloc".
// `= delete` (deleted functions) is not a use of the delete operator.
inline std::vector<std::string> raw_memory_uses(const std::string& source) {
    const std::string code = strip_comments_and_literals(source);
    std::vector<std::string> found;
    for (const char* w : {"new", "delete", "malloc", "calloc", "realloc", "free"}) {
        for (std::size_t p : find_word(code, w)) {
            if (std::string(w) == "delete") {
                std::size_t q = p;
                while (q > 0 && std::isspace(static_cast<unsigned char>(code[q - 1]))) --q;
                if (q > 0 && code[q - 1] == '=') continue;  // `= delete;`
            }
            if (std::string(w) == "free") {
                std::size_t q = p + 4;
                while (q < code.size() && std::isspace(static_cast<unsigned char>(code[q]))) ++q;
                if (q >= code.size() || code[q] != '(') continue;  // an identifier named free, not a call
            }
            found.push_back(w);
            break;
        }
    }
    return found;
}

// Fills the stack below the caller with a recognizable pattern, so that a function which reads an
// uninitialized local variable sees 0x5A5A5A5A... (reproducible) instead of "probably zero".
__attribute__((noinline)) inline void dirty_stack() {
    volatile unsigned char junk[16384];
    for (std::size_t i = 0; i < sizeof junk; ++i) junk[i] = 0x5A;
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

// Under AddressSanitizer a quarantined (freed, never reused) block is also POISONED, so reading or writing it ends the
// test with a heap-use-after-free report instead of just seeing the 0xDD bytes.
#if defined(__SANITIZE_ADDRESS__)
#define ZTH_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ZTH_ASAN 1
#endif
#endif
#ifdef ZTH_ASAN
#include <sanitizer/asan_interface.h>
#define ZTH_POISON(p, n) ASAN_POISON_MEMORY_REGION((p), (n))
#else
#define ZTH_POISON(p, n) ((void)0)
#endif

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
        ZTH_POISON(p, h->size);
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
    // Blocks allocated since the scope began and still alive. Never negative: freeing blocks that were
    // allocated BEFORE the scope began cannot make this go below zero.
    std::size_t leaked_blocks() const { return g_live_blocks > blocks0_ ? g_live_blocks - blocks0_ : 0; }
    std::size_t leaked_bytes() const { return g_live_bytes > bytes0_ ? g_live_bytes - bytes0_ : 0; }
    std::size_t bad_frees() const { return g_bad_frees - bad0_; }
    const char* problem() const { return g_problem; }
};

}  // namespace zth::mem

#ifdef __cpp_exceptions
#define ZTH_BAD_ALLOC() throw std::bad_alloc()
#else
#define ZTH_BAD_ALLOC() std::abort()
#endif
void* operator new(std::size_t n) {
    void* p = zth::mem::alloc(n, 0);
    if (!p) ZTH_BAD_ALLOC();
    return p;
}
void* operator new[](std::size_t n) {
    void* p = zth::mem::alloc(n, 1);
    if (!p) ZTH_BAD_ALLOC();
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
#ifdef __wasi__
// In the browser there is no fork(), so the HOST provides the isolation instead: it starts a fresh
// instance of the program per test, with a time limit it enforces itself (it kills the worker).
//   prog --list      prints one `@@ZTH {"event":"list",...}` line per test and exits
//   prog --only N    runs test number N in this process and prints its result line
// Without arguments every test runs in this one process, in order (a crash then ends the whole run).
inline int run_all(int argc, char** argv) {
    const auto& tests = registry();
    if (argc >= 2 && std::strcmp(argv[1], "--list") == 0) {
        for (std::size_t i = 0; i < tests.size(); ++i)
            std::printf("@@ZTH {\"event\":\"list\",\"index\":%zu,\"tier\":\"%s\",\"name\":\"%s\",\"timeout\":%u}\n", i,
                        tests[i].tier, json_escape(tests[i].name).c_str(), tests[i].timeout_s);
        return 0;
    }
    std::size_t first = 0, last = tests.size();
    if (argc >= 3 && std::strcmp(argv[1], "--only") == 0) {
        first = static_cast<std::size_t>(std::strtoul(argv[2], nullptr, 10));
        if (first >= tests.size()) return 2;
        last = first + 1;
    }
    int passed = 0, failed = 0;
    for (std::size_t i = first; i < last; ++i) {
        g_failed = false;
        g_message.clear();
        auto t0 = std::chrono::steady_clock::now();
        tests[i].fn();
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        emit(tests[i].tier, tests[i].name, g_failed ? "fail" : "pass", g_message, ms);
        (g_failed ? failed : passed)++;
    }
    if (argc < 2) std::printf("@@ZTH {\"event\":\"summary\",\"passed\":%d,\"failed\":%d}\n", passed, failed);
    std::fflush(stdout);
    return failed == 0 ? 0 : 1;
}
#else
// A sanitizer (AddressSanitizer, UBSan, ...) that finds a problem ends its process with this exit code (the sandbox sets
// ASAN_OPTIONS/UBSAN_OPTIONS=exitcode=77), so "a sanitizer stopped the test" differs from "the test failed".
constexpr int kSanitizerExit = 77;

// One line that says what the sanitizer found: the `runtime error: ...` line (UBSan), else the SUMMARY line (ASan/TSan).
inline std::string sanitizer_headline(const std::string& report) {
    std::istringstream in(report);
    std::string line, runtime, summary, first;
    while (std::getline(in, line)) {
        if (first.empty() && !line.empty() && line.find("=====") == std::string::npos) first = line;
        if (runtime.empty() && line.find("runtime error:") != std::string::npos) runtime = line;
        if (summary.empty() && line.rfind("SUMMARY:", 0) == 0) summary = line;
    }
    return !runtime.empty() ? runtime : !summary.empty() ? summary : first;
}

inline int run_all() {
    int passed = 0, failed = 0;
    for (const auto& t : registry()) {
        std::fflush(stdout);
        std::cout.flush();
        std::fflush(stderr);
        // The child's stderr goes to a file, so a sanitizer report can be attached to THIS test; it is passed on afterwards.
        char err_path[] = "/tmp/zth-stderr-XXXXXX";
        const int err_fd = mkstemp(err_path);
        pid_t pid = fork();
        if (pid == 0) {  // child: run one test
            if (err_fd >= 0) dup2(err_fd, 2);
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
        std::string captured;
        if (err_fd >= 0) {
            captured = read_file(err_path);
            close(err_fd);
            unlink(err_path);
            if (!captured.empty()) std::fputs(captured.c_str(), stderr);
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == kSanitizerExit) {
            emit(t.tier, t.name, "error", "stopped by a sanitizer: " + sanitizer_headline(captured), 0);
            ++failed;
        } else if (WIFSIGNALED(status)) {
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
#endif  // __wasi__

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

// Fails if the learner's file uses new / delete / malloc / free (for "no raw memory management" tiers).
#define ZTH_EXPECT_NO_RAW_MEMORY(file)                                                              \
    do {                                                                                            \
        const auto zth_uses = ::zth::raw_memory_uses(::zth::read_file(file));                       \
        if (!zth_uses.empty()) {                                                                    \
            std::string zth_list;                                                                   \
            for (const auto& u : zth_uses) zth_list += (zth_list.empty() ? "" : ", ") + u;         \
            ::zth::fail(std::string(file) + " uses raw memory management (" + zth_list +            \
                        "); let std::vector / std::string own the memory instead");                \
            return;                                                                                 \
        }                                                                                           \
    } while (0)

// Run the learner's main() with the given stdin; returns zth::Captured {out, exit_code}.
#define ZTH_RUN_USER_MAIN(...) ::zth::run_main(&zth_user_main, ##__VA_ARGS__)

#ifndef ZTH_NO_MAIN
#ifdef __wasi__
int main(int argc, char** argv) { return ::zth::run_all(argc, argv); }
#else
int main() { return ::zth::run_all(); }
#endif
#endif
