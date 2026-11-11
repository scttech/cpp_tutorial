#pragma once
// YOUR LOADER. load_world(text) reads the world description (below) and returns the World, or the first problem with its line number.
//
//   # a comment            (a line whose first non-blank character is #; blank lines are ignored too)
//   start <room>
//   alias <word> <what it stands for ...>              alias n go north
//   room <id> "<description>" [dark] [win]
//   exit <direction> <room> [via <item>]               belongs to the room defined above it
//   item <id> "<name>" in <room | inventory> [portable] [switch on|off] [door locked|closed|open [key <item>]]
//
// Ids, directions and aliases are lower-case words: letters a-z, digits, '-' and '_'. A "quoted" text has no quote inside it.
// Indentation, tabs, trailing spaces and Windows line endings do not matter. Line numbers count every line, also blank and comment lines.
// Problems found while reading a line stop the loading at once; references to rooms/items that are never defined are checked
// at the end, and the one with the smallest line number is reported.
#include <string>
#include <string_view>
#include <vector>

#include "model.hpp"

namespace adv {

inline LoadResult load_world(std::string_view text) {
    LoadResult r;
    // TODO: go through the lines. This skeleton knows nothing: it reports the first non-blank line as unknown.
    std::size_t line_no = 0, pos = 0;
    while (pos <= text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        ++line_no;
        const std::size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string_view::npos || line[first] == '#') continue;
        r.error = LoadError::UnknownDirective;
        r.line = line_no;
        r.message = "not implemented yet";
        return r;
    }
    r.error = LoadError::MissingStart;
    r.message = "there is no start line";
    return r;
}

}  // namespace adv
