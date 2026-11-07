#include <iostream>
#include <string>
#include <vector>

// Arithmetic mean. 0.0 for an empty vector.
double mean(const std::vector<double>& values) {
    // TODO
    (void)values;
    return 0.0;
}

// Middle value (average of the two middle values for an even count). 0.0 if empty.
// The caller's vector must NOT be modified: sort a copy.
double median(const std::vector<double>& values) {
    // TODO
    (void)values;
    return 0.0;
}

// Most frequent value; on a tie the smallest of the tied values. 0 if empty.
int mode(const std::vector<int>& values) {
    // TODO
    (void)values;
    return 0;
}

// `s` repeated `times` times with `sep` between copies (not after the last). "" if times <= 0.
std::string repeat(const std::string& s, int times = 2, const std::string& sep = "") {
    // TODO
    (void)s;
    (void)times;
    (void)sep;
    return "";
}

// Overloads: pick the right one at compile time.
std::string kind_of(int) {
    return "?";  // TODO
}
std::string kind_of(double) {
    return "?";  // TODO
}

int main() {
    std::cout << "repeat(\"ab\") = " << repeat("ab") << "\n";
    std::cout << "repeat(\"ab\", 3) = " << repeat("ab", 3) << "\n";
    std::cout << "repeat(\"ab\", 3, \"-\") = " << repeat("ab", 3, "-") << "\n";

    const std::vector<double> data{3.0, 1.0, 2.0, 10.0};
    std::cout << "mean = " << mean(data) << ", median = " << median(data) << "\n";
    std::cout << "mode = " << mode({4, 1, 4, 2, 2, 4}) << "\n";
    std::cout << "kind_of(3) = " << kind_of(3) << ", kind_of(3.5) = " << kind_of(3.5) << "\n";
}
