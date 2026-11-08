// A demo of copying versus moving a Text (read-only). Press Run to see what it prints.
#include <iostream>
#include <utility>
#include <vector>

#include "text.hpp"

Text make_greeting() {
    Text t("hello");
    t.append(", world");
    return t;  // no copy: the compiler constructs t directly in the caller (or moves it)
}

int main() {
    Text a = make_greeting();
    const char* buffer = a.c_str();

    Text copied = a;             // a copy: a new buffer
    Text moved = std::move(a);   // a move: takes a's buffer
    std::cout << "copy made a new buffer: " << (copied.c_str() != buffer ? "yes" : "no") << "\n";
    std::cout << "move kept the buffer:   " << (moved.c_str() == buffer ? "yes" : "no") << "\n";
    std::cout << "moved-from a is empty:  " << (a.empty() ? "yes" : "no") << " (size " << a.size() << ")\n";

    a = Text("a is usable again");  // a moved-from object may be assigned to
    std::cout << a.c_str() << "\n";

    std::vector<Text> v;
    for (int i = 0; i < 5; ++i) v.push_back(Text("item"));  // the vector moves its elements when it grows (if the move is noexcept)
    std::cout << v.size() << " items\n";
}
