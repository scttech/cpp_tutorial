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

// Run the learner's main() with the given stdin; returns zth::Captured {out, exit_code}.
#define ZTH_RUN_USER_MAIN(...) ::zth::run_main(&zth_user_main, ##__VA_ARGS__)

#ifndef ZTH_NO_MAIN
int main() { return ::zth::run_all(); }
#endif
