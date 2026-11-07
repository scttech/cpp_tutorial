#include <charconv>
#include <iostream>
#include <string>
#include <string_view>

// Reads lines of "<name> <age>" from stdin.
//   valid line:   print "<name> is <age> years old"
//   invalid line: print "Invalid line: <the line as read>"
//   blank line:   ignore
// At the end print "Valid: <V>, Invalid: <I>".

int main() {
    int valid = 0;
    int invalid = 0;

    // TODO: read lines with std::getline in the loop condition and process each one.

    std::cout << "Valid: " << valid << ", Invalid: " << invalid << "\n";
}
