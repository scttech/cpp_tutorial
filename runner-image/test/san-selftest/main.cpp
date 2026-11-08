// "Learner" code for the sanitizer self-test: each function has one deliberate bug.
#include <climits>
#include <vector>

int sum_to(const std::vector<int>& v, int n) {  // n is inclusive: reads one past the end when n == size
    int s = 0;
    for (int i = 0; i <= n; ++i) s += v[static_cast<std::size_t>(i)];
    return s;
}

int read_freed() {
    int* p = new int(5);
    delete p;
    return *p;
}

int add(int a, int b) { return a + b; }  // signed overflow when it does not fit

int divide(int a, int b) { return a / b; }

int ok_sum(const std::vector<int>& v) {
    int s = 0;
    for (int x : v) s += x;
    return s;
}

int main() {}
