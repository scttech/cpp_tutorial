#pragma once
// YOUR PARSER. parse_command(line) turns one line of text into a Command (see types.hpp), or reports what is wrong and where.
//
//   SET key value | GET key | DEL key | COUNT value | RANGE lo hi | INCR key | BEGIN | ROLLBACK | COMMIT
//
// Command names are case-insensitive. Arguments are separated by spaces/tabs. An argument is a bare word (no quotes inside) or a
// "quoted string" that may contain spaces and the escapes \" \\ \n \t. Mistakes are reported in this order: a bad quote or
// escape anywhere in the line first (left to right), then an empty line, an unknown command, the wrong number of arguments.
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace db {

inline ParsedCommand parse_command(std::string_view line) {
    ParsedCommand p;
    // TODO: split the line into tokens (bare words and quoted strings), then build the Command.
    // This skeleton understands nothing but the empty line.
    p.error = ParseError::Empty;
    p.pos = line.size();
    p.message = "not implemented yet";
    return p;
}

}  // namespace db
