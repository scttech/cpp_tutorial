// Structured logging: every line is a JSON object a machine can parse. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "logger.hpp"

class StdoutSink : public slog::Sink {
public:
    void write(std::string_view line) override { std::printf("%.*s\n", static_cast<int>(line.size()), line.data()); }
};
class FakeClock : public slog::Clock {
public:
    std::int64_t now = 1714564800123;   // 2024-05-01T12:00:00.123Z
    std::int64_t now_ms() override { return now; }
};

int main() {
    StdoutSink sink;
    FakeClock clock;
    slog::Options options;
    options.min_level = slog::Level::Info;
    options.dedupe_window_ms = 1000;   // identical lines within a second are collapsed
    options.max_line_bytes = 200;
    slog::Logger log(sink, clock, options);

    log.info("server started", {slog::kv("port", 8080), slog::kv("tls", true), slog::kv("load", 0.75)});
    log.debug("this is below the minimum level", {});
    slog::Logger request = log.with({slog::kv("request_id", "r-42"), slog::kv("user", "ann")});
    request.warn("slow query", {slog::kv("ms", 1234), slog::kv("sql", "SELECT *\nFROM \"orders\" WHERE note = 'caf\xC3\xA9'")});
    for (int i = 0; i < 5; ++i) {
        clock.now += 100;
        request.error("connection reset", {slog::kv("peer", "10.0.0.7")});   // the same line five times in half a second
    }
    clock.now += 2000;
    request.error("connection reset", {slog::kv("peer", "10.0.0.7")});
    log.info("bytes that are not UTF-8", {slog::kv("raw", "ok\xFF\xFE""bad")});
    log.info(std::string(300, 'x') + " a message much too long for the line limit", {slog::kv("a", 1), slog::kv("b", 2)});
    log.info("duplicate and reserved keys", {slog::kv("a", 1), slog::kv("level", "mine"), slog::kv("a", 2), slog::kv("ts", "mine")});
    log.info("numbers", {slog::kv("whole", 1.0), slog::kv("tiny", 1e-9), slog::kv("huge", 1e21), slog::kv("nan", std::nan("")), slog::kv_null("nothing")});
    log.flush();
}
