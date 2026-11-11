#pragma once
// READ-ONLY. The result types of the parser.
#include <cstddef>
#include <string_view>
#include <vector>

namespace fix {

inline constexpr char kSoh = '\x01';  // the field separator of FIX messages (ASCII 1)

// One field of a message: "35=D" is tag 35 with the value "D". `value` is a VIEW into the text that was parsed: no copy is made,
// so it is only valid as long as that text is alive and unchanged.
struct Field {
    int tag = 0;
    std::string_view value;
};

enum class Error {
    None,
    Empty,             // no text at all
    MissingDelimiter,  // the message does not end with the field separator, or a field has no '='
    BadTag,            // a tag that is empty, has a non-digit, or does not fit an int / is 0 (position: the tag)
    BadBeginString,    // the first field is not 8=... (position 0)
    BadBodyLength,     // the second field is not 9=<digits>, or the body is not that long
    BadChecksum,       // the last field is not 10=<three digits>, or the sum does not match
    TrailingData,      // something after the checksum field
};

inline const char* name(Error e) {
    switch (e) {
        case Error::None: return "None";
        case Error::Empty: return "Empty";
        case Error::MissingDelimiter: return "MissingDelimiter";
        case Error::BadTag: return "BadTag";
        case Error::BadBeginString: return "BadBeginString";
        case Error::BadBodyLength: return "BadBodyLength";
        case Error::BadChecksum: return "BadChecksum";
        case Error::TrailingData: return "TrailingData";
    }
    return "?";
}

struct ParseResult {
    std::vector<Field> fields;  // in the order of the message, including 8, 9 and 10
    Error error = Error::None;
    std::size_t pos = 0;        // offset of the problem in the text (0-based)
    bool ok() const { return error == Error::None; }
};

}  // namespace fix
