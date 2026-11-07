// MUTANT: correct but far too slow (O(n * sqrt(n))): the Fast test must time out.
#include <string>
#include <vector>

std::string fizzbuzz(int n) {
    if (n % 15 == 0) return "FizzBuzz";
    if (n % 3 == 0) return "Fizz";
    if (n % 5 == 0) return "Buzz";
    return std::to_string(n);
}
std::string day_type(int d) {
    switch (d) {
        case 1: case 2: case 3: case 4: case 5: return "weekday";
        case 6: case 7: return "weekend";
        default: return "invalid";
    }
}
std::vector<int> primes_up_to(int n) {
    std::vector<int> out;
    for (int i = 2; i <= n; ++i) {
        bool prime = true;
        for (int d = 2; d < i; ++d) {
            if (i % d == 0) { prime = false; break; }
        }
        if (prime) out.push_back(i);
    }
    return out;
}
