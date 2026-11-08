// A demo of your StaticVector (read-only). Press Run to see what it prints.
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

#include "static_vector.hpp"

// Says when it is created and destroyed, so you can SEE the lifetimes.
struct Noisy {
    int id;
    explicit Noisy(int i) : id(i) { std::cout << "  construct " << id << "\n"; }
    Noisy(const Noisy& o) : id(o.id) { std::cout << "  copy " << id << "\n"; }
    Noisy(Noisy&& o) noexcept : id(o.id) { std::cout << "  move " << id << "\n"; }
    Noisy& operator=(const Noisy&) = default;
    Noisy& operator=(Noisy&&) noexcept = default;
    ~Noisy() { std::cout << "  destroy " << id << "\n"; }
};

template <class V>
void show(const char* name, const V& v) {
    std::cout << name << " (size " << v.size() << " of " << v.capacity() << "):";
    for (const auto& x : v) std::cout << " " << x;
    std::cout << "\n";
}

int main() {
    StaticVector<int, 5> v{30, 10, 20};
    v.push_back(5);
    show("v", v);
    std::sort(v.begin(), v.end());
    show("sorted", v);
    v.insert(1, 7);
    show("insert(1, 7)", v);
    std::cout << "push_back on a full vector: " << (v.push_back(99) ? "ok" : "refused") << "\n";
    v.erase(0);
    show("erase(0)", v);

    StaticVector<std::string, 3> words;
    words.emplace_back(3, 'x');
    words.push_back("hello");
    show("words", words);

    std::cout << "A StaticVector<Noisy, 4>: no Noisy exists until one is added.\n";
    {
        StaticVector<Noisy, 4> noisy;
        std::cout << "created the (empty) vector\n";
        noisy.emplace_back(1);  // constructed in place: ONE construct, no copy, no move
        noisy.push_back(Noisy(2));
        std::cout << "pop_back:\n";
        noisy.pop_back();
        std::cout << "end of scope:\n";
    }
}
