#pragma once
// READ-ONLY. The rounding modes.
namespace fin {

// What to do with the fraction when a division does not come out even.
enum class Rounding {
    Down,      // toward zero: 2.9 -> 2, -2.9 -> -2
    HalfUp,    // to the nearest; exactly half goes AWAY from zero: 2.5 -> 3, -2.5 -> -3
    HalfEven,  // to the nearest; exactly half goes to the EVEN neighbour ("banker's rounding"): 2.5 -> 2, 3.5 -> 4, -2.5 -> -2
};

}  // namespace fin
