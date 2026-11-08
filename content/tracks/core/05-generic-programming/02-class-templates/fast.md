# Fast path: what's different in C++

- **Class template**: `template <class T> class Stack { ... };`. `Stack<int>` and `Stack<std::string>` are different, unrelated classes. Member functions are instantiated only when used.
- Members defined **outside**: `template <class T> R Stack<T>::name(...) { ... }`, **in the header** (the compiler needs the definition wherever it instantiates the class).
- **`typename`** before a dependent type name: `typename Stack<T>::size_type`. Without it the compiler assumes a value ("missing 'typename' prior to dependent type name").
- Define `value_type` / `size_type` member typedefs, like the standard containers.
- **CTAD**: a constructor from `std::initializer_list<T>` lets `Stack s{1, 2, 3};` deduce `Stack<int>`.
- **Non-type template parameters** (`std::size_t R, C`) put values in the type: fixed-size storage (`std::array<T, R*C>`) inside the object, no heap, and dimension mistakes (`2x3 * 2x3`) fail to compile. A repeated parameter (`K` in `Matrix<T,R,K> * Matrix<T,K,C>`) enforces agreement.
- A member function that returns a different instantiation (`Matrix<T, C, R> transposed() const`) is fine.
- Move-only element types work as long as you never copy them: push the rvalue overload (`T&&`), return by moving.
