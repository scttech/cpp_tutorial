#pragma once
// YOUR FIX-STYLE PARSER. A message is a sequence of fields "tag=value" each ENDED by the separator SOH (kSoh):
//
//   8=FIX.4.4<SOH>9=<body length><SOH> ...fields... 10=<checksum><SOH>
//
// - the first field is 8=<BeginString> (any non-empty value), the second is 9=<number of bytes of the BODY>,
//   the last is 10=<three digits>. The body is everything after the 9=...<SOH> field up to (not including) the "10=" of the checksum.
// - the checksum is the sum of ALL bytes before the "10=" field (as unsigned bytes), modulo 256, written with exactly 3 digits.
// - a tag is one or more digits, an int > 0 that fits; a value may be EMPTY and may contain '=' (only the FIRST '=' of a field splits it)
//   but never SOH. Tags may repeat; keep every field, in order.
// - ZERO COPY: Field::value is a std::string_view into the input text. No std::string may be made for a value.
//
//   checksum_of(text)        the sum of the bytes, modulo 256 (an int 0..255)
//   parse(text)              the fields, or the first error with the offset where it was found:
//        Empty                no text
//        MissingDelimiter     a field is not ended by SOH (so the message does not end with SOH), or a field has no '=' (pos: the field)
//        BadTag               empty tag, a non-digit, 0, or too big for an int (pos: the start of the tag)
//        BadBeginString       the first field is not 8=<non-empty>        (pos 0)
//        BadBodyLength        the second field is not 9=<digits>, or the body length does not match the real one
//                             (pos: the start of the 9 field, or of the body when the length is wrong)
//        BadChecksum          the last field is not 10=<exactly 3 digits>, or the checksum is wrong (pos: the start of the 10 field)
//        TrailingData         fields after the 10 field cannot exist (the 10 field must be the last one): pos: the byte after it
//   find(result, tag)        pointer to the first field with that tag, or nullptr
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace fix {

inline int checksum_of(std::string_view text) {
    (void)text;
    return 0;  // TODO
}

inline ParseResult parse(std::string_view text) {
    ParseResult r;
    (void)text;
    r.error = Error::Empty;  // TODO
    return r;
}

inline const Field* find(const ParseResult& r, int tag) {
    (void)r;
    (void)tag;
    return nullptr;  // TODO
}

}  // namespace fix
