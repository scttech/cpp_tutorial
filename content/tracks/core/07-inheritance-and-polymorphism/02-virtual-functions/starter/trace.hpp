#pragma once

#include <string>
#include <vector>

// A shared log (read-only). The classes in animals.hpp append a line from their constructors and destructors,
// and the tests and main.cpp read it back: it shows in which ORDER the parts of an object are built and destroyed.
inline std::vector<std::string>& trace_log() {
    static std::vector<std::string> log;
    return log;
}
inline void trace(const std::string& line) { trace_log().push_back(line); }
