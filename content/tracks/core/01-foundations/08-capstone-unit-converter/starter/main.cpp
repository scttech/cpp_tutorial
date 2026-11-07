#include <iostream>
#include <string>

// Unit Converter CLI. See the lesson text for the full specification.
//
// Suggested structure (you are free to organize it differently):
//   - a table mapping a unit name to its dimension and conversion factor
//   - a function that parses a number and rejects bad input (abc, 5x, nan, inf)
//   - a function that converts a value between two units via a base unit
//   - a loop that reads lines, splits them into words, and dispatches the command

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        // TODO: split the line into words, handle "convert" and "quit", report errors.
    }
}
