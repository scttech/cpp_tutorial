#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

std::string fizzbuzz(int n) {
    if (n % 15 == 0) {
        return "FizzBuzz";
    }
    if (n % 3 == 0) {
        return "Fizz";
    }
    if (n % 5 == 0) {
        return "Buzz";
    }
    return std::to_string(n);
}

std::string day_type(int day) {
    switch (day) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            return "weekday";
        case 6:
        case 7:
            return "weekend";
        default:
            return "invalid";
    }
}

std::vector<int> primes_up_to(int n) {
    std::vector<int> primes;
    if (n < 2) {
        return primes;
    }
    std::vector<bool> composite(static_cast<std::size_t>(n) + 1, false);
    for (std::size_t i = 2; i <= static_cast<std::size_t>(n); ++i) {
        if (composite[i]) {
            continue;
        }
        primes.push_back(static_cast<int>(i));
        for (std::size_t j = i * i; j <= static_cast<std::size_t>(n); j += i) {
            composite[j] = true;
        }
    }
    return primes;
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
