// MUTANT: push inserts at the front: first-in, first-out.
#pragma once

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

// A last-in, first-out stack of ANY element type. `Stack<int>`, `Stack<std::string>` and `Stack<std::unique_ptr<int>>`
// are three different classes that the compiler writes from this one recipe.
template <class T>
class Stack {
public:
    using value_type = T;
    using size_type = std::size_t;

    Stack() = default;

    // Stack s{1, 2, 3};  (the compiler deduces the class: Stack<int>, with 3 on top. This is "CTAD".)
    Stack(std::initializer_list<T> items) : items_(items) {}

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

// ---- Member functions defined OUTSIDE the class ------------------------------------------------------------------------
// Each one repeats `template <class T>` and names the class as `Stack<T>::`. They must be in the header too:
// the compiler needs the definition wherever it instantiates Stack<something>.

template <class T>
void Stack<T>::push(const T& value) {
    items_.insert(items_.begin(), value);
}

template <class T>
void Stack<T>::push(T&& value) {
    items_.push_back(std::move(value));
}

template <class T>
std::optional<T> Stack<T>::pop() {
    if (items_.empty()) return std::nullopt;
    std::optional<T> removed(std::move(items_.back()));
    items_.pop_back();
    return removed;
}

template <class T>
T* Stack<T>::top() {
    return items_.empty() ? nullptr : &items_.back();
}

template <class T>
const T* Stack<T>::top() const {
    return items_.empty() ? nullptr : &items_.back();
}

template <class T>
bool Stack<T>::empty() const {
    return items_.empty();
}

// `size_type` is a name that DEPENDS on T (it is a member of Stack<T>, which the compiler cannot look into until it knows T), so
// the compiler assumes it is a value unless you say it is a type: `typename`.
template <class T>
typename Stack<T>::size_type Stack<T>::size() const {
    return items_.size();
}

template <class T>
void Stack<T>::clear() {
    items_.clear();
}
