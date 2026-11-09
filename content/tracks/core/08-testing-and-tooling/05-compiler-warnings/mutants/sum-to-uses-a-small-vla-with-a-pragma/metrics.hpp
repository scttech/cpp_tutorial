// MUTANT: a #pragma hides the VLA warning, and the array still overflows the stack for millions of terms.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Nine small pieces of code, each fixed at the cause of its warning (see the lesson).

class Counter {
public:
    explicit Counter(int initial) : count_(initial) {}
    int value() const { return count_; }
    void add(int n) { count_ += n; }

private:
    int count_ = 0;
};

inline int sum_of_squares(const std::vector<int>& values) {
    int total = 0;
    for (int x : values) {
        total += x * x;
    }
    return total;
}

inline int percent(double fraction) {
    return static_cast<int>(std::lround(fraction * 100));
}

inline std::uint8_t clamp_to_byte(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<std::uint8_t>(value);
}

inline std::string pad_left(const std::string& text, std::size_t width, char fill) {
    if (text.size() >= width) return text;
    return std::string(width - text.size(), fill) + text;
}

inline int grade_points(int score) {
    int points = 0;
    if (score >= 90) {
        points = 4;
    } else if (score >= 80) {
        points = 3;
    } else if (score >= 70) {
        points = 2;
    }
    return points;
}

enum class Level { Low, Medium, High, Critical };

inline const char* label(Level level) {
    switch (level) {
        case Level::Low: return "low";
        case Level::Medium: return "medium";
        case Level::High: return "high";
        case Level::Critical: return "critical";
    }
    return "?";
}

inline bool is_zero(int n) {
    return n == 0;
}

inline long long sum_to(int n) {
    if (n <= 0) return 0;
#pragma clang diagnostic ignored "-Wvla-extension"
#pragma clang diagnostic ignored "-Wvla"
    int numbers[n];
    for (int i = 0; i < n; ++i) numbers[i] = i + 1;
    long long total = 0;
    for (int i = 0; i < n; ++i) total += numbers[i];
    return total;
}
