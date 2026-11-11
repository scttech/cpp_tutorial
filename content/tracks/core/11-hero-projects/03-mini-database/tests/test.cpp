#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "database.hpp"
#include "zth_test.hpp"

using namespace db;

// shortens a text for a failure message (a local helper: zth::shorten is not available in every build)
inline std::string clip(std::string s) {
    for (char& c : s) {
        if (c == '\n') c = ' ';
    }
    return s.size() > 90 ? s.substr(0, 87) + "..." : s;
}

namespace {

std::string show(const std::string& t) {
    std::string s;
    for (char c : t) s += c == '\n' ? std::string("\\n") : c == '\t' ? std::string("\\t") : std::string(1, c);
    return clip(s);
}

// run commands and compare each reply with the expected description
struct Script {
    Database db;
    void step(const std::string& line, const std::string& want) {
        const std::string got = describe(db.execute(line));
        if (got != want) ::zth::fail("after `" + show(line) + "` the reply was " + show(got) + ", expected " + show(want));
    }
};

void expect_parse_error(const std::string& line, ParseError e, std::size_t pos) {
    const ParsedCommand p = parse_command(line);
    const std::string want = std::string(name(e)) + " at " + std::to_string(pos);
    const std::string got = p.ok() ? std::string("success") : std::string(name(p.error)) + " at " + std::to_string(p.pos);
    if (want != got) ::zth::fail("parse_command(" + show(line) + "): expected " + want + ", got " + got);
    else if (p.message.empty()) ::zth::fail("parse_command(" + show(line) + "): the error has no message");
}

Text T(const char* s) { return Text(s); }

template <class C>
bool is_cmd(const std::string& line) {
    const ParsedCommand p = parse_command(line);
    return p.ok() && std::holds_alternative<C>(p.command);
}

std::vector<std::string> keys_of(const std::vector<Text>& v) {
    std::vector<std::string> out;
    for (const Text& t : v) out.push_back(t.s);
    return out;
}

std::string keyname(int i) {
    std::string s = std::to_string(i);
    return "key-" + std::string(6 - std::min<std::size_t>(6, s.size()), '0') + s;
}

// A database with n keys "key-000000".. holding one of 100 different values
void fill(Database& d, int n) {
    for (int i = 0; i < n; ++i) d.execute("SET " + keyname(i) + " value-number-" + std::to_string(i % 100));
}

// The obviously-right model: a std::map and a stack of full snapshots.
struct Model {
    std::map<std::string, std::string> data;
    std::vector<std::map<std::string, std::string>> snapshots;

