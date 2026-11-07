#pragma once

#include <vector>

#include "circle.hpp"  // note: main.cpp ALSO includes circle.hpp, so circle.hpp needs an include guard

// `inline` is what makes it legal to define this function in a header: every translation unit that
// includes shapes.hpp gets an identical definition and the linker accepts that.
inline double total_area(const std::vector<Circle>& circles) {
    double sum = 0.0;
    for (const Circle& c : circles) {
        sum += area(c);
    }
    return sum;
}
