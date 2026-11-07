#include <cstddef>
#include <iostream>
#include <vector>

void swap_values(int& a, int& b) {
    const int tmp = a;  // a temporary copy: works even if a and b are the same variable
    a = b;
    b = tmp;
}

void rotate3(int& a, int& b, int& c) {
    const int first = a;
    a = b;
    b = c;
    c = first;
}

const int& larger(const int& a, const int& b) {
    return b > a ? b : a;  // both operands are lvalues of the same type, so the result is a reference, not a copy
}

int& largest_in(std::vector<int>& v) {
    std::size_t best = 0;
    for (std::size_t i = 1; i < v.size(); ++i) {
        if (v[i] > v[best]) best = i;  // strictly greater: ties keep the first
    }
    return v[best];  // a reference to the element that lives inside the vector
}

void clamp_all(std::vector<int>& v, int lo, int hi) {
    for (int& x : v) {  // int&: x IS the element. (Plain `int x` would modify a copy.)
        if (x < lo) {
            x = lo;
        } else if (x > hi) {
            x = hi;
        }
    }
}

int& element_or(std::vector<int>& v, std::size_t i, int& fallback) {
    if (i < v.size()) return v[i];
    return fallback;
}

int main() {
    int x = 1, y = 2, z = 3;
    rotate3(x, y, z);
    std::cout << x << " " << y << " " << z << "\n";  // 2 3 1

    std::vector<int> v{4, 9, 2, 9};
    largest_in(v) = 0;  // sets the FIRST 9 to 0
    for (int n : v) std::cout << n << " ";
    std::cout << "\n";
}
