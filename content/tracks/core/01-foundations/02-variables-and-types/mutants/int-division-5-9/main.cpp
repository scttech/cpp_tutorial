// MUTANT: the classic trap. 5 / 9 is integer division, so fahrenheit_to_celsius always returns 0.
double celsius_to_fahrenheit(double c) { return c * 9.0 / 5.0 + 32.0; }
double fahrenheit_to_celsius(double f) { return (f - 32) * (5 / 9); }
double km_to_miles(double km) { return km * 0.621371; }
int minutes_to_seconds(int m) { return m * 60; }
double average_of_two(int a, int b) { return (a + b) / 2.0; }
