#pragma once
#include <ostream>

// One run in a run-length encoding: `count` copies of `value` in a row.
struct Run {
    int value;
    int count;
    bool operator==(const Run&) const = default;
};

inline std::ostream& operator<<(std::ostream& os, const Run& r) {
    return os << r.value << " x" << r.count;
}
