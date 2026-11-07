#include <charconv>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace {

enum class Dimension { Length, Mass, Temperature };

struct Unit {
    Dimension dimension;
    double factor;  // to the base unit (m or kg); unused for temperatures
};

const std::unordered_map<std::string, Unit>& unit_table() {
    static const std::unordered_map<std::string, Unit> table{
        {"m", {Dimension::Length, 1.0}},
        {"km", {Dimension::Length, 1000.0}},
        {"mi", {Dimension::Length, 1609.344}},
        {"ft", {Dimension::Length, 0.3048}},
        {"kg", {Dimension::Mass, 1.0}},
        {"g", {Dimension::Mass, 0.001}},
        {"lb", {Dimension::Mass, 0.45359237}},
        {"C", {Dimension::Temperature, 0.0}},
        {"F", {Dimension::Temperature, 0.0}},
        {"K", {Dimension::Temperature, 0.0}},
    };
    return table;
}

constexpr double kZeroCelsiusInKelvin = 273.15;

// value in `unit` -> value in the base unit of its dimension
double to_base(double value, const std::string& name, const Unit& unit) {
    if (unit.dimension != Dimension::Temperature) {
        return value * unit.factor;
    }
    if (name == "C") {
        return value + kZeroCelsiusInKelvin;
    }
    if (name == "F") {
        return (value - 32.0) * 5.0 / 9.0 + kZeroCelsiusInKelvin;
    }
    return value;  // kelvin
}

// value in the base unit -> value in `unit`
double from_base(double base, const std::string& name, const Unit& unit) {
    if (unit.dimension != Dimension::Temperature) {
        return base / unit.factor;
    }
    if (name == "C") {
        return base - kZeroCelsiusInKelvin;
    }
    if (name == "F") {
        return (base - kZeroCelsiusInKelvin) * 9.0 / 5.0 + 32.0;
    }
    return base;
}

bool parse_number(const std::string& text, double& out) {
    const char* first = text.data();
    const char* last = first + text.size();
    double value = 0.0;
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last || !std::isfinite(value)) {
        return false;  // not a number, trailing junk, or nan/inf (from_chars accepts those)
    }
    out = value;
    return true;
}

std::vector<std::string> split_words(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> words;
    for (std::string w; in >> w;) {
        words.push_back(w);
    }
    return words;
}

void handle_convert(const std::vector<std::string>& words) {
    if (words.size() != 4) {
        std::cout << "error: usage: convert <value> <from> <to>\n";
        return;
    }
    double value = 0.0;
    if (!parse_number(words[1], value)) {
        std::cout << "error: invalid number '" << words[1] << "'\n";
        return;
    }
    const auto& table = unit_table();
    const auto from = table.find(words[2]);
    if (from == table.end()) {
        std::cout << "error: unknown unit '" << words[2] << "'\n";
        return;
    }
    const auto to = table.find(words[3]);
    if (to == table.end()) {
        std::cout << "error: unknown unit '" << words[3] << "'\n";
        return;
    }
    if (from->second.dimension != to->second.dimension) {
        std::cout << "error: cannot convert " << words[2] << " to " << words[3] << "\n";
        return;
    }
    const double base = to_base(value, words[2], from->second);
    if (from->second.dimension == Dimension::Temperature && base < -1e-9) {
        std::cout << "error: below absolute zero\n";
        return;
    }
    double result = from_base(base, words[3], to->second);
    if (std::abs(result) < 0.00005) {
        result = 0.0;  // rounds to zero: never print -0.0000
    }
    std::cout << words[1] << ' ' << words[2] << " = " << std::fixed << std::setprecision(4) << result << ' '
              << words[3] << '\n';
}

}  // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        const std::vector<std::string> words = split_words(line);
        if (words.empty()) {
            continue;
        }
        if (words[0] == "quit") {
            break;
        }
        if (words[0] == "convert") {
            handle_convert(words);
        } else {
            std::cout << "error: unknown command '" << words[0] << "'\n";
        }
    }
}
