#include <iostream>

double celsius_to_fahrenheit(double celsius) {
    return celsius * 9.0 / 5.0 + 32.0;
}

double fahrenheit_to_celsius(double fahrenheit) {
    return (fahrenheit - 32.0) * 5.0 / 9.0;
}

double km_to_miles(double km) {
    constexpr double kMilesPerKm = 0.621371;
    return km * kMilesPerKm;
}

int minutes_to_seconds(int minutes) {
    return minutes * 60;
}

double average_of_two(int a, int b) {
    return (static_cast<double>(a) + b) / 2.0;
}

int main() {
    std::cout << "100 C = " << celsius_to_fahrenheit(100) << " F\n";
    std::cout << "212 F = " << fahrenheit_to_celsius(212) << " C\n";
    std::cout << "10 km = " << km_to_miles(10) << " miles\n";
    std::cout << "2 min = " << minutes_to_seconds(2) << " s\n";
    std::cout << "avg(1, 2) = " << average_of_two(1, 2) << "\n";
}
