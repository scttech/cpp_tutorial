#pragma once
// READ-ONLY. The trading signal.
namespace ma {

enum class Signal {
    None,
    Buy,   // the fast average crossed ABOVE the slow one
    Sell,  // the fast average crossed BELOW the slow one
};

inline const char* name(Signal s) { return s == Signal::Buy ? "Buy" : s == Signal::Sell ? "Sell" : "None"; }

}  // namespace ma
