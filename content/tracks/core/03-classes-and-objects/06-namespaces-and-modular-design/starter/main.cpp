// A demo of your geometry library (read-only). Press Run to see what it prints.
#include <iostream>
#include <vector>

#include "geometry.hpp"

namespace g = geo;  // a namespace alias: a short local name for a long one

int main() {
    const g::Rect room{g::Point{0, 0}, 8, 5};
    const g::Rect closet{g::Point{6, 3}, 4, 4};
    const g::Rect hall{g::Point{8, 0}, 3, 5};

    std::cout << "room   ";
    std::cout << room;
    std::cout << ", area " << g::area(room) << "\n";
    std::cout << "closet ";
    std::cout << closet;
    std::cout << "\n";
    std::cout << "room overlaps closet: " << g::overlaps(room, closet) << "\n";
    std::cout << "room overlaps hall:   " << g::overlaps(room, hall) << "  (they only touch along an edge)\n";
    std::cout << "(7, 4) is in the room: " << g::contains(room, g::Point{7, 4}) << "\n";

    const std::vector<g::Point> spots{{2, 1}, {9, -2}, {4, 6}};
    std::cout << "bounding box of the spots: ";
    std::cout << g::bounding_box(spots);
    std::cout << "\n";
}
