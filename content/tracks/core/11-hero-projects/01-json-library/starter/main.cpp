// Reads JSON from the input box (a sample if it is empty), parses it, and prints it pretty. Read-only. Press Run.
#include <iostream>
#include <iterator>
#include <string>

#include "parser.hpp"
#include "writer.hpp"

int main() {
    std::string text((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
    if (text.empty()) text = R"({"name": "Ada", "langs": ["C++", "Rust"], "age": 36, "admin": true, "boss": null, "pi": 3.14159})";

    const json::ParseResult r = json::parse(text);
    if (!r.ok()) {
        std::cout << "error " << json::name(r.error) << " at " << r.line << ":" << r.column << " - " << r.message << "\n";
        return 1;
    }
    std::cout << json::to_string(r.value, 2) << "\n";
    std::cout << json::to_string(r.value) << "\n";
}
