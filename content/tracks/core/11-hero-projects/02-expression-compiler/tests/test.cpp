#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <string>
#include <vector>

#include "eval.hpp"
#include "lexer.hpp"
#include "optimize.hpp"
#include "parser.hpp"
#include "zth_test.hpp"

using namespace expr;

// shortens a text for a failure message (a local helper: zth::shorten is not available in every build)
inline std::string clip(std::string s) {
    for (char& c : s) {
        if (c == '\n') c = ' ';
    }
    return s.size() > 90 ? s.substr(0, 87) + "..." : s;
}

namespace {

std::string show(const std::string& t) { return clip(t); }

ParseResult compile(const std::string& text) {
    TokenizeResult t = tokenize(text);
    if (!t.ok()) {
        ParseResult r;
        r.error = t.error;
        r.pos = t.pos;
        r.message = t.message;
        return r;
    }
    return parse(t.tokens);
}

std::string tree(const std::string& text) {
    ParseResult r = compile(text);
    if (!r.ok()) {
        ::zth::fail("compile(" + show(text) + ") reported " + name(r.error) + " at " + std::to_string(r.pos) + " (" + r.message + ")");
        return "";
    }
    return to_string(*r.ast);
}

std::string opt(const std::string& text) {
    ParseResult r = compile(text);
    if (!r.ok()) {
        ::zth::fail("compile(" + show(text) + ") reported " + name(r.error) + " at " + std::to_string(r.pos));
        return "";
    }
    return to_string(*optimize(std::move(r.ast)));
}

void expect_tree(const std::string& text, const std::string& want) {
    const std::string got = tree(text);
    if (got != want) ::zth::fail("the tree of " + show(text) + " is " + got + ", expected " + want);
}
void expect_opt(const std::string& text, const std::string& want) {
    const std::string got = opt(text);
    if (got != want) ::zth::fail("optimize(" + show(text) + ") gave " + got + ", expected " + want);
}

void expect_error(const std::string& text, Error e, std::size_t pos) {
    const ParseResult r = compile(text);
    const std::string want = std::string(name(e)) + " at " + std::to_string(pos);
    const std::string got = r.ok() ? std::string("success (") + to_string(*r.ast) + ")" : std::string(name(r.error)) + " at " + std::to_string(r.pos);
    if (want != got) ::zth::fail("compile(" + show(text) + "): expected " + want + ", got " + got);
    else if (r.message.empty()) ::zth::fail("compile(" + show(text) + "): the error has no message");
}

Env standard_env() {
    Env e;
    e.set("x", 2);
    e.set("y", 3);
    e.set("z", -4);
    e.set("half", 0.5);
    return e;
}

EvalResult run(const std::string& text, const Env& env = standard_env()) {
    ParseResult r = compile(text);
    if (!r.ok()) {
        ::zth::fail("compile(" + show(text) + ") reported " + name(r.error) + " at " + std::to_string(r.pos));
        return EvalResult{};
    }
    return evaluate(*r.ast, env);
}

void expect_value(const std::string& text, double want) {
    const EvalResult r = run(text);
    if (!r.ok()) ::zth::fail(show(text) + " evaluated to the error " + name(r.error) + " " + r.detail + ", expected " + std::to_string(want));
    else if (std::fabs(r.value - want) > 1e-9 * (1 + std::fabs(want))) ::zth::fail(show(text) + " = " + std::to_string(r.value) + ", expected " + std::to_string(want));
}

void expect_eval_error(const std::string& text, EvalError e, const std::string& detail = "") {
    const EvalResult r = run(text);
    if (r.error != e || r.detail != detail) ::zth::fail(show(text) + ": expected the error " + name(e) + " '" + detail + "', got " + name(r.error) + " '" + r.detail + "' (value " + std::to_string(r.value) + ")");
}

std::string kinds(const std::string& text) {
    const TokenizeResult t = tokenize(text);
    if (!t.ok()) return std::string("error ") + name(t.error) + " at " + std::to_string(t.pos);
    std::string s;
    for (const Token& tok : t.tokens) {
        static const char* names[] = {"num", "id", "+", "-", "*", "/", "%", "^", "(", ")", ",", "end"};
        s += names[static_cast<int>(tok.kind)];
        s += '@' + std::to_string(tok.pos) + ' ';
    }
    if (!s.empty()) s.pop_back();
    return s;
}

std::string long_name(int i) { return "some_long_variable_name_" + std::to_string(i); }

// (v0 + v1 + ... + v49) + (v50 + ...) + ... : n groups of 50 variables with long names
std::string big_expression(int groups, int per_group = 50) {
    std::string s;
    int k = 0;
    for (int g = 0; g < groups; ++g) {
        s += g ? " + (" : "(";
        for (int i = 0; i < per_group; ++i) s += (i ? " + " : "") + long_name(k++);
        s += ")";
    }
    return s;
}

std::string constant_expression(int groups, int per_group = 50) {
    std::string s;
    for (int g = 0; g < groups; ++g) {
        s += g ? " + (" : "(";
        for (int i = 0; i < per_group; ++i) s += (i ? " + " : "") + std::to_string(i + 1);
        s += ")";
    }
    return s;
}

// a random expression over x, y, z, w (w is never defined), numbers 0..3, and every operator
std::string random_expression(::zth::Lcg& rng, int depth) {
    const int pick = rng.below(depth > 3 ? 3 : 9);
    switch (pick) {
        case 0: return std::to_string(rng.below(4));
        case 1: return std::string(1, "xyzw"[rng.below(4)]);
        case 2: return std::to_string(rng.below(3)) + "." + std::to_string(rng.below(10));
        case 3: return "(" + random_expression(rng, depth + 1) + ")";
        case 4: return "-" + random_expression(rng, depth + 1);
        case 5: {
            static const char* fn[] = {"sqrt", "abs", "floor", "ceil"};
            return std::string(fn[rng.below(4)]) + "(" + random_expression(rng, depth + 1) + ")";
        }
        case 6: {
            static const char* fn[] = {"min", "max", "pow"};
            return std::string(fn[rng.below(3)]) + "(" + random_expression(rng, depth + 1) + ", " + random_expression(rng, depth + 1) + ")";
        }
        default: {
            static const char* ops[] = {"+", "-", "*", "/", "%", "^", "+", "-", "*"};
            return random_expression(rng, depth + 1) + " " + ops[rng.below(9)] + " " + random_expression(rng, depth + 1);
        }
    }
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "tokens: numbers, names, operators, and where they start") {
    ZTH_EXPECT_EQ(kinds("1+2"), std::string("num@0 +@1 num@2 end@3"));
    ZTH_EXPECT_EQ(kinds("  x * (y_2 - 10)"), std::string("id@2 *@4 (@6 id@7 -@11 num@13 )@15 end@16"));
    ZTH_EXPECT_EQ(kinds("max(1, 2)"), std::string("id@0 (@3 num@4 ,@5 num@7 )@8 end@9"));
    ZTH_EXPECT_EQ(kinds("a%b^c/d"), std::string("id@0 %@1 id@2 ^@3 id@4 /@5 id@6 end@7"));
    ZTH_EXPECT_EQ(kinds(""), std::string("end@0"));
    ZTH_EXPECT_EQ(kinds("\t\n x \r\n"), std::string("id@3 end@7"));
}
ZTH_TEST("works", "tokens: the value of a number and the name of an identifier") {
    const TokenizeResult t = tokenize("12 3.5 1e3 2E-2 4.5e+1 foo Bar_9");
    ZTH_EXPECT_TRUE(t.ok());
    ZTH_EXPECT_EQ(t.tokens.size(), std::size_t{8});
    ZTH_EXPECT_EQ(t.tokens[0].number, 12.0);
    ZTH_EXPECT_EQ(t.tokens[1].number, 3.5);
    ZTH_EXPECT_EQ(t.tokens[2].number, 1000.0);
    ZTH_EXPECT_EQ(t.tokens[3].number, 0.02);
    ZTH_EXPECT_EQ(t.tokens[4].number, 45.0);
    ZTH_EXPECT_EQ(t.tokens[5].text, std::string("foo"));
    ZTH_EXPECT_EQ(t.tokens[6].text, std::string("Bar_9"));
    ZTH_EXPECT_TRUE(t.tokens[7].kind == Kind::End);
}
ZTH_TEST("works", "parser: precedence (* before +, ^ before *)") {
    expect_tree("1+2*3", "(1 + (2 * 3))");
    expect_tree("1*2+3", "((1 * 2) + 3)");
    expect_tree("2^3*4", "((2 ^ 3) * 4)");
    expect_tree("2*3^4", "(2 * (3 ^ 4))");
    expect_tree("a%b*c", "((a % b) * c)");
    expect_tree("a+b%c", "(a + (b % c))");
    expect_tree("(1+2)*3", "((1 + 2) * 3)");
}
ZTH_TEST("works", "parser: - + * / % are left-associative, ^ is right-associative") {
    expect_tree("1-2-3", "((1 - 2) - 3)");
    expect_tree("10/2/5", "((10 / 2) / 5)");
    expect_tree("8%5%2", "((8 % 5) % 2)");
    expect_tree("1+2+3", "((1 + 2) + 3)");
    expect_tree("2^3^2", "(2 ^ (3 ^ 2))");
    expect_tree("1-2+3", "((1 - 2) + 3)");
}
ZTH_TEST("works", "parser: unary minus and plus") {
    expect_tree("-x", "(-x)");
    expect_tree("--x", "(-(-x))");
    expect_tree("- -3", "(-(-3))");
    expect_tree("+5", "5");
    expect_tree("1 - -1", "(1 - (-1))");
    expect_tree("2*-3", "(2 * (-3))");
    expect_tree("-(1+2)", "(-(1 + 2))");
    expect_tree("+-x", "(-x)");
}
ZTH_TEST("works", "parser: a minus in front binds weaker than ^: -2^2 is -(2^2); the exponent may have its own minus") {
    expect_tree("-2^2", "(-(2 ^ 2))");
    expect_tree("-x^2", "(-(x ^ 2))");
    expect_tree("2^-1", "(2 ^ (-1))");
    expect_tree("2^-3^2", "(2 ^ (-(3 ^ 2)))");
    expect_tree("(-2)^2", "((-2) ^ 2)");
}
ZTH_TEST("works", "parser: function calls and variables") {
    expect_tree("sqrt(16)", "sqrt(16)");
    expect_tree("max(1, 2+3)", "max(1, (2 + 3))");
    expect_tree("min(x,y)*2", "(min(x, y) * 2)");
    expect_tree("-sqrt(4)", "(-sqrt(4))");
    expect_tree("sqrt(2)^2", "(sqrt(2) ^ 2)");
    expect_tree("pow(2, max(1, 3))", "pow(2, max(1, 3))");
    expect_tree("sqrt", "sqrt");  // a function name without parentheses is just a variable
    expect_tree("abs(-x)", "abs((-x))");
}
ZTH_TEST("works", "numbers print without noise") {
    expect_tree("1.50", "1.5");
    expect_tree("1e3", "1000");
    expect_tree("0.1", "0.1");
}
ZTH_TEST("works", "evaluate: arithmetic and precedence") {
    expect_value("1+2*3", 7);
    expect_value("(1+2)*3", 9);
    expect_value("10-4-3", 3);
    expect_value("100/10/5", 2);
    expect_value("2^3^2", 512);
    expect_value("-2^2", -4);
    expect_value("2^-1", 0.5);
    expect_value("7 % 4", 3);
    expect_value("-7 % 3", -1);
    expect_value("7 % -3", 1);
    expect_value("5.5 % 2", 1.5);
    expect_value("--3", 3);
    expect_value("1.5*4", 6);
}
ZTH_TEST("works", "evaluate: variables and functions") {
    expect_value("x*x + y", 7);
    expect_value("z", -4);
    expect_value("half*x", 1);
    expect_value("sqrt(16) + abs(-3)", 7);
    expect_value("floor(2.7) + ceil(2.1)", 5);
    expect_value("floor(-2.5)", -3);
    expect_value("ceil(-2.5)", -2);
    expect_value("min(3, 1) + max(3, 1)", 4);
    expect_value("pow(2, 10)", 1024);
    expect_value("pow(x, y)", 8);
    expect_value("max(min(10, 4), 2)", 4);
    expect_value("0^0", 1);
}
ZTH_TEST("works", "optimize: constants are folded") {
    expect_opt("1+2*3", "7");
    expect_opt("2^10", "1024");
    expect_opt("-(2+3)", "-5");
    expect_opt("sqrt(16)", "4");
    expect_opt("max(1, 2) + min(5, 4)", "6");
    expect_opt("x*(2+3)", "(x * 5)");
    expect_opt("2*3*x", "(6 * x)");
    expect_opt("1+1+x", "(2 + x)");
    expect_opt("y + (10 - 4) / 2", "(y + 3)");
}
ZTH_TEST("works", "optimize: identities that keep the other operand") {
    expect_opt("x+0", "x");
    expect_opt("0+x", "x");
    expect_opt("x-0", "x");
    expect_opt("x*1", "x");
    expect_opt("1*x", "x");
    expect_opt("x/1", "x");
    expect_opt("x^1", "x");
    expect_opt("0-x", "(-x)");
    expect_opt("--x", "x");
    expect_opt("(x+0)*(1*y)", "(x * y)");
    expect_opt("x + (1 - 1)", "x");
    expect_opt("((x*1)+0)^1", "x");
}
ZTH_TEST("works", "optimize: what must NOT change") {
    expect_opt("x+y", "(x + y)");
    expect_opt("x*y+1", "((x * y) + 1)");
    expect_opt("x*0", "(x * 0)");
    expect_opt("0*x", "(0 * x)");
    expect_opt("x^0", "(x ^ 0)");
    expect_opt("0/x", "(0 / x)");
    expect_opt("(x+1)+2", "((x + 1) + 2)");
    expect_opt("x*2*3", "((x * 2) * 3)");
    expect_opt("x-x", "(x - x)");
    expect_opt("1-x", "(1 - x)");
    expect_opt("1/x", "(1 / x)");
    expect_opt("1^x", "(1 ^ x)");
    expect_opt("2*x", "(2 * x)");
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "lexer errors: a character that starts no token (BadCharacter)") {
    expect_error("1 # 2", Error::BadCharacter, 2);
    expect_error("$x", Error::BadCharacter, 0);
    expect_error("x = 1", Error::BadCharacter, 2);
    expect_error(".5", Error::BadCharacter, 0);
    expect_error("1.2.3", Error::BadCharacter, 3);
    expect_error("a < b", Error::BadCharacter, 2);
    expect_error("\"x\"", Error::BadCharacter, 0);
    expect_error("(1 + 2)!", Error::BadCharacter, 7);
    expect_error("caf\xC3\xA9", Error::BadCharacter, 3);
}
ZTH_TEST("robust", "lexer errors: the number grammar (BadNumber, where a digit was needed; too large: at its start)") {
    expect_error("1.", Error::BadNumber, 2);
    expect_error("1.x", Error::BadNumber, 2);
    expect_error("3.e2", Error::BadNumber, 2);
    expect_error("1e", Error::BadNumber, 2);
    expect_error("1e+", Error::BadNumber, 3);
    expect_error("2 * 1E-", Error::BadNumber, 7);
    expect_error("1e999", Error::BadNumber, 0);
    expect_error("x + 1e400", Error::BadNumber, 4);
    ZTH_EXPECT_EQ(kinds("007"), std::string("num@0 end@3"));
    const TokenizeResult seven = tokenize("007");
    ZTH_EXPECT_EQ(seven.tokens[0].number, 7.0);
}
ZTH_TEST("robust", "lexer: a name may contain digits and underscores, and 2x is a number then a name") {
    ZTH_EXPECT_EQ(kinds("_a1b2"), std::string("id@0 end@5"));
    ZTH_EXPECT_EQ(kinds("2x"), std::string("num@0 id@1 end@2"));
}
ZTH_TEST("robust", "parser errors: the input ends too early (UnexpectedEnd, at the end)") {
    expect_error("", Error::UnexpectedEnd, 0);
    expect_error("   ", Error::UnexpectedEnd, 3);
    expect_error("1 +", Error::UnexpectedEnd, 3);
    expect_error("(1", Error::UnexpectedEnd, 2);
    expect_error("(1 + 2", Error::UnexpectedEnd, 6);
    expect_error("max(1", Error::UnexpectedEnd, 5);
    expect_error("max(1,", Error::UnexpectedEnd, 6);
    expect_error("max(", Error::UnexpectedEnd, 4);
    expect_error("-", Error::UnexpectedEnd, 1);
    expect_error("2^", Error::UnexpectedEnd, 2);
}
ZTH_TEST("robust", "parser errors: a token that cannot be here (UnexpectedToken, at that token)") {
    expect_error(")", Error::UnexpectedToken, 0);
    expect_error("1 2", Error::UnexpectedToken, 2);
    expect_error("1 + * 2", Error::UnexpectedToken, 4);
    expect_error("()", Error::UnexpectedToken, 1);
    expect_error("1+2)", Error::UnexpectedToken, 3);
    expect_error("max(1,)", Error::UnexpectedToken, 6);
    expect_error("max(1 2)", Error::UnexpectedToken, 6);
    expect_error("2x", Error::UnexpectedToken, 1);
    expect_error("* 3", Error::UnexpectedToken, 0);
    expect_error("(1, 2)", Error::UnexpectedToken, 2);
    expect_error("x y", Error::UnexpectedToken, 2);
    expect_error("1 +\n  )", Error::UnexpectedToken, 6);
}
ZTH_TEST("robust", "parser errors: functions (UnknownFunction / WrongArity, at the name)") {
    expect_error("foo(1)", Error::UnknownFunction, 0);
    expect_error("1 + bar(2)", Error::UnknownFunction, 4);
    expect_error("foo(", Error::UnknownFunction, 0);
    expect_error("max(1)", Error::WrongArity, 0);
    expect_error("2 * sqrt(1, 2)", Error::WrongArity, 4);
    expect_error("pow()", Error::WrongArity, 0);
    expect_error("abs()", Error::WrongArity, 0);
    expect_error("max(1, 2, 3)", Error::WrongArity, 0);
}
ZTH_TEST("robust", "nesting: 150 levels are fine; 100000 parentheses, minus signs or ^ are TooDeep, not a crash") {
    ZTH_EXPECT_TRUE(compile(std::string(150, '(') + "1" + std::string(150, ')')).ok());
    ZTH_EXPECT_TRUE(compile(std::string(150, '-') + "1").ok());
    {
        const ParseResult r = compile(std::string(100000, '(') + "1" + std::string(100000, ')'));
        ZTH_EXPECT_TRUE(r.error == Error::TooDeep);
        ZTH_EXPECT_FALSE(r.message.empty());
    }
    ZTH_EXPECT_TRUE(compile(std::string(100000, '-') + "1").error == Error::TooDeep);
    std::string pows = "1";
    for (int i = 0; i < 100000; ++i) pows += "^1";
    ZTH_EXPECT_TRUE(compile(pows).error == Error::TooDeep);
    std::string calls;
    for (int i = 0; i < 100000; ++i) calls += "abs(";
    ZTH_EXPECT_TRUE(compile(calls).error == Error::TooDeep);
}
ZTH_TEST("robust", "a long chain of + is fine (a loop, not recursion)") {
    std::string s = "1";
    for (int i = 0; i < 3000; ++i) s += " + 1";
    ParseResult r = compile(s);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(node_count(*r.ast), std::size_t{6001});
    ZTH_EXPECT_EQ(evaluate(*r.ast, Env()).value, 3001.0);
}
ZTH_TEST("robust", "evaluate errors: unknown variable (with its name), division by zero, not finite") {
    expect_eval_error("q + 1", EvalError::UnknownVariable, "q");
    expect_eval_error("sqrt", EvalError::UnknownVariable, "sqrt");
    expect_eval_error("1/0", EvalError::DivisionByZero);
    expect_eval_error("5 % 0", EvalError::DivisionByZero);
    expect_eval_error("x / (y - 3)", EvalError::DivisionByZero);
    expect_eval_error("1 / (0 * -1)", EvalError::DivisionByZero);
    expect_eval_error("sqrt(-1)", EvalError::NotFinite);
    expect_eval_error("pow(0, -1)", EvalError::NotFinite);
    expect_eval_error("2^2000", EvalError::NotFinite);
    expect_eval_error("(-8)^0.5", EvalError::NotFinite);
    expect_eval_error("1e308 * 10", EvalError::NotFinite);
    expect_eval_error("1e308 + 1e308", EvalError::NotFinite);
    expect_eval_error("-1e308 - 1e308", EvalError::NotFinite);
}
ZTH_TEST("robust", "evaluate: the FIRST error counts, left to right, inside calls too") {
    expect_eval_error("q + 1/0", EvalError::UnknownVariable, "q");
    expect_eval_error("1/0 + q", EvalError::DivisionByZero);
    expect_eval_error("max(1/0, q)", EvalError::DivisionByZero);
    expect_eval_error("max(q, 1/0)", EvalError::UnknownVariable, "q");
    expect_eval_error("(q) * (r)", EvalError::UnknownVariable, "q");
    expect_eval_error("2 * -q", EvalError::UnknownVariable, "q");
    expect_eval_error("r ^ q", EvalError::UnknownVariable, "r");
}
ZTH_TEST("robust", "evaluate: a hand-built tree with a bad call is BadCall, not a crash") {
    {
        std::vector<NodePtr> args;
        args.push_back(make_number(1));
        const EvalResult r = evaluate(*make_call("nope", std::move(args)), Env());
        ZTH_EXPECT_TRUE(r.error == EvalError::BadCall);
        ZTH_EXPECT_EQ(r.detail, std::string("nope"));
    }
    {
        std::vector<NodePtr> args;
        args.push_back(make_number(1));
        const EvalResult r = evaluate(*make_call("max", std::move(args)), Env());
        ZTH_EXPECT_TRUE(r.error == EvalError::BadCall);
    }
}
ZTH_TEST("robust", "evaluate: a failed evaluation reports value 0 and keeps the Env unchanged; the same tree can be evaluated twice") {
    ParseResult p = compile("x * y + 1");
    ZTH_EXPECT_TRUE(p.ok());
    Env a = standard_env();
    ZTH_EXPECT_EQ(evaluate(*p.ast, a).value, 7.0);
    a.set("x", 10);
    ZTH_EXPECT_EQ(evaluate(*p.ast, a).value, 31.0);
    {
        const EvalResult failed = evaluate(*p.ast, Env());
        ZTH_EXPECT_EQ(failed.detail, std::string("x"));
        ZTH_EXPECT_EQ(failed.value, 0.0);
        ZTH_EXPECT_TRUE(failed.error == EvalError::UnknownVariable);
    }
}
ZTH_TEST("robust", "optimize: errors survive folding (1/0, sqrt(-1), 2^2000 are not folded away or turned into numbers)") {
    expect_opt("1/0", "(1 / 0)");
    expect_opt("5 % 0", "(5 % 0)");
    expect_opt("sqrt(-1)", "sqrt(-1)");
    expect_opt("x + 1/0", "(x + (1 / 0))");
    expect_opt("(2^2000) + x", "((2 ^ 2000) + x)");
    expect_opt("pow(0, -1)", "pow(0, -1)");
    expect_opt("(0 * 1) / 0 + x", "((0 / 0) + x)");
    expect_opt("0^0 + x", "(1 + x)");
    expect_opt("-(1/0)", "(-(1 / 0))");
}
ZTH_TEST("robust", "optimize: an unknown variable stays an unknown variable (x*0 keeps its x)") {
    ParseResult p = compile("w * 0 + 1");
    ZTH_EXPECT_TRUE(p.ok());
    NodePtr o = optimize(std::move(p.ast));
    const EvalResult r = evaluate(*o, standard_env());
    ZTH_EXPECT_TRUE(r.error == EvalError::UnknownVariable);
    ZTH_EXPECT_EQ(r.detail, std::string("w"));
}
ZTH_TEST("robust", "optimize: other shapes (calls with a variable argument, nested folds, results that are zero or one)") {
    expect_opt("max(x, 2+3)", "max(x, 5)");
    expect_opt("sqrt(x*1)", "sqrt(x)");
    expect_opt("abs(0-x)", "abs((-x))");
    expect_opt("(1+2)*(3+4)", "21");
    expect_opt("x*(1+0)", "x");
    expect_opt("x*(2-1)", "x");
    expect_opt("(x+0)+(0+y)", "(x + y)");
    expect_opt("-(-(-x))", "(-x)");
    expect_opt("---x", "(-x)");
    expect_opt("x^(0+1)", "x");
    expect_opt("(0-0)-x", "(-x)");
}
ZTH_TEST_TIMEOUT("robust", "optimize: 500 random expressions give exactly the same result or the same error, with every environment", 20) {
    ::zth::Lcg rng(987654321);
    Env env = standard_env();
    int checked = 0;
    for (int i = 0; i < 500; ++i) {
        const std::string text = random_expression(rng, 0);
        ParseResult before = compile(text);
        ParseResult again = compile(text);
        if (!before.ok()) {
            ::zth::fail("the generator made an unparsable expression: " + show(text));
            return;
        }
        const EvalResult a = evaluate(*before.ast, env);
        NodePtr o = optimize(std::move(again.ast));
        const EvalResult b = evaluate(*o, env);
        if (a.error != b.error || a.detail != b.detail || (a.ok() && !(a.value == b.value))) {
            ::zth::fail("optimize changed the meaning of " + show(text) + ": before " + (a.ok() ? std::to_string(a.value) : name(a.error)) + ", after " + (b.ok() ? std::to_string(b.value) : name(b.error)) + " [" + to_string(*o) + "]");
            return;
        }
        if (node_count(*o) > node_count(*compile(text).ast)) {
            ::zth::fail("optimize made the tree bigger for " + show(text));
            return;
        }
        ++checked;
    }
    ZTH_EXPECT_EQ(checked, 500);
}
ZTH_TEST("robust", "optimize: running it twice changes nothing more (it reaches its result in one pass)") {
    for (const char* text : {"1+2*3+x*(4-4)", "x*1+0*y", "(x+0)*(2+3)-(1-1)", "--x+ - -y", "max(x,1)+min(2,3)*y^1"}) {
        ParseResult p = compile(text);
        ZTH_EXPECT_TRUE(p.ok());
        NodePtr once = optimize(std::move(p.ast));
        const std::string first = to_string(*once);
        NodePtr twice = optimize(std::move(once));
        ZTH_EXPECT_EQ(to_string(*twice), first);
    }
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "tokenizer: 100000 tokens with long names need at most one allocation per name (plus the vector growing)", 10) {
    const std::string text = big_expression(1000, 50);  // 50000 variables, 49000 plus signs and parentheses
    zth::mem::Scope scope;
    const TokenizeResult t = tokenize(text);
    ZTH_EXPECT_TRUE(t.ok());
    ZTH_EXPECT_TRUE(t.tokens.size() > 100000);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 50000 + 200);
}
ZTH_TEST_TIMEOUT("fast", "tokenizer + parser: 100000 tokens in time (nothing quadratic: no erasing from the front, no copying the rest)", 10) {
    const std::string text = big_expression(1000, 50);
    const TokenizeResult t = tokenize(text);
    ZTH_EXPECT_TRUE(t.ok());
    ParseResult p = parse(t.tokens);
    ZTH_EXPECT_TRUE(p.ok());
    ZTH_EXPECT_TRUE(node_count(*p.ast) > 90000);
}
ZTH_TEST_TIMEOUT("fast", "parser: one node and (for a variable) one name per leaf: at most 3 allocations per variable (50000 variables)", 10) {
    const std::string text = big_expression(1000, 50);
    const TokenizeResult t = tokenize(text);
    ZTH_EXPECT_TRUE(t.ok());
    zth::mem::Scope scope;
    ParseResult p = parse(t.tokens);
    ZTH_EXPECT_TRUE(p.ok());
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 50000 * 3 + 2000);
}
ZTH_TEST_TIMEOUT("fast", "evaluate: 1000 evaluations of an expression with 100 variables: no allocation, one lookup per variable, no Env copy", 10) {
    const TokenizeResult t = tokenize(big_expression(2, 50));
    ParseResult p = parse(t.tokens);
    ZTH_EXPECT_TRUE(p.ok());
    Env env;
    for (int i = 0; i < 100; ++i) env.set(long_name(i), i);
    stats::reset();
    double sum = 0;
    zth::mem::Scope scope;
    for (int round = 0; round < 1000; ++round) sum += evaluate(*p.ast, env).value;
    ZTH_EXPECT_EQ(sum, 1000.0 * 4950.0);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 10);
    ZTH_EXPECT_EQ(stats::env_lookups, 100000ULL);
    ZTH_EXPECT_EQ(stats::env_copies, 0ULL);
}
ZTH_TEST_TIMEOUT("fast", "optimize: a tree with nothing to simplify is rewritten in place (at most 50 allocations for 50000 nodes)", 10) {
    const TokenizeResult t = tokenize(big_expression(1000, 50));
    ParseResult p = parse(t.tokens);
    ZTH_EXPECT_TRUE(p.ok());
    const std::size_t before = node_count(*p.ast);
    zth::mem::Scope scope;
    NodePtr o = optimize(std::move(p.ast));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 50);
    ZTH_EXPECT_EQ(node_count(*o), before);
}
ZTH_TEST_TIMEOUT("fast", "optimize: a constant tree folds to one number, reusing nodes (at most 50 allocations)", 10) {
    const TokenizeResult t = tokenize(constant_expression(1000, 50));
    ParseResult p = parse(t.tokens);
    ZTH_EXPECT_TRUE(p.ok());
    ZTH_EXPECT_TRUE(node_count(*p.ast) > 90000);
    stats::reset();
    zth::mem::Scope scope;
    NodePtr o = optimize(std::move(p.ast));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 50);
    ZTH_EXPECT_EQ(stats::env_copies, 0ULL);
    ZTH_EXPECT_EQ(to_string(*o), std::string("1275000"));  // 1000 * (1 + 2 + ... + 50)
}
ZTH_TEST_TIMEOUT("fast", "optimize: identities do not copy the Env, names or subtrees (stats and allocations stay flat)", 10) {
    std::string s;
    for (int i = 0; i < 5000; ++i) s += (i ? " + (" : "(") + long_name(i) + " * 1 + 0)";
    const TokenizeResult t = tokenize(s);
    ParseResult p = parse(t.tokens);
    ZTH_EXPECT_TRUE(p.ok());
    zth::mem::Scope scope;
    NodePtr o = optimize(std::move(p.ast));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 50);
    ZTH_EXPECT_EQ(node_count(*o), std::size_t{9999});
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw new/delete/malloc/free in any of the four files (trees are owned by std::unique_ptr)") {
    ZTH_EXPECT_NO_RAW_MEMORY("lexer.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("parser.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("eval.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("optimize.hpp");
}
ZTH_TEST("clean", "errors are values: no throw, try, catch or goto") {
    for (const char* file : {"lexer.hpp", "parser.hpp", "eval.hpp", "optimize.hpp"}) {
        const std::string code = ::zth::strip_comments_and_literals(::zth::read_file(file));
        for (const char* word : {"throw", "try", "catch", "goto"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`: report problems with the Error / EvalError values");
                return;
            }
        }
    }
}
ZTH_TEST("clean", "no throwing conversions or streams: stod / stoi / atof / sscanf / stringstream") {
    for (const char* file : {"lexer.hpp", "parser.hpp", "eval.hpp", "optimize.hpp"}) {
        const std::string code = ::zth::strip_comments_and_literals(::zth::read_file(file));
        for (const char* word : {"stod", "stof", "stoi", "stol", "stoll", "atof", "atoi", "sscanf", "stringstream", "istringstream", "ostringstream"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`");
                return;
            }
        }
    }
}
ZTH_TEST("clean", "no dynamic_cast / typeid and no output: the pipeline only computes (the tree is a std::variant)") {
    for (const char* file : {"lexer.hpp", "parser.hpp", "eval.hpp", "optimize.hpp"}) {
        const std::string code = ::zth::strip_comments_and_literals(::zth::read_file(file));
        for (const char* word : {"dynamic_cast", "typeid", "cout", "cerr", "printf", "puts"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`");
                return;
            }
        }
    }
}
