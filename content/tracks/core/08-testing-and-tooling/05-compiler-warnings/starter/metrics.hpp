#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Nine small pieces of code. Each one has a COMPILER WARNING, and behind each warning there is a real bug: the code compiles
// without -Werror and then does the wrong thing. This lesson builds with
//
//     -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
//
// so the build fails until every warning is dealt with. Fix the CODE so that the warning has no reason to exist. Do not silence
// it: a cast or a #pragma that only hides the warning leaves the bug in place, and the tests check the behavior.

// 1. A counter that starts at `count`. (value() must return what was given to the constructor, and add(n) adds n.)
class Counter {
public:
    explicit Counter(int count) { count = count; }
    int value() const { return count; }
    void add(int n) { count += n; }

private:
    int count = 0;
};

// 2. The sum of the squares of the values.
inline int sum_of_squares(const std::vector<int>& values) {
    int total = 0;
    for (int x : values) {
        int total = x * x;
    }
    return total;
}

// 3. A fraction (0.739) as a whole percentage, ROUNDED to the nearest one (74); halves away from zero (0.125 -> 13, -0.125 -> -13).
inline int percent(double fraction) {
    int whole = fraction * 100;
    return whole;
}

// 4. A value as a byte, CLAMPED to 0..255 (anything below 0 is 0, anything above 255 is 255).
inline std::uint8_t clamp_to_byte(int value) {
    return value;
}

// 5. The text, padded on the left with `fill` up to `width` characters; text that is already that long is returned unchanged.
inline std::string pad_left(const std::string& text, std::size_t width, char fill) {
    if (text.size() >= width) return text;
    return std::string(width - text.size(), ' ') + text;
}

// 6. Grade points: 90 and above -> 4, 80 and above -> 3, 70 and above -> 2, anything lower -> 0.
inline int grade_points(int score) {
    int points;
    if (score >= 90) {
        points = 4;
    } else if (score >= 80) {
        points = 3;
    } else if (score >= 70) {
        points = 2;
    }
    return points;
}

// 7. A name for every level: "low", "medium", "high", "critical".
enum class Level { Low, Medium, High, Critical };

inline const char* label(Level level) {
    switch (level) {
        case Level::Low: return "low";
        case Level::Medium: return "medium";
        case Level::High: return "high";
    }
    return "?";
}

// 8. Is the number zero?
inline bool is_zero(int n) {
    if (n = 0) return true;
    return false;
}

// 9. The sum 1 + 2 + ... + n (0 for n <= 0). It must work for n in the millions.
inline long long sum_to(int n) {
    if (n <= 0) return 0;
    int numbers[n];
    for (int i = 0; i < n; ++i) numbers[i] = i + 1;
    long long total = 0;
    for (int i = 0; i < n; ++i) total += numbers[i];
    return total;
}
