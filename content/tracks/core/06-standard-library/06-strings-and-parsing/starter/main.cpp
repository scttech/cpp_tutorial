// A demo of your text tools (read-only). Press Run to see what it prints.
#include <iostream>

#include "text_tools.hpp"

int main() {
    for (const char* text : {"42", "-7", "12abc", "", "+5", "99999999999"}) {
        const auto v = parse_int(text);
        std::cout << "parse_int(\"" << text << "\") = " << (v ? std::to_string(*v) : "nothing") << "\n";
    }
    for (const char* text : {"3.5", "1e3", "nan", "2.5x"}) {
        const auto v = parse_double(text);
        std::cout << "parse_double(\"" << text << "\") = " << (v ? std::to_string(*v) : "nothing") << "\n";
    }
    std::cout << "to_hex(255) = " << to_hex(255) << ", to_hex(0) = " << to_hex(0) << "\n";
    std::cout << "[" << format_row("ab", 42, 3.14159) << "]\n";
    std::cout << "[" << trim("  \t padded text \r\n") << "]\n";
    for (std::string_view field : split_fields("a,,b,", ',')) std::cout << "<" << field << ">";
    std::cout << "\n";
    if (auto e = parse_log_line("ERROR 503 Service unavailable\r\n")) {
        std::cout << "level " << e->level << ", code " << e->code << ", message \"" << e->message << "\"\n";
    } else {
        std::cout << "not a log line\n";
    }
}
