#include <iostream>
#include <string>
#include <vector>

// "FizzBuzz" if n % 15 == 0, "Fizz" if n % 3 == 0, "Buzz" if n % 5 == 0, else the number itself.
std::string fizzbuzz(int n) {
    // TODO
    (void)n;
    return "";
}

// Use a switch: 1-5 are "weekday", 6-7 are "weekend", anything else is "invalid".
std::string day_type(int day) {
    // TODO
    (void)day;
    return "";
}

// All primes <= n in increasing order (Sieve of Eratosthenes). Empty if n < 2.
std::vector<int> primes_up_to(int n) {
    // TODO
    (void)n;
    return {};
}

int main() {
    for (int i = 1; i <= 15; ++i) {
        std::cout << fizzbuzz(i) << (i < 15 ? ", " : "\n");
    }
    std::cout << "day 3: " << day_type(3) << ", day 7: " << day_type(7) << "\n";
    std::cout << "primes up to 30:";
    for (int p : primes_up_to(30)) {
        std::cout << ' ' << p;
    }
    std::cout << "\n";
}
