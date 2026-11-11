#pragma once
// READ-ONLY. The vocabulary of the logger.
#include <cstdint>
#include <string>

namespace flightlog {

enum class Level : unsigned char { Debug, Info, Warn, Error };  // ordered: Debug < Info < Warn < Error

struct Entry {
    std::uint64_t seq = 0;  // 1, 2, 3 ... in the order logs were ACCEPTED
    Level level = Level::Info;
    std::string message;
};

inline const char* level_name(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
    }
    return "?";
}

}  // namespace flightlog
