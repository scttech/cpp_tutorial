#pragma once
// YOUR REGISTER LAYER for firmware: describe a hardware register as typed FIELDS and touch the hardware exactly as often as it must be touched.
//
//   Field<T, Offset, Width>                a run of Width bits starting at bit Offset of a T register (static_assert: Width >= 1 and it fits in T)
//     mask               the field's bits in place             max_value   the largest value the field can hold
//     get(reg)           the field's value, shifted down to bit 0
//     insert(reg, v)     reg with ONLY this field replaced by v; v is truncated to Width bits (it must never spill into the neighbours)
//     fits(v)            v <= max_value
//   Register<T>                            a simulated memory-mapped register; EVERY access through read()/write() is counted
//     Register(reset = 0), read() / write(v) (counted), peek() (not counted, for tests and debuggers), reads(), writes()
//     read_field<F>()                    one read
//     write_field<F>(v)                  read-modify-write: ONE read and ONE write
//     modify(set<F1>(v1), set<F2>(v2), ...)   several fields in ONE read and ONE write
//     set_bits(m) / clear_bits(m) / toggle_bits(m)   read-modify-write on raw bits (one read, one write)
//     write_one_to_clear(m)              for status registers where writing 1 clears a flag: ONE write of m and NO read
//                                        (a read-modify-write would write back the other pending flags and clear them too!)
//     wait_for<F>(expected, max_polls)   reads the register up to max_polls times; true as soon as the field equals `expected`
//                                        (stops reading at once); false after max_polls reads; with max_polls == 0 it does not touch the hardware
//   set<F>(v)                            a value for modify(): "field F gets v"
#include <cstddef>
#include <limits>

namespace regs {

template <class T, unsigned Offset, unsigned Width>
struct Field {
    static_assert(Width >= 1, "a field has at least one bit");
    static_assert(Offset + Width <= std::numeric_limits<T>::digits, "the field does not fit in the register");

    static constexpr T mask = 0;       // TODO
    static constexpr T max_value = 0;  // TODO

    static constexpr T get(T reg) {
        (void)reg;
        return 0;  // TODO
    }
    static constexpr T insert(T reg, T value) {
        (void)value;
        return reg;  // TODO
    }
    static constexpr bool fits(T value) {
        (void)value;
        return false;  // TODO
    }
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
        return value_;  // TODO: count it
    }
    void write(T v) {
        value_ = v;  // TODO: count it
    }
    T peek() const { return value_; }
    std::size_t reads() const { return 0; }   // TODO
    std::size_t writes() const { return 0; }  // TODO

    template <class F>
    T read_field() {
        return 0;  // TODO
    }
    template <class F>
    void write_field(T v) {
        (void)v;  // TODO
    }
    template <class... Assigns>
    void modify(Assigns... assigns) {
        (void)sizeof...(assigns);  // TODO
    }
    void set_bits(T m) { (void)m; }     // TODO
    void clear_bits(T m) { (void)m; }   // TODO
    void toggle_bits(T m) { (void)m; }  // TODO
    void write_one_to_clear(T m) { (void)m; }  // TODO
    template <class F>
    bool wait_for(T expected, unsigned max_polls) {
        (void)expected;
        (void)max_polls;
        return false;  // TODO
    }

private:
    T value_;
    // TODO: counters
};

}  // namespace regs
