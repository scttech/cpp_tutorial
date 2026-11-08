# Fast path: what's different in C++

- **Capacity in the type**: `template <class T, std::size_t N> class RingBuffer`. `N` is a *non-type template parameter* (a compile-time constant), so the elements can live in a `std::array<T, N>` **inside** the object: no heap, `sizeof` is known, a `RingBuffer<int, 8>` is 8 ints plus two indices.
- **Ring arithmetic**: keep `head_` (index of the oldest) and `size_`. Logical position `i` is slot `(head_ + i) % N`; the next free slot is `(head_ + size_) % N`. `size_ == 0` is empty, `size_ == N` is full (that is why a size is kept: `head == tail` cannot tell empty from full).
- **Two push overloads**: `push(const T&)` copy-assigns, `push(T&&)` move-assigns. **`emplace(Args&&...)`** forwards the arguments to a `T` constructor: `T(std::forward<Args>(args)...)`.
- **`pop()` returns `std::optional<T>`** (a value or nothing, no exception): `std::optional<T>(std::move(slot))` moves the element out exactly once.
- **Constrain the class** with `template <std::default_initializable T, std::size_t N> requires(N > 0) && std::movable<T>`.
- **Iterator class template**: one `Iter<bool Const>` for both iterators; it stores the owner and a logical index; `std::conditional_t<Const, const T&, T&>` chooses the reference type; a conversion operator with `requires(!Const)` allows iterator -> const_iterator only. The five typedefs (`iterator_category = std::forward_iterator_tag`, ...) make it work with `<algorithm>`.
- **`clear()` must release**: the slots stay alive in a `std::array`, so assign `T{}` to the live ones, otherwise a `shared_ptr` element keeps its object alive.
