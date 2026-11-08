# Class templates

You have used class templates since Module 2: `std::vector<int>`, `std::optional<std::string>`, `std::unique_ptr<Node>`. Writing one is the same
idea as a function template, with a placeholder for a type (or a value) in the class:

```cpp
template <class T>
class Stack {
public:
    void push(const T& value);
    ...
private:
    std::vector<T> items_;
};

Stack<int> a;                       // the compiler writes a class where T is int
Stack<std::string> b;               // and another one, a different type, where T is std::string
```

`Stack<int>` and `Stack<std::string>` are unrelated types. Only what you *use* is generated: a member function of a class template is
compiled only for the `T`s that call it, so `Stack<std::unique_ptr<int>>` works as long as you never call something that needs to copy.

## Members outside the class, and `typename`

Short member functions go inside the class. Longer ones are usually *declared* inside and *defined* below it, and for a template that definition needs two things:
the `template <class T>` prefix again, and the full name `Stack<T>::`:

```cpp
template <class T>
void Stack<T>::push(const T& value) { items_.push_back(value); }
```

These definitions stay **in the header**. This is the opposite of the habit from Module 1 (declaration in `.hpp`, definition in `.cpp`):
the compiler can only generate `Stack<double>` if it can see the definition at the point where you use it.

Names that **depend on `T`** cause one more rule. `Stack<T>::size_type` is a member of a class that does not exist until `T` is known, so the compiler cannot tell
whether it is a type or a value, and assumes a value. You tell it with `typename`:

```cpp
template <class T>
typename Stack<T>::size_type Stack<T>::size() const { return items_.size(); }
```

If you forget, Clang says *"missing 'typename' prior to dependent type name 'Stack<T>::size_type'"*, and it means exactly that.
(Inside the class, the plain name `size_type` is fine: the compiler is already looking inside `Stack<T>`.)

## Member typedefs and class template argument deduction (CTAD)

Standard containers define `value_type` and `size_type` so that generic code can ask "what do you hold?". Do the same with `using value_type = T;`.

Since C++17 you do not always have to write the argument: `Stack s{1, 2, 3};` deduces `Stack<int>` from a constructor that takes
`std::initializer_list<T>`. (`std::vector v{1, 2, 3};` works the same way.)

## Values as template parameters: `Matrix<T, R, C>`

Template parameters can also be **values**: `template <class T, std::size_t R, std::size_t C>`. The dimensions become part of the type, which gives you two things:

- The elements fit in a fixed `std::array<T, R * C>` **inside the object**: no heap allocation, no `new`, no leak, no pointer to chase.
- **Mistakes become compile errors.** A `Matrix<int, 2, 3>` times a `Matrix<int, 2, 3>` makes no sense, and the compiler refuses it:

```cpp
template <class T, std::size_t R, std::size_t K, std::size_t C>
Matrix<T, R, C> operator*(const Matrix<T, R, K>& a, const Matrix<T, K, C>& b);    // K must agree in both
```

`transposed()` of an R x C matrix is a C x R matrix, a different type, and the return type says so: `Matrix<T, C, R>`.

## Your task

Two headers, two classes. Both start as stubs.

**`stack.hpp`**: `Stack<T>`: `push(const T&)` and `push(T&&)` (copies or moves), `std::optional<T> pop()` (an empty optional if there is nothing),
`T* top()` and `const T* top() const` (`nullptr` if empty), `empty()`, `size()`, `clear()`, the typedefs `value_type` and `size_type`, and a constructor from
`std::initializer_list<T>` (the last element on top). Define the members outside the class, and use `typename` where the compiler needs it.
`pop` must *move* the element out.

**`matrix.hpp`**: `Matrix<T, R, C>` with `transposed()`, `identity()` (square only), `+=` and `+` for matrices of the same size, `operator*` for compatible sizes
and `==`. Elements start at zero.

The tests use both with many element types (`std::string`, move-only `std::unique_ptr`, `Stack<Stack<int>>`, `double`, `long long`), check that dimension mistakes do **not**
compile, and in the Fast tier check the moves and copies of a `Stack`, and that a `Matrix` never touches the heap.
