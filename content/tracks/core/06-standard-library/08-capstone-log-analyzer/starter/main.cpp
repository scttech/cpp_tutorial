// A demo of your log analyzer (read-only). Press Run to see what it prints.
#include <iostream>

#include "log_analyzer.hpp"

int main() {
    const char* log =
        "2024-03-05T12:00:00 INFO web 200 Request served\n"
        "2024-03-05T12:15:30 WARN db 429 Slow query on the orders table\n"
        "2024-03-05T13:02:11 ERROR db 503 Connection refused to replica-2\n"
        "2024-03-05T13:02:40 ERROR db 503 Connection refused to replica-2\n"
        "this line is not a log line\n"
        "\n"
        "2024-03-05T13:40:00 ERROR web 404 Not found: /missing\n"
        "2024-03-05T14:30:10 DEBUG web 0 \r\n"
        "2024-02-30T10:00:00 INFO web 200 There is no February 30th\n"
        "2024-03-05T13:59:59 ERROR db 503 Connection refused again\n";

    if (auto e = parse_line("2024-03-05T13:02:11 ERROR db 503 Connection refused to replica-2")) {
        std::cout << "parsed: " << level_name(e->level) << " in " << e->component << ", code " << e->code << ", \"" << e->message << "\" at "
                  << format_time(e->time) << "\n\n";
    }
    std::cout << format_report(analyze(log));
}
