#pragma once
// READ-ONLY. The vocabulary of the tokenizer.
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace shell {

using Env = std::map<std::string, std::string>;  // variable name -> value

enum class TokenKind { Word, Operator };

struct Token {
    TokenKind kind;
    std::string text;  // for a Word: the final text, with quotes removed, escapes resolved and variables expanded
    std::size_t pos;   // offset in the input line where this token STARTS (the opening quote counts; for a word: its first piece)
};

enum class ErrorKind {
    None,
    UnterminatedSingleQuote,  // error_pos: the opening '
    UnterminatedDoubleQuote,  // error_pos: the opening "
    DanglingBackslash,        // error_pos: the backslash at the very end of the line
    UnterminatedBrace,        // error_pos: the $ of an unclosed ${
    BadSubstitution,          // error_pos: the $ of ${...} whose name is empty or not a valid identifier
};

struct Result {
    std::vector<Token> tokens;  // on an error: the tokens that were COMPLETE before the error; the unfinished word is dropped
    ErrorKind error = ErrorKind::None;
    std::size_t error_pos = 0;
    bool ok() const { return error == ErrorKind::None; }
};

inline const char* describe(ErrorKind kind) {
    switch (kind) {
        case ErrorKind::None: return "no error";
        case ErrorKind::UnterminatedSingleQuote: return "unterminated single quote";
        case ErrorKind::UnterminatedDoubleQuote: return "unterminated double quote";
        case ErrorKind::DanglingBackslash: return "backslash at end of line";
        case ErrorKind::UnterminatedBrace: return "unterminated ${";
        case ErrorKind::BadSubstitution: return "bad substitution";
    }
    return "?";
}

}  // namespace shell
