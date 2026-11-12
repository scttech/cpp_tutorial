// Reading and writing CSV: quotes, embedded commas and newlines, and honest error messages. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "csv.hpp"

static const char* kind_name(csv::ErrorKind k) {
    switch (k) {
        case csv::ErrorKind::BadOptions: return "bad options";
        case csv::ErrorKind::UnterminatedQuote: return "a quote that never closes";
        case csv::ErrorKind::QuoteInUnquotedField: return "a quote inside an unquoted field";
        case csv::ErrorKind::TextAfterClosingQuote: return "text after a closing quote";
        case csv::ErrorKind::RaggedRecord: return "a record with the wrong number of fields";
    }
    return "?";
}

int main() {
    const std::string text =
        "name,quote,score\r\n"
        "Ada,\"The Analytical Engine \"\"weaves\"\" patterns\",9.5\r\n"
        "\"Smith, J.\",\"two\nlines\",7\r\n";
    const csv::ParseResult r = csv::parse(text);
    for (const auto& row : r.table) {
        std::printf("%zu fields:", row.size());
        for (const auto& f : row) std::printf(" [%s]", f.c_str());
        std::printf("\n");
    }
    if (r.error) std::printf("error: %s on line %zu\n", kind_name(r.error->kind), r.error->line);

    std::printf("\nwritten back:\n%s", csv::write(r.table, csv::Options{',', false, false, false}).c_str());

    for (const char* bad : {"a,b\n\"never closed,c\n", "a,b\nx\"y,z\n", "a,b\n\"x\"y,z\n"}) {
        const csv::ParseResult e = csv::parse(bad);
        if (e.error) std::printf("\n%s -> %s on line %zu (after %zu good record%s)\n", "input", kind_name(e.error->kind), e.error->line, e.table.size(), e.table.size() == 1 ? "" : "s");
    }
}
