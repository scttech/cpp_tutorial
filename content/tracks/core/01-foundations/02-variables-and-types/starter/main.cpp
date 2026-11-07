#include <iostream>

double celsius_to_fahrenheit(double celsius) {
    // TODO: celsius * 9/5 + 32  (watch out for integer division!)
    return 0.0;
}

double fahrenheit_to_celsius(double fahrenheit) {
    // TODO: (fahrenheit - 32) * 5/9
    return 0.0;
}

double km_to_miles(double km) {
    // TODO: use a constexpr constant for the conversion factor 0.621371
    return 0.0;
}

int minutes_to_seconds(int minutes) {
    // TODO
    return 0;
}

double average_of_two(int a, int b) {
    // TODO: the exact average, so average_of_two(1, 2) is 1.5
    return 0.0;
}

int main() {
    std::cout << "100 C = " << celsius_to_fahrenheit(100) << " F\n";
    std::cout << "212 F = " << fahrenheit_to_celsius(212) << " C\n";
    std::cout << "10 km = " << km_to_miles(10) << " miles\n";
    std::cout << "2 min = " << minutes_to_seconds(2) << " s\n";
    std::cout << "avg(1, 2) = " << average_of_two(1, 2) << "\n";
}