    Reply apply(const std::string& op, const std::string& a, const std::string& b) {
        if (op == "SET") { data[a] = b; return Reply::ok(); }
        if (op == "GET") { auto it = data.find(a); return it == data.end() ? Reply::nil() : Reply::value(it->second); }
        if (op == "DEL") return Reply::integer(static_cast<long long>(data.erase(a)));
        if (op == "COUNT") {
            long long n = 0;
            for (const auto& kv : data) n += kv.second == a;
            return Reply::integer(n);
        }
        if (op == "RANGE") {
            std::vector<std::string> keys;
            for (const auto& kv : data) if (kv.first >= a && kv.first <= b) keys.push_back(kv.first);
            return Reply::list(keys);
        }
        if (op == "INCR") {
            long long n = 0;
            auto it = data.find(a);
            if (it != data.end()) {
                const std::string& v = it->second;
                bool digits = !v.empty();
                for (std::size_t i = 0; i < v.size(); ++i) digits = digits && ((v[i] >= '0' && v[i] <= '9') || (i == 0 && v[i] == '-' && v.size() > 1));
                if (!digits || v.size() > 17) return Reply::error("value is not an integer");
                n = std::stoll(v);
            }
            data[a] = std::to_string(n + 1);
            return Reply::integer(n + 1);
        }
        if (op == "BEGIN") { snapshots.push_back(data); return Reply::ok(); }
        if (op == "ROLLBACK") {
            if (snapshots.empty()) return Reply::error("NO TRANSACTION");
            data = snapshots.back();
            snapshots.pop_back();
            return Reply::ok();
        }
        if (snapshots.empty()) return Reply::error("NO TRANSACTION");  // COMMIT
        snapshots.clear();
        return Reply::ok();
    }
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "parse_command: SET GET DEL COUNT RANGE INCR and the three transaction commands") {
    {
        ParsedCommand p = parse_command("SET name Ada");
        ZTH_EXPECT_TRUE(p.ok());
        auto* c = std::get_if<Set>(&p.command);
        ZTH_EXPECT_TRUE(c != nullptr);
        ZTH_EXPECT_EQ(c->key.s, std::string("name"));
        ZTH_EXPECT_EQ(c->value.s, std::string("Ada"));
    }
    {
        ParsedCommand p = parse_command("RANGE a  z");
        auto* c = std::get_if<Range>(&p.command);
        ZTH_EXPECT_TRUE(c != nullptr);
        ZTH_EXPECT_EQ(c->lo.s, std::string("a"));
        ZTH_EXPECT_EQ(c->hi.s, std::string("z"));
    }
    ZTH_EXPECT_TRUE(is_cmd<Get>("GET k"));
    ZTH_EXPECT_TRUE(is_cmd<Del>("DEL k"));
    ZTH_EXPECT_TRUE(is_cmd<Count>("COUNT v"));
    ZTH_EXPECT_TRUE(is_cmd<Incr>("INCR n"));
    ZTH_EXPECT_TRUE(is_cmd<Begin>("BEGIN"));
    ZTH_EXPECT_TRUE(is_cmd<Rollback>("ROLLBACK"));
    ZTH_EXPECT_TRUE(is_cmd<Commit>("COMMIT"));
}
ZTH_TEST("works", "parse_command: command names in any case, any amount of spaces and tabs, a trailing newline") {
    ZTH_EXPECT_TRUE(is_cmd<Set>("set a b"));
    ZTH_EXPECT_TRUE(is_cmd<Set>("SeT a b"));
    ZTH_EXPECT_TRUE(is_cmd<Get>("   \t get   \t k  "));
    ZTH_EXPECT_TRUE(is_cmd<Begin>("begin\r\n"));
    ParsedCommand keep = parse_command("set Key VALUE");  // the arguments keep their case
    auto* c = std::get_if<Set>(&keep.command);
    ZTH_EXPECT_TRUE(c != nullptr);
    ZTH_EXPECT_EQ(c->key.s, std::string("Key"));
    ZTH_EXPECT_EQ(c->value.s, std::string("VALUE"));
}
ZTH_TEST("works", "parse_command: quoted strings keep their spaces; \\\" \\\\ \\n \\t are unescaped") {
    {
        ParsedCommand p = parse_command("SET \"my key\" \"hello   world\"");
        auto* c = std::get_if<Set>(&p.command);
        ZTH_EXPECT_TRUE(c != nullptr);
        ZTH_EXPECT_EQ(c->key.s, std::string("my key"));
        ZTH_EXPECT_EQ(c->value.s, std::string("hello   world"));
    }
    {
        ParsedCommand p = parse_command("SET k \"a\\\"b\\\\c\\nd\\te\"");
        auto* c = std::get_if<Set>(&p.command);
        ZTH_EXPECT_TRUE(c != nullptr);
        ZTH_EXPECT_EQ(c->value.s, std::string("a\"b\\c\nd\te"));
    }
    {
        ParsedCommand p = parse_command("SET \"\" \"\"");
        auto* c = std::get_if<Set>(&p.command);
        ZTH_EXPECT_TRUE(c != nullptr);
        ZTH_EXPECT_EQ(c->key.s, std::string());
        ZTH_EXPECT_EQ(c->value.s, std::string());
    }
}
ZTH_TEST("works", "SET, GET and overwriting") {
    Script s;
    s.step("SET a 1", "OK");
    s.step("GET a", "\"1\"");
    s.step("GET b", "(nil)");
    s.step("SET a 2", "OK");
    s.step("GET a", "\"2\"");
    s.step("set b hello", "OK");
    s.step("get B", "(nil)");
    s.step("GET b", "\"hello\"");
}
ZTH_TEST("works", "DEL tells whether the key existed") {
    Script s;
    s.step("SET a 1", "OK");
    s.step("DEL a", "(integer) 1");
    s.step("DEL a", "(integer) 0");
    s.step("GET a", "(nil)");
    s.step("DEL never", "(integer) 0");
}
ZTH_TEST("works", "COUNT: how many keys hold a value, also after overwrites and deletes") {
    Script s;
    s.step("COUNT x", "(integer) 0");
    s.step("SET a x", "OK");
    s.step("SET b x", "OK");
    s.step("SET c y", "OK");
    s.step("COUNT x", "(integer) 2");
    s.step("COUNT y", "(integer) 1");
    s.step("SET b y", "OK");
    s.step("COUNT x", "(integer) 1");
    s.step("COUNT y", "(integer) 2");
    s.step("SET a x", "OK");
    s.step("COUNT x", "(integer) 1");
    s.step("DEL a", "(integer) 1");
    s.step("COUNT x", "(integer) 0");
    s.step("DEL a", "(integer) 0");
    s.step("COUNT y", "(integer) 2");
}
ZTH_TEST("works", "RANGE: keys in ascending order, both ends included") {
    Script s;
    for (const char* k : {"delta", "alpha", "echo", "charlie", "bravo"}) s.step(std::string("SET ") + k + " 1", "OK");
    s.step("RANGE alpha echo", "1) \"alpha\" 2) \"bravo\" 3) \"charlie\" 4) \"delta\" 5) \"echo\"");
    s.step("RANGE bravo delta", "1) \"bravo\" 2) \"charlie\" 3) \"delta\"");
    s.step("RANGE b d", "1) \"bravo\" 2) \"charlie\"");
    s.step("RANGE charlie charlie", "1) \"charlie\"");
    s.step("RANGE x z", "(empty list)");
    s.step("RANGE echo alpha", "(empty list)");
    s.step("RANGE a b", "1) \"alpha\"");
}
ZTH_TEST("works", "RANGE compares bytes: capitals sort before lower case, and a prefix before its extensions") {
    Script s;
    for (const char* k : {"b", "B", "a", "ab", "A", "abc"}) s.step(std::string("SET ") + k + " 1", "OK");
    s.step("RANGE A z", "1) \"A\" 2) \"B\" 3) \"a\" 4) \"ab\" 5) \"abc\" 6) \"b\"");
    s.step("RANGE a ab", "1) \"a\" 2) \"ab\"");
}
ZTH_TEST("works", "quoted keys and values in whole commands") {
    Script s;
    s.step("SET \"first name\" \"Ada Lovelace\"", "OK");
    s.step("GET \"first name\"", "\"Ada Lovelace\"");
    s.step("COUNT \"Ada Lovelace\"", "(integer) 1");
    s.step("SET k \"line1\\nline2\"", "OK");
    s.step("GET k", "\"line1\\nline2\"");
    s.step("SET q \"say \\\"hi\\\"\"", "OK");
    s.step("GET q", "\"say \\\"hi\\\"\"");
    s.step("SET \"\" empty", "OK");
    s.step("GET \"\"", "\"empty\"");
    s.step("SET SET GET", "OK");  // command names are fine as keys and values
    s.step("GET SET", "\"GET\"");
}
ZTH_TEST("works", "INCR: a missing key starts at 0") {
    Script s;
    s.step("INCR n", "(integer) 1");
    s.step("INCR n", "(integer) 2");
    s.step("GET n", "\"2\"");
    s.step("SET m 41", "OK");
    s.step("INCR m", "(integer) 42");
    s.step("SET neg -5", "OK");
    s.step("INCR neg", "(integer) -4");
    s.step("SET pad 007", "OK");
    s.step("INCR pad", "(integer) 8");
    s.step("COUNT 2", "(integer) 1");
}
ZTH_TEST("works", "transactions: BEGIN + changes + ROLLBACK undoes them, COMMIT keeps them") {
    Script s;
    s.step("SET a 1", "OK");
    s.step("BEGIN", "OK");
    s.step("SET a 2", "OK");
    s.step("SET b 3", "OK");
    s.step("GET a", "\"2\"");
    s.step("ROLLBACK", "OK");
    s.step("GET a", "\"1\"");
    s.step("GET b", "(nil)");
    s.step("BEGIN", "OK");
    s.step("SET a 5", "OK");
    s.step("COMMIT", "OK");
    s.step("GET a", "\"5\"");
    s.step("ROLLBACK", "ERR NO TRANSACTION");
}
ZTH_TEST("works", "transactions: ROLLBACK or COMMIT without BEGIN is the error NO TRANSACTION") {
    Script s;
    s.step("ROLLBACK", "ERR NO TRANSACTION");
    s.step("COMMIT", "ERR NO TRANSACTION");
    s.step("BEGIN", "OK");
    s.step("COMMIT", "OK");
    s.step("COMMIT", "ERR NO TRANSACTION");
}
ZTH_TEST("works", "transactions nest: ROLLBACK undoes only the innermost, COMMIT closes all of them") {
    Script s;
    s.step("BEGIN", "OK");
    s.step("SET a 1", "OK");
    s.step("BEGIN", "OK");
    s.step("SET a 2", "OK");
    s.step("SET b 9", "OK");
    s.step("ROLLBACK", "OK");
    s.step("GET a", "\"1\"");
    s.step("GET b", "(nil)");
    s.step("ROLLBACK", "OK");
    s.step("GET a", "(nil)");
    s.step("BEGIN", "OK");
    s.step("SET c 1", "OK");
    s.step("BEGIN", "OK");
    s.step("SET d 2", "OK");
    s.step("COMMIT", "OK");
    s.step("GET c", "\"1\"");
    s.step("GET d", "\"2\"");
    s.step("ROLLBACK", "ERR NO TRANSACTION");
}
ZTH_TEST("works", "rollback restores deleted keys and overwritten values, and COUNT follows") {
    Script s;
    s.step("SET a x", "OK");
    s.step("SET b x", "OK");
    s.step("SET c y", "OK");
    s.step("BEGIN", "OK");
    s.step("DEL a", "(integer) 1");
    s.step("SET b z", "OK");
    s.step("SET d x", "OK");
    s.step("COUNT x", "(integer) 1");
    s.step("COUNT z", "(integer) 1");
    s.step("ROLLBACK", "OK");
    s.step("COUNT x", "(integer) 2");
    s.step("COUNT z", "(integer) 0");
    s.step("GET a", "\"x\"");
    s.step("GET b", "\"x\"");
    s.step("GET d", "(nil)");
    s.step("RANGE a z", "1) \"a\" 2) \"b\" 3) \"c\"");
}
ZTH_TEST("works", "the Store used directly: set / get / del / count / range / size") {
    Store st;
    st.set(T("b"), T("1"));
    st.set(T("a"), T("1"));
    st.set(T("c"), T("2"));
    ZTH_EXPECT_EQ(st.size(), std::size_t{3});
    ZTH_EXPECT_TRUE(st.get(T("a")) != nullptr);
    ZTH_EXPECT_EQ(st.get(T("a"))->s, std::string("1"));
    ZTH_EXPECT_TRUE(st.get(T("zz")) == nullptr);
    ZTH_EXPECT_EQ(st.count(T("1")), std::size_t{2});
    ZTH_EXPECT_EQ(st.count(T("2")), std::size_t{1});
    ZTH_EXPECT_EQ(st.count(T("nothing")), std::size_t{0});
    ZTH_EXPECT_TRUE(st.del(T("a")));
    ZTH_EXPECT_FALSE(st.del(T("a")));
    ZTH_EXPECT_EQ(st.count(T("1")), std::size_t{1});
    ZTH_EXPECT_EQ(keys_of(st.range(T("a"), T("z"))), (std::vector<std::string>{"b", "c"}));
    ZTH_EXPECT_EQ(st.depth(), std::size_t{0});
    st.begin();
    st.begin();
    ZTH_EXPECT_EQ(st.depth(), std::size_t{2});
    ZTH_EXPECT_TRUE(st.rollback());
    ZTH_EXPECT_EQ(st.depth(), std::size_t{1});
    ZTH_EXPECT_TRUE(st.commit());
    ZTH_EXPECT_EQ(st.depth(), std::size_t{0});
    ZTH_EXPECT_FALSE(st.rollback());
    ZTH_EXPECT_FALSE(st.commit());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "parse errors: an empty line (Empty, at the end of the line)") {
    expect_parse_error("", ParseError::Empty, 0);
    expect_parse_error("   ", ParseError::Empty, 3);
    expect_parse_error("\t\r\n", ParseError::Empty, 3);
}
ZTH_TEST("robust", "parse errors: an unknown command (UnknownCommand, at the first token; a quoted name is not a command)") {
    expect_parse_error("FOO", ParseError::UnknownCommand, 0);
    expect_parse_error("  FOO bar", ParseError::UnknownCommand, 2);
    expect_parse_error("\"SET\" a b", ParseError::UnknownCommand, 0);
    expect_parse_error("SETS a b", ParseError::UnknownCommand, 0);
    expect_parse_error("SE a b", ParseError::UnknownCommand, 0);
    expect_parse_error("a SET b", ParseError::UnknownCommand, 0);
    expect_parse_error("DELETE a", ParseError::UnknownCommand, 0);
}
ZTH_TEST("robust", "parse errors: the wrong number of arguments (WrongArity, at the command)") {
    expect_parse_error("SET a", ParseError::WrongArity, 0);
    expect_parse_error("SET", ParseError::WrongArity, 0);
    expect_parse_error("SET a b c", ParseError::WrongArity, 0);
    expect_parse_error("  GET a b", ParseError::WrongArity, 2);
    expect_parse_error("GET", ParseError::WrongArity, 0);
    expect_parse_error("RANGE a", ParseError::WrongArity, 0);
    expect_parse_error("BEGIN now", ParseError::WrongArity, 0);
    expect_parse_error("COMMIT 1", ParseError::WrongArity, 0);
    expect_parse_error("COUNT", ParseError::WrongArity, 0);
    expect_parse_error("INCR a b", ParseError::WrongArity, 0);
    expect_parse_error("SET \"a b\" c d", ParseError::WrongArity, 0);
}
ZTH_TEST("robust", "parse errors: quotes (UnterminatedString at the opening quote, BadEscape at the backslash, StrayQuote at the offender)") {
    expect_parse_error("SET a \"abc", ParseError::UnterminatedString, 6);
    expect_parse_error("SET \"a\\", ParseError::UnterminatedString, 4);
    expect_parse_error("\"", ParseError::UnterminatedString, 0);
    expect_parse_error("SET a \"x\\qy\"", ParseError::BadEscape, 8);
    expect_parse_error("SET a \"\\x41\"", ParseError::BadEscape, 7);
    expect_parse_error("SET a b\"c\"", ParseError::StrayQuote, 7);
    expect_parse_error("SET \"a\"b c", ParseError::StrayQuote, 7);
    expect_parse_error("SET a\"", ParseError::StrayQuote, 5);
    expect_parse_error("SET \"a\"\"b\" c", ParseError::StrayQuote, 7);
}
ZTH_TEST("robust", "parse errors: a bad quote anywhere wins over an unknown command or the wrong arity") {
    expect_parse_error("FOO \"abc", ParseError::UnterminatedString, 4);
    expect_parse_error("GET a \"b\\q\"", ParseError::BadEscape, 8);
    expect_parse_error("BEGIN x\"y", ParseError::StrayQuote, 7);
}
ZTH_TEST("robust", "a line that does not parse is an error reply and changes nothing, also inside a transaction") {
    Script s;
    s.step("SET a 1", "OK");
    s.step("BEGIN", "OK");
    for (const char* bad : {"", "FOO", "SET a", "SET a \"x", "SET a b c", "GET", "RANGE a", "SET a \"\\q\""}) {
        const Reply r = s.db.execute(bad);
        ZTH_EXPECT_TRUE(r.kind == Reply::Kind::Error);
        ZTH_EXPECT_FALSE(r.text.empty());
    }
    s.step("GET a", "\"1\"");
    s.step("ROLLBACK", "OK");
    s.step("GET a", "\"1\"");
    ZTH_EXPECT_EQ(s.db.store().size(), std::size_t{1});
    ZTH_EXPECT_EQ(s.db.store().depth(), std::size_t{0});
}
ZTH_TEST("robust", "INCR: a value that is not a whole number is an error and nothing changes") {
    Script s;
    s.step("SET n abc", "OK");
    s.step("INCR n", "ERR value is not an integer");
    s.step("GET n", "\"abc\"");
    for (const char* bad : {"\"+5\"", "\" 5\"", "\"5 \"", "\"\"", "-", "1.5", "0x10", "1e3", "99999999999999999999", "-99999999999999999999"}) {
        s.step(std::string("SET n ") + bad, "OK");
        const Reply r = s.db.execute("INCR n");
        if (r.kind != Reply::Kind::Error || r.text != "value is not an integer") {
            ::zth::fail(std::string("INCR on the value ") + bad + " gave " + describe(r));
            return;
        }
    }
    s.step("COUNT 1.5", "(integer) 0");
}
ZTH_TEST("robust", "INCR: the extremes of long long") {
    Script s;
    s.step("SET n 9223372036854775806", "OK");
    s.step("INCR n", "(integer) 9223372036854775807");
    s.step("INCR n", "ERR increment would overflow");
    s.step("GET n", "\"9223372036854775807\"");
    s.step("SET n -9223372036854775808", "OK");
    s.step("INCR n", "(integer) -9223372036854775807");
    s.step("SET z -0", "OK");
    s.step("INCR z", "(integer) 1");
    s.step("COUNT -9223372036854775807", "(integer) 1");
}
ZTH_TEST("robust", "INCR inside a transaction is undone by ROLLBACK, and a failed INCR leaves nothing to undo") {
    Script s;
    s.step("SET n 5", "OK");
    s.step("BEGIN", "OK");
    s.step("INCR n", "(integer) 6");
    s.step("INCR fresh", "(integer) 1");
    s.step("SET text abc", "OK");
    s.step("INCR text", "ERR value is not an integer");
    s.step("ROLLBACK", "OK");
    s.step("GET n", "\"5\"");
    s.step("GET fresh", "(nil)");
    s.step("GET text", "(nil)");
    s.step("COUNT 6", "(integer) 0");
}
ZTH_TEST("robust", "rollback in detail: a key changed several times, deleted then re-created, created then deleted") {
    Script s;
    s.step("SET a 1", "OK");
    s.step("BEGIN", "OK");
    s.step("SET a 2", "OK");
    s.step("SET a 3", "OK");
    s.step("DEL a", "(integer) 1");
    s.step("SET a 4", "OK");
    s.step("SET fresh 1", "OK");
    s.step("DEL fresh", "(integer) 1");
    s.step("SET fresh2 1", "OK");
    s.step("SET fresh2 2", "OK");
    s.step("ROLLBACK", "OK");
    s.step("GET a", "\"1\"");
    s.step("GET fresh", "(nil)");
    s.step("GET fresh2", "(nil)");
    s.step("COUNT 1", "(integer) 1");
    s.step("COUNT 2", "(integer) 0");
    s.step("COUNT 3", "(integer) 0");
    s.step("COUNT 4", "(integer) 0");
    ZTH_EXPECT_EQ(s.db.store().size(), std::size_t{1});
}
ZTH_TEST("robust", "three levels of nesting: each ROLLBACK restores exactly its own level") {
    Script s;
    s.step("SET k v0", "OK");
    s.step("BEGIN", "OK");
    s.step("SET k v1", "OK");
    s.step("BEGIN", "OK");
    s.step("SET k v2", "OK");
    s.step("DEL k", "(integer) 1");
    s.step("BEGIN", "OK");
    s.step("SET k v3", "OK");
    s.step("ROLLBACK", "OK");
    s.step("GET k", "(nil)");
    s.step("ROLLBACK", "OK");
    s.step("GET k", "\"v1\"");
    s.step("COUNT v1", "(integer) 1");
    s.step("COUNT v3", "(integer) 0");
    s.step("ROLLBACK", "OK");
    s.step("GET k", "\"v0\"");
    s.step("COUNT v0", "(integer) 1");
    s.step("COUNT v1", "(integer) 0");
}
ZTH_TEST("robust", "after COMMIT a later ROLLBACK cannot undo it, and a new BEGIN starts a fresh level") {
    Script s;
    s.step("BEGIN", "OK");
    s.step("BEGIN", "OK");
    s.step("SET a 1", "OK");
    s.step("COMMIT", "OK");
    ZTH_EXPECT_EQ(s.db.store().depth(), std::size_t{0});
    s.step("BEGIN", "OK");
    s.step("SET a 2", "OK");
    s.step("ROLLBACK", "OK");
    s.step("GET a", "\"1\"");
    s.step("ROLLBACK", "ERR NO TRANSACTION");
}
ZTH_TEST("robust", "RANGE: one key, no keys, equal ends, ends that are not keys, empty strings") {
    Script s;
    s.step("RANGE a z", "(empty list)");
    s.step("SET m 1", "OK");
    s.step("RANGE a z", "1) \"m\"");
    s.step("RANGE m m", "1) \"m\"");
    s.step("RANGE n n", "(empty list)");
    s.step("RANGE \"\" z", "1) \"m\"");
    s.step("SET \"\" 2", "OK");
    s.step("RANGE \"\" \"\"", "1) \"\"");
    s.step("RANGE \"\" z", "1) \"\" 2) \"m\"");
}
ZTH_TEST("robust", "COUNT of the empty string, of values with spaces, and many keys with one value") {
    Script s;
    s.step("SET a \"\"", "OK");
    s.step("SET b \"\"", "OK");
    s.step("COUNT \"\"", "(integer) 2");
    s.step("SET c \"two words\"", "OK");
    s.step("COUNT \"two words\"", "(integer) 1");
    s.step("COUNT two", "(integer) 0");
    for (int i = 0; i < 300; ++i) s.db.execute("SET k" + std::to_string(i) + " same");
    s.step("COUNT same", "(integer) 300");
    for (int i = 0; i < 300; i += 2) s.db.execute("DEL k" + std::to_string(i));
    s.step("COUNT same", "(integer) 150");
}
ZTH_TEST("robust", "Store: range with lo > hi and with lo == hi; get of a deleted key; del twice") {
    Store st;
    st.set(T("a"), T("1"));
    st.set(T("b"), T("1"));
    ZTH_EXPECT_TRUE(st.range(T("b"), T("a")).empty());
    ZTH_EXPECT_EQ(keys_of(st.range(T("a"), T("a"))), (std::vector<std::string>{"a"}));
    ZTH_EXPECT_TRUE(st.del(T("a")));
    ZTH_EXPECT_TRUE(st.get(T("a")) == nullptr);
    ZTH_EXPECT_FALSE(st.del(T("a")));
    ZTH_EXPECT_EQ(st.size(), std::size_t{1});
}
ZTH_TEST("robust", "values with special characters survive (binary-ish bytes, a very long value)") {
    Script s;
    s.step("SET k \"tab\\there\"", "OK");
    s.step("GET k", "\"tab\\there\"");
    s.step("SET u \xC3\xA9\xF0\x9F\x98\x80", "OK");
    s.step("GET u", "\"\xC3\xA9\xF0\x9F\x98\x80\"");
    const std::string big(100000, 'x');
    s.step("SET big " + big, "OK");
    ZTH_EXPECT_EQ(s.db.execute("GET big").text.size(), std::size_t{100000});
    s.step("COUNT " + big, "(integer) 1");
}
ZTH_TEST_TIMEOUT("robust", "3000 random commands (including transactions and mistakes) give the same replies as a model that copies the whole map on BEGIN", 20) {
    ::zth::Lcg rng(424242);
    Database d;
    Model m;
    static const char* keys[] = {"a", "b", "c", "d", "e", "f"};
    static const char* vals[] = {"1", "2", "3", "x", "y", "-7"};
    for (int step = 0; step < 3000; ++step) {
        const int pick = rng.below(100);
        std::string op, a, b, line;
        if (pick < 28) { op = "SET"; a = keys[rng.below(6)]; b = vals[rng.below(6)]; line = "SET " + a + " " + b; }
        else if (pick < 42) { op = "GET"; a = keys[rng.below(6)]; line = "GET " + a; }
        else if (pick < 52) { op = "DEL"; a = keys[rng.below(6)]; line = "DEL " + a; }
        else if (pick < 62) { op = "COUNT"; a = vals[rng.below(6)]; line = "COUNT " + a; }
        else if (pick < 68) { op = "RANGE"; a = keys[rng.below(6)]; b = keys[rng.below(6)]; line = "RANGE " + a + " " + b; }
        else if (pick < 74) { op = "INCR"; a = keys[rng.below(6)]; line = "INCR " + a; }
        else if (pick < 84) { op = "BEGIN"; line = "BEGIN"; }
        else if (pick < 94) { op = "ROLLBACK"; line = "ROLLBACK"; }
        else if (pick < 98) { op = "COMMIT"; line = "COMMIT"; }
        else {
            const Reply r = d.execute("SET onlyone");  // a mistake: must change nothing
            ZTH_EXPECT_TRUE(r.kind == Reply::Kind::Error);
            continue;
        }
        const std::string got = describe(d.execute(line));
        const std::string want = describe(m.apply(op, a, b));
        if (got != want) {
            ::zth::fail("step " + std::to_string(step) + ": `" + line + "` gave " + got + ", the model says " + want);
            return;
        }
    }
    ZTH_EXPECT_EQ(d.store().size(), m.data.size());
    ZTH_EXPECT_EQ(d.store().depth(), m.snapshots.size());
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "SET moves keys and values into the store: at most one Text copy per SET on average (4000 SETs)", 10) {
    Database d;
    probe::reset();
    fill(d, 4000);
    ZTH_EXPECT_EQ(d.store().size(), std::size_t{4000});
    ZTH_EXPECT_TRUE(probe::copies <= 4000);
}
ZTH_TEST_TIMEOUT("fast", "SET and GET find their key in an ordered tree: at most 30 comparisons per GET and 25 per SET (4000 keys)", 10) {
    Database d;
    fill(d, 4000);
    probe::reset();
    for (int i = 0; i < 4000; ++i) d.execute("GET " + keyname(i));
    ZTH_EXPECT_TRUE(probe::compares <= 4000ULL * 30);
    ZTH_EXPECT_EQ(probe::copies, 0ULL);  // looking a value up does not copy a Text
    probe::reset();
    for (int i = 0; i < 4000; ++i) d.execute("SET " + keyname(i) + " other-value-number-" + std::to_string(i % 50));
    ZTH_EXPECT_TRUE(probe::compares <= 4000ULL * 25);
}
ZTH_TEST_TIMEOUT("fast", "COUNT is answered from an index: at most 10 comparisons per COUNT (4000 keys, 4000 queries)", 10) {
    Database d;
    fill(d, 4000);
    probe::reset();
    for (int i = 0; i < 4000; ++i) {
        const Reply r = d.execute("COUNT value-number-" + std::to_string(i % 100));
        if (r.number != 40) {
            ::zth::fail("COUNT gave " + describe(r));
            return;
        }
    }
    ZTH_EXPECT_TRUE(probe::compares <= 4000ULL * 10);
}
ZTH_TEST_TIMEOUT("fast", "RANGE starts at lo (lower_bound) and stops after hi: comparisons stay near log n + the size of the answer", 10) {
    Database d;
    fill(d, 4000);
    probe::reset();
    for (int i = 0; i < 1000; ++i) {
        const Reply r = d.execute("RANGE " + keyname(i * 3) + " " + keyname(i * 3 + 4));
        if (r.items.size() != 5) {
            ::zth::fail("RANGE gave " + describe(r));
            return;
        }
    }
    ZTH_EXPECT_TRUE(probe::compares <= 1000ULL * 30);
}
ZTH_TEST_TIMEOUT("fast", "BEGIN does not copy the data: 20 nested BEGINs on 4000 keys cost at most 20 Text copies and 200 comparisons", 10) {
    Database d;
    fill(d, 4000);
    probe::reset();
    for (int i = 0; i < 20; ++i) ZTH_EXPECT_TRUE(describe(d.execute("BEGIN")) == "OK");
    ZTH_EXPECT_TRUE(probe::copies <= 20);
    ZTH_EXPECT_TRUE(probe::compares <= 200);
}
ZTH_TEST_TIMEOUT("fast", "a transaction costs in proportion to its changes: 1000 changes, ROLLBACK, on 4000 keys", 10) {
    Database d;
    fill(d, 4000);
    probe::reset();
    d.execute("BEGIN");
    for (int i = 0; i < 1000; ++i) d.execute("SET " + keyname(i * 4) + " changed");
    for (int i = 0; i < 500; ++i) d.execute("DEL " + keyname(i * 4 + 1));
    ZTH_EXPECT_TRUE(describe(d.execute("ROLLBACK")) == "OK");
    ZTH_EXPECT_TRUE(probe::copies <= 4000);
    ZTH_EXPECT_TRUE(probe::compares <= 68000);
    ZTH_EXPECT_EQ(d.store().size(), std::size_t{4000});
    ZTH_EXPECT_EQ(d.store().count(T("changed")), std::size_t{0});
}
ZTH_TEST_TIMEOUT("fast", "COMMIT of a big transaction is cheap (no copying of the data, no per-change work)", 10) {
    Database d;
    fill(d, 4000);
    d.execute("BEGIN");
    for (int i = 0; i < 2000; ++i) d.execute("SET " + keyname(i) + " changed");
    probe::reset();
    ZTH_EXPECT_TRUE(describe(d.execute("COMMIT")) == "OK");
    ZTH_EXPECT_TRUE(probe::copies == 0);
    ZTH_EXPECT_TRUE(probe::compares <= 10);
}
ZTH_TEST_TIMEOUT("fast", "parsing a command allocates little: at most 6 allocations for SET with a long key and a long value", 10) {
    const std::string line = "SET a-rather-long-key-name \"a value that is longer than the small string buffer\"";
    ParsedCommand p;
    zth::mem::Scope scope;
    p = parse_command(line);
    ZTH_EXPECT_TRUE(p.ok());
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 6);
}
ZTH_TEST_TIMEOUT("fast", "INCR moves its key into the store: at most 500 Text copies for 1000 INCRs on different keys", 10) {
    Database d;
    probe::reset();
    for (int i = 0; i < 1000; ++i) d.execute("INCR " + keyname(i));
    ZTH_EXPECT_EQ(d.store().size(), std::size_t{1000});
    ZTH_EXPECT_TRUE(probe::copies <= 500);
}
ZTH_TEST_TIMEOUT("fast", "scaling: twice as many keys cost about twice the comparisons, not four times", 10) {
    unsigned long long a = 0, b = 0;
    {
        Database d;
        probe::reset();
        fill(d, 2000);
        for (int i = 0; i < 2000; ++i) d.execute("COUNT value-number-" + std::to_string(i % 100));
        for (int i = 0; i < 500; ++i) d.execute("RANGE " + keyname(i) + " " + keyname(i + 2));
        a = probe::compares;
    }
    {
        Database d;
        probe::reset();
        fill(d, 4000);
        for (int i = 0; i < 4000; ++i) d.execute("COUNT value-number-" + std::to_string(i % 100));
        for (int i = 0; i < 1000; ++i) d.execute("RANGE " + keyname(i) + " " + keyname(i + 2));
        b = probe::compares;
    }
    ZTH_EXPECT_TRUE(a > 0);
    ZTH_EXPECT_TRUE(b <= a * 5 / 2);
}
ZTH_TEST_TIMEOUT("fast", "200000 commands in time (nothing quadratic)", 10) {
    Database d;
    for (int i = 0; i < 100000; ++i) d.execute("SET k" + std::to_string(i % 5000) + " v" + std::to_string(i % 70));
    long long total = 0;
    for (int i = 0; i < 100000; ++i) total += d.execute("COUNT v" + std::to_string(i % 70)).number;
    ZTH_EXPECT_TRUE(total > 0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw new/delete/malloc/free in any of the three files") {
    ZTH_EXPECT_NO_RAW_MEMORY("query.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("store.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("database.hpp");
}
ZTH_TEST("clean", "no throw/try/catch/goto: problems are Reply and ParseError values") {
    for (const char* file : {"query.hpp", "store.hpp", "database.hpp"}) {
        const std::string code = ::zth::strip_comments_and_literals(::zth::read_file(file));
        for (const char* word : {"throw", "try", "catch", "goto"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`");
                return;
            }
        }
    }
}
ZTH_TEST("clean", "no throwing conversions or streams: stoi / stoll / atoi / sscanf / stringstream (use std::from_chars)") {
    for (const char* file : {"query.hpp", "store.hpp", "database.hpp"}) {
        const std::string code = ::zth::strip_comments_and_literals(::zth::read_file(file));
        for (const char* word : {"stoi", "stol", "stoll", "stoul", "stoull", "atoi", "atol", "atoll", "sscanf", "stringstream", "istringstream", "ostringstream"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`");
                return;
            }
        }
    }
}
ZTH_TEST("clean", "the commands are dispatched with std::visit or std::get_if on the variant, not with dynamic_cast, a table of function pointers or macros") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("database.hpp"));
    ZTH_EXPECT_TRUE(!::zth::find_word(code, "visit").empty() || !::zth::find_word(code, "get_if").empty() || !::zth::find_word(code, "holds_alternative").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "dynamic_cast").empty());
    ZTH_EXPECT_TRUE(code.find("#define") == std::string::npos);
}
