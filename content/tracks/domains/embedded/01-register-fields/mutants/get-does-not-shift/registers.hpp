// MUTANT: get returns the field in place
#pragma once
// Masks and shifts in Field; Register counts every hardware access and does each operation with the minimum number of them.
#include <cstddef>
#include <limits>

namespace regs {

template <class T, unsigned Offset, unsigned Width>
struct Field {
    static_assert(Width >= 1, "a field has at least one bit");
    static_assert(Offset + Width <= std::numeric_limits<T>::digits, "the field does not fit in the register");

    // Width may be the full width of T, where `1 << Width` would be undefined: build the mask by shifting an all-ones value right instead.
    static constexpr T max_value = static_cast<T>(~T{0}) >> (std::numeric_limits<T>::digits - Width);
    static constexpr T mask = static_cast<T>(max_value << Offset);

    static constexpr T get(T reg) { return static_cast<T>(reg & mask); }
    static constexpr T insert(T reg, T value) { return static_cast<T>((reg & static_cast<T>(~mask)) | ((value << Offset) & mask)); }
    static constexpr bool fits(T value) { return value <= max_value; }
};

template <class F, class T>
struct Assign {
    using field = F;
    T value;
};

template <class F, class T>
constexpr Assign<F, T> set(T value) {
    return Assign<F, T>{value};
}

template <class T>
class Register {
public:
    explicit Register(T reset = 0) : value_(reset) {}

    T read() {
        ++reads_;
        return value_;
    }
    void write(T v) {
        ++writes_;
        value_ = v;
    }
    T peek() const { return value_; }
    std::size_t reads() const { return reads_; }
    std::size_t writes() const { return writes_; }

    template <class F>
    T read_field() {
        return F::get(read());
    }

    template <class F>
    void write_field(T v) {
        write(F::insert(read(), v));
    }

    template <class... Assigns>
    void modify(Assigns... assigns) {
        T v = read();
        ((v = Assigns::field::insert(v, assigns.value)), ...);
        write(v);
    }

    void set_bits(T m) { write(static_cast<T>(read() | m)); }
    void clear_bits(T m) { write(static_cast<T>(read() & static_cast<T>(~m))); }
    void toggle_bits(T m) { write(static_cast<T>(read() ^ m)); }

    void write_one_to_clear(T m) { write(m); }

    template <class F>
    bool wait_for(T expected, unsigned max_polls) {
        for (unsigned i = 0; i < max_polls; ++i) {
            if (F::get(read()) == expected) return true;
        }
        return false;
    }

private:
    T value_;
    std::size_t reads_ = 0;
    std::size_t writes_ = 0;
};

}  // namespace regs
