// A demo of your RingBuffer (read-only). Press Run to see what it prints.
#include <iostream>
#include <numeric>
#include <string>

#include "ring_buffer.hpp"

template <class Buffer>
void show(const char* name, const Buffer& b) {
    std::cout << name << " (size " << b.size() << " of " << b.capacity() << "):";
    for (const auto& x : b) std::cout << " " << x;
    std::cout << "\n";
}

int main() {
    RingBuffer<int, 4> ints;
    for (int i = 1; i <= 6; ++i) {
        const bool ok = ints.push(i * 10);
        std::cout << "push(" << i * 10 << ") -> " << (ok ? "ok" : "full, refused") << "\n";
    }
    show("ints", ints);

    const auto oldest = ints.pop();
    std::cout << "pop() -> " << (oldest ? std::to_string(*oldest) : "nothing") << "\n";
    ints.push(70);  // the slot of the popped element is reused: the buffer WRAPS around
    show("ints", ints);

    ints.push_overwrite(80);  // full: the oldest element (20) is replaced
    show("ints", ints);
    std::cout << "sum: " << std::accumulate(ints.begin(), ints.end(), 0) << ", front " << ints.front() << ", back " << ints.back() << "\n";

    RingBuffer<std::string, 3> words;
    words.push("alpha");
    words.emplace(5, 'x');  // builds std::string(5, 'x') in place
    words.push(std::string("gamma"));
    show("words", words);
    words.clear();
    std::cout << "after clear: empty = " << (words.empty() ? "yes" : "no") << "\n";
}
