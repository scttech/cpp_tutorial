#pragma once

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

// A last-in, first-out stack of ANY element type. `Stack<int>`, `Stack<std::string>` and `Stack<std::unique_ptr<int>>`
// are three different classes that the compiler writes from this one recipe.
//
// The class is declared below. Its member functions are defined OUTSIDE the class, further down, as stubs that do nothing
// useful yet: write them. Each definition repeats `template <class T>` and names the class `Stack<T>::`:
//     template <class T>
//     void Stack<T>::push(const T& value) { ... }
// Templates must be defined in the header: the compiler needs the definition wherever it instantiates Stack<something>.
template <class T>
class Stack {
public:
    using value_type = T;
    using size_type = std::size_t;

    Stack() = default;

    // TODO: Stack s{1, 2, 3}; builds a stack holding 1, 2, 3 with 3 on top, and the compiler deduces the class: Stack<int>
    // (class template argument deduction). A constructor taking std::initializer_list<T> does it:
    // initialize items_ from it, in the member initializer list.
    Stack(std::initializer_list<T> items) { (void)items; }

    void push(const T& value);  // copies the value
    void push(T&& value);       // moves it
    std::optional<T> pop();     // the removed element, or an empty optional when the stack is empty
    T* top();                   // the top element, or nullptr when the stack is empty
    const T* top() const;
    bool empty() const;
    size_type size() const;
    void clear();

private:
    std::vector<T> items_;  // the back is the top
};

// ---- Member functions defined OUTSIDE the class (TODO: all of them) ----------------------------------------------------

template <class T>
void Stack<T>::push(const T& value) {
    (void)value;
}

template <class T>
void Stack<T>::push(T&& value) {
    (void)value;
}

// Hint: if the stack is empty return std::nullopt. Otherwise MOVE the top element into a std::optional<T> (do not copy it),
// remove it from the vector, and return the optional.
template <class T>
std::optional<T> Stack<T>::pop() {
    return std::nullopt;
}

template <class T>
T* Stack<T>::top() {
    return nullptr;
}

template <class T>
const T* Stack<T>::top() const {
    return nullptr;
}

template <class T>
bool Stack<T>::empty() const {
    return true;
}

// Hint: `size_type` is a name that depends on T, so the return type needs `typename`: typename Stack<T>::size_type
template <class T>
typename Stack<T>::size_type Stack<T>::size() const {
    return 0;
}

template <class T>
void Stack<T>::clear() {}
