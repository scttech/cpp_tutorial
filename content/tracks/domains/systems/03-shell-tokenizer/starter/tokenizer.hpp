#pragma once
// YOUR TOKENIZER: split one command line into words and operators, like a (tiny) POSIX shell. ONE pass over the line.
//
//   shell::Result tokenize(std::string_view line, const shell::Env& env = {});
//
// RULES
//  * Space and tab separate words. Nothing else does.
//  * 'single quotes' keep everything literally until the next single quote (no escapes, no expansion).
//  * "double quotes" keep spaces and operators literally; a backslash escapes only  "  \  $  (before any other character the backslash stays);
//    $NAME and ${NAME} are expanded inside them.
//  * A backslash outside quotes makes the next character literal (it may be a space, a quote, an operator, #, $ ...).
//  * Pieces that touch are ONE word:  a"b c"d'e f'g  is the single word  ab cde fg.
//  * An empty quoted string is a real (empty) word:  echo "" ''  has three words.  But  a""b  is just  ab.
//  * Operators (unquoted, even without spaces around them) are their own tokens:  |  &&  ||  ;  >  >>  <  &
//    The longest match wins:  >>>  is  >>  followed by  >.
//  * # at the START of a word (unquoted) begins a comment that runs to the end of the line.  a#b is an ordinary word.
//  * $NAME ($ followed by a letter or _, then letters, digits, _) and ${NAME} are replaced by env[NAME]; an undefined name gives "".
//    The value is NOT split and NOT re-interpreted (spaces, quotes, | in it are ordinary characters).
//    An unquoted expansion that is EMPTY adds nothing (it can even make a word vanish); inside double quotes it still leaves an empty word.
//    A $ that starts no reference ($ at the end, $5, $-x, $ followed by space) is a literal $.  ${} and ${1x} are BadSubstitution.
//  * Errors: see types.hpp. On an error return what was complete so far and fill error / error_pos.
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace shell {

inline Result tokenize(std::string_view line, const Env& env = Env{}) {
    (void)line;
    (void)env;
    return Result{};  // TODO
}

}  // namespace shell
