#include <cstddef>
#include <iostream>
#include <vector>

// Exchange the values of a and b. It must also work when a and b are the SAME variable.
void swap_values(int& a, int& b) {
    // TODO
    (void)a;
    (void)b;
}

// Rotate three variables: a gets b's value, b gets c's value, c gets a's ORIGINAL value.
void rotate3(int& a, int& b, int& c) {
    // TODO
    (void)a;
    (void)b;
    (void)c;
}

// Return a reference to the larger of the two arguments (not a copy of it).
// If they are equal, return a.
const int& larger(const int& a, const int& b) {
    // TODO
    (void)b;
    return a;
}

// Return a reference to the largest element of v, so the caller can read it OR change it:
//     largest_in(v) = 0;
// If several elements share the maximum, the FIRST one. v is never empty.
int& largest_in(std::vector<int>& v) {
    // TODO
    return v[0];
}

// Limit every element of v to the range lo..hi (lo <= hi). Modify the vector itself.
void clamp_all(std::vector<int>& v, int lo, int hi) {
    // TODO
    (void)v;
    (void)lo;
    (void)hi;
}

// A reference to v[i] if i is a valid index; otherwise a reference to `fallback`.
// The caller can write through the result to change whichever one it got.
int& element_or(std::vector<int>& v, std::size_t i, int& fallback) {
    // TODO
    (void)v;
    (void)i;
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
