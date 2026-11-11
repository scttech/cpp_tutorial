#pragma once
// READ-ONLY. Tokens, errors, the variable environment and the counters that the Fast tests read.
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace expr {

namespace stats {
inline unsigned long long env_lookups = 0;  // calls of Env::find
inline unsigned long long env_copies = 0;   // copies of an Env
inline void reset() { env_lookups = env_copies = 0; }
}  // namespace stats

enum class Kind { Number, Ident, Plus, Minus, Star, Slash, Percent, Caret, LParen, RParen, Comma, End };

struct Token {
    Kind kind = Kind::End;
    std::string text;       // an identifier's name (empty for every other kind)
    double number = 0;      // a number's value
    std::size_t pos = 0;    // 0-based byte offset of the token in the text
};

enum class Error {
    None,
    BadCharacter,     // the lexer met a character that starts no token
    BadNumber,        // 1.  1e  1e+  or a number too large for a double
    UnexpectedToken,  // the parser met a token that cannot be here
    UnexpectedEnd,    // the input ended while something was still needed
    UnknownFunction,  // name( ... with a name that is not a function
    WrongArity,       // a function called with the wrong number of arguments
    TooDeep,          // parentheses / prefix operators / ^ chains nested more than kMaxDepth
};
inline constexpr int kMaxDepth = 200;

inline const char* name(Error e) {
    switch (e) {
        case Error::None: return "None";
        case Error::BadCharacter: return "BadCharacter";
        case Error::BadNumber: return "BadNumber";
        case Error::UnexpectedToken: return "UnexpectedToken";
        case Error::UnexpectedEnd: return "UnexpectedEnd";
        case Error::UnknownFunction: return "UnknownFunction";
        case Error::WrongArity: return "WrongArity";
        case Error::TooDeep: return "TooDeep";
    }
    return "?";
}

struct TokenizeResult {
    std::vector<Token> tokens;  // always ends with a Kind::End token (pos = text size) when there is no error
    Error error = Error::None;
    std::size_t pos = 0;        // 0-based offset of the problem
    std::string message;        // for people, never empty when there is an error
    bool ok() const { return error == Error::None; }
};

// The values of the variables.
class Env {
public:
    Env() = default;
    Env(const Env& other) : vars_(other.vars_) { ++stats::env_copies; }
    Env(Env&&) noexcept = default;
    Env& operator=(const Env& other) {
        vars_ = other.vars_;
        ++stats::env_copies;
        return *this;
    }
    Env& operator=(Env&&) noexcept = default;

    void set(std::string name, double value) { vars_[std::move(name)] = value; }
    // nullptr if there is no such variable. Every call counts as one lookup.
    const double* find(std::string_view name) const {
        ++stats::env_lookups;
        const auto it = vars_.find(name);
        return it == vars_.end() ? nullptr : &it->second;
    }

private:
    std::map<std::string, double, std::less<>> vars_;
};

enum class EvalError {
    None,
    UnknownVariable,  // detail: the name
    DivisionByZero,   // / or % with a right side of zero
    NotFinite,        // the result is not a finite number: sqrt(-1), pow(0,-1), 2^2000, ...
    BadCall,          // a call that does not exist or has the wrong number of arguments (detail: the name)
};

inline const char* name(EvalError e) {
    switch (e) {
        case EvalError::None: return "None";
        case EvalError::UnknownVariable: return "UnknownVariable";
        case EvalError::DivisionByZero: return "DivisionByZero";
        case EvalError::NotFinite: return "NotFinite";
        case EvalError::BadCall: return "BadCall";
    }
    return "?";
}

struct EvalResult {
    double value = 0;
    EvalError error = EvalError::None;
    std::string detail;
    bool ok() const { return error == EvalError::None; }
};

}  // namespace expr
