#pragma once
// READ-ONLY. The syntax tree, its printer, and the table of functions.
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "types.hpp"

namespace expr {

struct Node;
using NodePtr = std::unique_ptr<Node>;

struct Number { double value; };
struct Variable { std::string name; };
struct Unary { char op; NodePtr operand; };                      // op is always '-'
struct Binary { char op; NodePtr lhs; NodePtr rhs; };            // op is one of + - * / % ^
struct Call { std::string name; std::vector<NodePtr> args; };

struct Node {
    std::variant<Number, Variable, Unary, Binary, Call> v;
    explicit Node(Number x) : v(std::move(x)) {}
    explicit Node(Variable x) : v(std::move(x)) {}
    explicit Node(Unary x) : v(std::move(x)) {}
    explicit Node(Binary x) : v(std::move(x)) {}
    explicit Node(Call x) : v(std::move(x)) {}
};

inline NodePtr make_number(double d) { return std::make_unique<Node>(Number{d}); }
inline NodePtr make_variable(std::string name) { return std::make_unique<Node>(Variable{std::move(name)}); }
inline NodePtr make_unary(char op, NodePtr operand) { return std::make_unique<Node>(Unary{op, std::move(operand)}); }
inline NodePtr make_binary(char op, NodePtr lhs, NodePtr rhs) { return std::make_unique<Node>(Binary{op, std::move(lhs), std::move(rhs)}); }
inline NodePtr make_call(std::string name, std::vector<NodePtr> args) { return std::make_unique<Node>(Call{std::move(name), std::move(args)}); }

// The functions: sqrt abs floor ceil (1 argument), min max pow (2 arguments). -1 for a name that is not a function.
inline int arity_of(std::string_view name) {
    if (name == "sqrt" || name == "abs" || name == "floor" || name == "ceil") return 1;
    if (name == "min" || name == "max" || name == "pow") return 2;
    return -1;
}

struct ParseResult {
    NodePtr ast;  // null when there is an error
    Error error = Error::None;
    std::size_t pos = 0;
    std::string message;
    bool ok() const { return error == Error::None; }
};

// Fully parenthesized text of a tree, so that its shape is visible: (1 + (2 * x)), (-x), max(1, (2 + 3))
inline std::string to_string(const Node& n) {
    if (const auto* num = std::get_if<Number>(&n.v)) {
        char buf[40];
        std::snprintf(buf, sizeof buf, "%.12g", num->value);
        return buf;
    }
    if (const auto* var = std::get_if<Variable>(&n.v)) return var->name;
    if (const auto* un = std::get_if<Unary>(&n.v)) return std::string("(") + un->op + to_string(*un->operand) + ")";
    if (const auto* bin = std::get_if<Binary>(&n.v)) return "(" + to_string(*bin->lhs) + " " + bin->op + " " + to_string(*bin->rhs) + ")";
    const auto& call = std::get<Call>(n.v);
    std::string s = call.name + "(";
    for (std::size_t i = 0; i < call.args.size(); ++i) s += (i ? ", " : "") + to_string(*call.args[i]);
    return s + ")";
}

inline std::size_t node_count(const Node& n) {
    std::size_t c = 1;
    if (const auto* un = std::get_if<Unary>(&n.v)) c += node_count(*un->operand);
    else if (const auto* bin = std::get_if<Binary>(&n.v)) c += node_count(*bin->lhs) + node_count(*bin->rhs);
    else if (const auto* call = std::get_if<Call>(&n.v)) for (const auto& a : call->args) c += node_count(*a);
    return c;
}

}  // namespace expr
