// MUTANT: loop bound `i < n` leaves out n itself when n is prime.
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
    if (n < 2) return out;
    std::vector<bool> comp(n + 1, false);
    for (int i = 2; i < n; ++i) {
        if (comp[i]) continue;
        out.push_back(i);
        for (long long j = static_cast<long long>(i) * i; j <= n; j += i) comp[j] = true;
    }
    return out;
}
