#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <string>
#include <vector>

#include "diff.hpp"
#include "zth_test.hpp"

using namespace difftool;

namespace {

Lines L(const std::string& text) { return split_lines(text); }

// build lines from a string of letters: "ABC" -> {"A\n","B\n","C\n"}
Lines letters(const std::string& s) {
    Lines out;
    for (char c : s) out.push_back(std::string(1, c) + "\n");
    return out;
}

// apply a script: the lines of B reconstructed from A and the edits (Delete skipped, Equal from A, Insert from B)
bool script_valid(const Lines& a, const Lines& b, const std::vector<Edit>& e) {
    std::size_t ia = 0, ib = 0;
    for (const Edit& x : e) {
        switch (x.op) {
            case Op::Equal:
                if (x.a != ia || x.b != ib || ia >= a.size() || ib >= b.size() || a[ia] != b[ib]) return false;
                ++ia;
                ++ib;
                break;
            case Op::Delete:
                if (x.a != ia || x.b != ib || ia >= a.size()) return false;
                ++ia;
                break;
            case Op::Insert:
                if (x.a != ia || x.b != ib || ib >= b.size()) return false;
                ++ib;
                break;
        }
    }
    return ia == a.size() && ib == b.size();
}
std::size_t changes(const std::vector<Edit>& e) {
    std::size_t n = 0;
    for (const Edit& x : e) n += x.op != Op::Equal;
    return n;
}
// no Insert directly before a Delete inside a run of changes
bool deletes_first(const std::vector<Edit>& e) {
    for (std::size_t i = 0; i + 1 < e.size(); ++i)
        if (e[i].op == Op::Insert && e[i + 1].op == Op::Delete) return false;
    return true;
}
// the textbook O(N M) dynamic program: D = N + M - 2 LCS
std::size_t reference_distance(const Lines& a, const Lines& b) {
    std::vector<std::vector<unsigned>> t(a.size() + 1, std::vector<unsigned>(b.size() + 1, 0));
    for (std::size_t i = 1; i <= a.size(); ++i)
        for (std::size_t j = 1; j <= b.size(); ++j) t[i][j] = a[i - 1] == b[j - 1] ? t[i - 1][j - 1] + 1 : std::max(t[i - 1][j], t[i][j - 1]);
    return a.size() + b.size() - 2 * t[a.size()][b.size()];
}
std::string ops(const std::vector<Edit>& e) {
    std::string s;
    for (const Edit& x : e) s += x.op == Op::Equal ? '=' : x.op == Op::Delete ? '-' : '+';
    return s;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "split_lines keeps the terminators; a last line without one is its own line that differs from the terminated one") {
    ZTH_EXPECT_TRUE((L("a\nb\nc\n") == Lines{"a\n", "b\n", "c\n"}));
    ZTH_EXPECT_TRUE((L("a\nb") == Lines{"a\n", "b"}));
    ZTH_EXPECT_TRUE((L("a\r\nb\r\n") == Lines{"a\r\n", "b\r\n"}));
    ZTH_EXPECT_TRUE((L("\n\n") == Lines{"\n", "\n"}));
    ZTH_EXPECT_TRUE(L("").empty());
    ZTH_EXPECT_TRUE((L("x") == Lines{"x"}));
    ZTH_EXPECT_TRUE((L("a\n\nb") == Lines{"a\n", "\n", "b"}));
    for (const char* t : {"", "a", "a\n", "a\nb", "\n", "a\r\nb\n\n\nc", "no newline at all"}) ZTH_EXPECT_EQ(join_lines(L(t)), std::string(t));
}
ZTH_TEST("works", "identical inputs give only Equal edits; empty inputs give nothing or only Inserts / Deletes") {
    const Lines a = letters("ABC");
    const auto same = diff(a, a);
    ZTH_EXPECT_EQ(ops(same), std::string("==="));
    ZTH_EXPECT_TRUE(script_valid(a, a, same));
    ZTH_EXPECT_TRUE(diff({}, {}).empty());
    ZTH_EXPECT_EQ(ops(diff({}, a)), std::string("+++"));
    ZTH_EXPECT_EQ(ops(diff(a, {})), std::string("---"));
    ZTH_EXPECT_TRUE(script_valid({}, a, diff({}, a)) && script_valid(a, {}, diff(a, {})));
    ZTH_EXPECT_EQ(distance(a, a), std::size_t{0});
    ZTH_EXPECT_EQ(distance({}, a), std::size_t{3});
    ZTH_EXPECT_EQ(distance(a, {}), std::size_t{3});
    ZTH_EXPECT_EQ(distance({}, {}), std::size_t{0});
}
ZTH_TEST("works", "the edit fields: indices into A and B for each kind of edit") {
    const Lines a = letters("ABCD"), b = letters("AXCDY");
    const auto e = diff(a, b);
    ZTH_EXPECT_TRUE(script_valid(a, b, e));
    ZTH_EXPECT_EQ(ops(e), std::string("=-+==+"));
    ZTH_EXPECT_TRUE(e[0].op == Op::Equal && e[0].a == 0 && e[0].b == 0);
    ZTH_EXPECT_TRUE(e[1].op == Op::Delete && e[1].a == 1 && e[1].b == 1);   // "B" is removed while one B line (A) has been produced
    ZTH_EXPECT_TRUE(e[2].op == Op::Insert && e[2].a == 2 && e[2].b == 1);   // "X" is line 1 of B; two A lines (A, B) have been consumed
    ZTH_EXPECT_TRUE(e[3].op == Op::Equal && e[3].a == 2 && e[3].b == 2);
    ZTH_EXPECT_TRUE(e[4].op == Op::Equal && e[4].a == 3 && e[4].b == 3);
    ZTH_EXPECT_TRUE(e[5].op == Op::Insert && e[5].a == 4 && e[5].b == 4);
}
ZTH_TEST("works", "changes at the start, in the middle and at the end; each is a shortest script") {
    struct Case { const char* a; const char* b; std::size_t d; };
    const Case cases[] = {{"ABC", "XABC", 1}, {"ABC", "ABCX", 1}, {"ABC", "ABXC", 1}, {"ABC", "BC", 1}, {"ABC", "AB", 1}, {"ABC", "AC", 1}, {"ABC", "XYZ", 6}, {"A", "B", 2}, {"AB", "BA", 2}, {"ABCDEF", "ABXDEF", 2}, {"AAA", "AA", 1}, {"AA", "AAA", 1}};
    for (const Case& c : cases) {
        const Lines a = letters(c.a), b = letters(c.b);
        const auto e = diff(a, b);
        ZTH_EXPECT_TRUE(script_valid(a, b, e));
        ZTH_EXPECT_EQ(changes(e), c.d);
        ZTH_EXPECT_EQ(distance(a, b), c.d);
        ZTH_EXPECT_TRUE(deletes_first(e));
    }
}
ZTH_TEST("works", "the example from Myers' paper: ABCABBA to CBABAC needs 5 edits (LCS of length 4)") {
    const Lines a = letters("ABCABBA"), b = letters("CBABAC");
    const auto e = diff(a, b);
    ZTH_EXPECT_TRUE(script_valid(a, b, e));
    ZTH_EXPECT_EQ(changes(e), std::size_t{5});
    ZTH_EXPECT_EQ(distance(a, b), std::size_t{5});
    ZTH_EXPECT_EQ(distance(b, a), std::size_t{5});
    ZTH_EXPECT_EQ(reference_distance(a, b), std::size_t{5});
}
ZTH_TEST("works", "inside a run of changes the deletions come before the insertions") {
    const Lines a = letters("AXYB"), b = letters("APQRB");
    const auto e = diff(a, b);
    ZTH_EXPECT_EQ(ops(e), std::string("=--+++="));
    ZTH_EXPECT_TRUE(script_valid(a, b, e));
    ZTH_EXPECT_TRUE(e[1].a == 1 && e[1].b == 1 && e[2].a == 2 && e[2].b == 1);   // both deletions happen "at" b = 1
    ZTH_EXPECT_TRUE(e[3].a == 3 && e[3].b == 1 && e[4].a == 3 && e[4].b == 2 && e[5].a == 3 && e[5].b == 3);
    const Lines c = letters("XY"), d = letters("PQ");
    ZTH_EXPECT_EQ(ops(diff(c, d)), std::string("--++"));
}
ZTH_TEST("works", "a missing final newline is a change; lines are compared byte for byte, CR included") {
    const Lines a = L("one\ntwo\nthree\n"), b = L("one\ntwo\nthree");
    const auto e = diff(a, b);
    ZTH_EXPECT_EQ(ops(e), std::string("==-+"));
    ZTH_EXPECT_TRUE(script_valid(a, b, e));
    ZTH_EXPECT_EQ(changes(diff(L("a\r\nb\r\n"), L("a\nb\n"))), std::size_t{4});
    ZTH_EXPECT_EQ(changes(diff(L("a \n"), L("a\n"))), std::size_t{2});
    ZTH_EXPECT_EQ(changes(diff(L("A\n"), L("a\n"))), std::size_t{2});
}
ZTH_TEST("works", "unified(): a single hunk with context, and the header") {
    const Lines a = L("1\n2\n3\n4\n5\n6\n7\n8\n9\n"), b = L("1\n2\n3\n4\nFIVE\n6\n7\n8\n9\n");
    const auto e = diff(a, b);
    const std::string expected = "--- old.txt\n+++ new.txt\n@@ -2,7 +2,7 @@\n 2\n 3\n 4\n-5\n+FIVE\n 6\n 7\n 8\n";
    ZTH_EXPECT_EQ(unified(a, b, e, "old.txt", "new.txt"), expected);
    ZTH_EXPECT_EQ(unified(a, b, e, "old.txt", "new.txt", 1), std::string("--- old.txt\n+++ new.txt\n@@ -4,3 +4,3 @@\n 4\n-5\n+FIVE\n 6\n"));
    ZTH_EXPECT_EQ(unified(a, b, e, "o", "n", 0), std::string("--- o\n+++ n\n@@ -5 +5 @@\n-5\n+FIVE\n"));
    ZTH_EXPECT_EQ(unified(a, a, diff(a, a), "o", "n"), std::string(""));
    ZTH_EXPECT_EQ(unified({}, {}, {}, "o", "n"), std::string(""));
}
ZTH_TEST("works", "unified(): the context is cut short by the ends of the file; counts of 1 lose their comma, empty ranges point at the line before") {
    const Lines a = L("a\nb\nc\n");
    ZTH_EXPECT_EQ(unified(a, L("X\nb\nc\n"), diff(a, L("X\nb\nc\n")), "a", "b"), std::string("--- a\n+++ b\n@@ -1,3 +1,3 @@\n-a\n+X\n b\n c\n"));
    ZTH_EXPECT_EQ(unified(a, L("a\nb\nX\n"), diff(a, L("a\nb\nX\n")), "a", "b"), std::string("--- a\n+++ b\n@@ -1,3 +1,3 @@\n a\n b\n-c\n+X\n"));
    ZTH_EXPECT_EQ(unified({}, a, diff({}, a), "a", "b"), std::string("--- a\n+++ b\n@@ -0,0 +1,3 @@\n+a\n+b\n+c\n"));
    ZTH_EXPECT_EQ(unified(a, {}, diff(a, {}), "a", "b"), std::string("--- a\n+++ b\n@@ -1,3 +0,0 @@\n-a\n-b\n-c\n"));
    ZTH_EXPECT_EQ(unified(L("x\n"), L("y\n"), diff(L("x\n"), L("y\n")), "a", "b"), std::string("--- a\n+++ b\n@@ -1 +1 @@\n-x\n+y\n"));
    const Lines five = L("1\n2\n3\n4\n5\n");
    ZTH_EXPECT_EQ(unified(five, L("1\n2\n3\nNEW\n4\n5\n"), diff(five, L("1\n2\n3\nNEW\n4\n5\n")), "a", "b", 1), std::string("--- a\n+++ b\n@@ -3,2 +3,3 @@\n 3\n+NEW\n 4\n"));
    ZTH_EXPECT_EQ(unified(five, L("0\n1\n2\n3\n4\n5\n"), diff(five, L("0\n1\n2\n3\n4\n5\n")), "a", "b", 1), std::string("--- a\n+++ b\n@@ -1 +1,2 @@\n+0\n 1\n"));
    ZTH_EXPECT_EQ(unified(five, L("1\n2\n3\n4\n5\n6\n"), diff(five, L("1\n2\n3\n4\n5\n6\n")), "a", "b", 1), std::string("--- a\n+++ b\n@@ -5 +5,2 @@\n 5\n+6\n"));
}
ZTH_TEST("works", "unified(): two changes are one hunk when at most 2 * context equal lines separate them, two hunks otherwise") {
    // 12 lines; changes at lines 2 and 7 leave 4 equal lines between them
    std::string t;
    for (int i = 1; i <= 12; ++i) t += std::to_string(i) + "\n";
    std::string u = t;
    u.replace(u.find("2\n"), 2, "two\n");
    u.replace(u.find("7\n"), 2, "seven\n");
    const Lines a = L(t), b = L(u);
    const auto e = diff(a, b);
    // context 2: the gap (4 lines) is exactly 2 * context -> one hunk
    ZTH_EXPECT_EQ(unified(a, b, e, "a", "b", 2), std::string("--- a\n+++ b\n@@ -1,9 +1,9 @@\n 1\n-2\n+two\n 3\n 4\n 5\n 6\n-7\n+seven\n 8\n 9\n"));
    // context 1: the gap is more than 2 -> two hunks
    ZTH_EXPECT_EQ(unified(a, b, e, "a", "b", 1), std::string("--- a\n+++ b\n@@ -1,3 +1,3 @@\n 1\n-2\n+two\n 3\n@@ -6,3 +6,3 @@\n 6\n-7\n+seven\n 8\n"));
    // context 3 merges them too (the gap is 4 <= 6); the trailing context is 3 lines
    ZTH_EXPECT_EQ(unified(a, b, e, "a", "b", 3), std::string("--- a\n+++ b\n@@ -1,10 +1,10 @@\n 1\n-2\n+two\n 3\n 4\n 5\n 6\n-7\n+seven\n 8\n 9\n 10\n"));
    // a gap of exactly 2 * context + 1 splits the hunk: changes at lines 2 and 8 leave 5 equal lines between them
    std::string v = t;
    v.replace(v.find("2\n"), 2, "two\n");
    v.replace(v.find("8\n"), 2, "eight\n");
    const std::string split = unified(L(t), L(v), diff(L(t), L(v)), "a", "b", 2);
    std::size_t hunks = 0;
    for (std::size_t at = split.find("@@ -"); at != std::string::npos; at = split.find("@@ -", at + 1)) ++hunks;
    ZTH_EXPECT_EQ(hunks, std::size_t{2});
    const std::string joined = unified(L(t), L(v), diff(L(t), L(v)), "a", "b", 3);   // 5 <= 2 * 3: one hunk
    std::size_t joined_hunks = 0;
    for (std::size_t at = joined.find("@@ -"); at != std::string::npos; at = joined.find("@@ -", at + 1)) ++joined_hunks;
    ZTH_EXPECT_EQ(joined_hunks, std::size_t{1});
}
ZTH_TEST("works", "unified(): lines without a final newline carry the marker, on the right side of the change") {
    const Lines a = L("x\ny"), b = L("x\ny\n");
    ZTH_EXPECT_EQ(unified(a, b, diff(a, b), "a", "b"), std::string("--- a\n+++ b\n@@ -1,2 +1,2 @@\n x\n-y\n\\ No newline at end of file\n+y\n"));
    ZTH_EXPECT_EQ(unified(b, a, diff(b, a), "a", "b"), std::string("--- a\n+++ b\n@@ -1,2 +1,2 @@\n x\n-y\n+y\n\\ No newline at end of file\n"));
    const Lines c = L("p\nq"), d = L("P\nq");
    ZTH_EXPECT_EQ(unified(c, d, diff(c, d), "a", "b"), std::string("--- a\n+++ b\n@@ -1,2 +1,2 @@\n-p\n+P\n q\n\\ No newline at end of file\n"));   // a context line, marked once
    const Lines e = L("only"), f = L("");
    ZTH_EXPECT_EQ(unified(e, f, diff(e, f), "a", "b"), std::string("--- a\n+++ b\n@@ -1 +0,0 @@\n-only\n\\ No newline at end of file\n"));
}
ZTH_TEST("works", "apply_patch applies a unified diff and returns the new lines") {
    const Lines a = L("a\nb\nc\nd\ne\n");
    const std::string patch = "--- x\n+++ y\n@@ -2,3 +2,3 @@\n b\n-c\n+C\n d\n";
    const auto r = apply_patch(a, patch);
    ZTH_EXPECT_TRUE(r.has_value());
    ZTH_EXPECT_EQ(join_lines(*r), std::string("a\nb\nC\nd\ne\n"));
    ZTH_EXPECT_TRUE(apply_patch(a, "").has_value() && *apply_patch(a, "") == a);
    ZTH_EXPECT_TRUE(apply_patch(a, "--- x\n+++ y\n").has_value());
    const auto two = apply_patch(a, "diff --git a/f b/f\nindex 1..2\n--- a/f\n+++ b/f\n@@ -1 +1,2 @@\n+first\n a\n@@ -5 +6 @@\n-e\n+E\n");
    ZTH_EXPECT_TRUE(two.has_value());
    ZTH_EXPECT_EQ(join_lines(*two), std::string("first\na\nb\nc\nd\nE\n"));
    const auto fn = apply_patch(a, "@@ -2 +2 @@ int main() {\n-b\n+B\n");   // text after the closing @@ is allowed
    ZTH_EXPECT_TRUE(fn.has_value() && join_lines(*fn) == "a\nB\nc\nd\ne\n");
    const auto into_empty = apply_patch({}, "--- a\n+++ b\n@@ -0,0 +1,2 @@\n+x\n+y\n");
    ZTH_EXPECT_TRUE(into_empty.has_value() && join_lines(*into_empty) == "x\ny\n");
    const auto delete_all = apply_patch(L("x\ny\n"), "--- a\n+++ b\n@@ -1,2 +0,0 @@\n-x\n-y\n");
    ZTH_EXPECT_TRUE(delete_all.has_value() && delete_all->empty());
    const auto blank_context = apply_patch(L("a\n\nb\n"), "@@ -1,3 +1,3 @@\n a\n\n-b\n+B\n");   // a completely empty line stands for an empty context line
    ZTH_EXPECT_TRUE(blank_context.has_value() && join_lines(*blank_context) == "a\n\nB\n");
}
ZTH_TEST("works", "apply_patch handles the no-newline marker on context, deleted and inserted lines") {
    const auto add_newline = apply_patch(L("x\ny"), "@@ -1,2 +1,2 @@\n x\n-y\n\\ No newline at end of file\n+y\n");
    ZTH_EXPECT_TRUE(add_newline.has_value() && join_lines(*add_newline) == "x\ny\n");
    const auto drop_newline = apply_patch(L("x\ny\n"), "@@ -1,2 +1,2 @@\n x\n-y\n+y\n\\ No newline at end of file\n");
    ZTH_EXPECT_TRUE(drop_newline.has_value() && join_lines(*drop_newline) == "x\ny");
    const auto context = apply_patch(L("p\nq"), "@@ -1,2 +1,2 @@\n-p\n+P\n q\n\\ No newline at end of file\n");
    ZTH_EXPECT_TRUE(context.has_value() && join_lines(*context) == "P\nq");
    ZTH_EXPECT_FALSE(apply_patch(L("x\ny\n"), "@@ -1,2 +1,2 @@\n x\n-y\n\\ No newline at end of file\n+y\n").has_value());   // A's y HAS a newline: the patch does not match
}
ZTH_TEST("works", "apply_patch refuses everything that does not fit: no fuzz, no guessing") {
    const Lines a = L("a\nb\nc\nd\ne\n");
    const char* bad[] = {
        "@@ -2,3 +2,3 @@\n b\n-X\n+C\n d\n",                    // context does not match
        "@@ -2,3 +2,3 @@\n b\n-c\n+C\n x\n",                    // trailing context does not match
        "@@ -3,3 +3,3 @@\n b\n-c\n+C\n d\n",                    // right text, wrong line numbers (no offset search)
        "@@ -2,3 +2,3 @@\n b\n-c\n+C\n",                       // body shorter than the header says
        "@@ -2,2 +2,3 @@\n b\n-c\n+C\n d\n",                    // header says 2 old lines, the body has 3
        "@@ -2,3 +2,2 @@\n b\n-c\n+C\n d\n",                    // header says 2 new lines, the body has 3
        "@@ -2,3 +3,3 @@\n b\n-c\n+C\n d\n",                    // the new-file start is wrong
        "@@ -9,1 +9,1 @@\n-z\n+y\n",                           // beyond the end of A
        "@@ -2,1 +2,1 @@\n?b\n",                              // unknown line prefix
        "@@ nonsense @@\n b\n",                                // malformed header
        "@@ -a,1 +2,1 @@\n b\n",
        "@@ -2,1 2,1 @@\n b\n",
        "@@ -2,1 +2,1\n b\n",
        "@@ -2,3 +2,3 @@\n b\n-c\n+C\n d\ntrailing text\n",     // text after the hunk
        "@@ -4,1 +4,1 @@\n-d\n+D\n@@ -2,1 +2,1 @@\n-b\n+B\n",   // hunks out of order
        "@@ -2,2 +2,2 @@\n b\n-c\n+C\n@@ -3,1 +3,1 @@\n-c\n+X\n",   // hunks overlap
        "@@ -0,1 +1,1 @@\n-a\n+b\n",                          // line 0 with a count
        "@@ -1,2 +1,1 @@\n-a\n-b\n",                           // fewer new lines than the header says
    };
    for (const char* p : bad) ZTH_EXPECT_FALSE(apply_patch(a, p).has_value());
}
ZTH_TEST("works", "headers with huge numbers do not overflow") {
    const Lines a = L("a\n");
    ZTH_EXPECT_FALSE(apply_patch(a, "@@ -99999999999999999999999999 +1 @@\n-a\n").has_value());
    ZTH_EXPECT_FALSE(apply_patch(a, "@@ -1,99999999999999999999999999 +1 @@\n-a\n").has_value());
    ZTH_EXPECT_FALSE(apply_patch(a, "@@ -18446744073709551615 +1 @@\n-a\n").has_value());
    ZTH_EXPECT_FALSE(apply_patch(a, "@@ -18446744073709551617 +0,0 @@\n-a\n").has_value());   // 2^64 + 1 would wrap around to line 1
    ZTH_EXPECT_FALSE(apply_patch(a, "@@ -1 +18446744073709551617 @@\n-a\n").has_value());
    ZTH_EXPECT_FALSE(apply_patch(a, "@@ -1,18446744073709551617 +1,0 @@\n-a\n").has_value());
    ZTH_EXPECT_FALSE(apply_patch(a, "@@ -1,18446744073709551615 +1,0 @@\n-a\n").has_value());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "1500 random line sets over small alphabets: the script is valid and as short as the dynamic-programming reference says") {
    zth::Lcg rng(2024);
    for (int round = 0; round < 1500; ++round) {
        const int alphabet = 1 + rng.below(5);
        Lines a, b;
        for (int i = 0, n = rng.below(14); i < n; ++i) a.push_back(std::string(1, static_cast<char>('A' + rng.below(alphabet))) + "\n");
        for (int i = 0, n = rng.below(14); i < n; ++i) b.push_back(std::string(1, static_cast<char>('A' + rng.below(alphabet))) + "\n");
        const auto e = diff(a, b);
        ZTH_EXPECT_TRUE(script_valid(a, b, e));
        ZTH_EXPECT_TRUE(deletes_first(e));
        const std::size_t ref = reference_distance(a, b);
        ZTH_EXPECT_EQ(changes(e), ref);
        ZTH_EXPECT_EQ(distance(a, b), ref);
        ZTH_EXPECT_EQ(distance(b, a), ref);
        ZTH_EXPECT_EQ(changes(diff(b, a)), ref);
    }
}
ZTH_TEST("robust", "similar texts: a base with random edits (the realistic case, many equal lines, small D)") {
    zth::Lcg rng(7);
    for (int round = 0; round < 300; ++round) {
        Lines a;
        for (int i = 0, n = 50 + rng.below(150); i < n; ++i) a.push_back("line " + std::to_string(rng.below(40)) + "\n");
        Lines b = a;
        for (int k = 0, edits = rng.below(8); k < edits; ++k) {
            const std::size_t pos = static_cast<std::size_t>(rng.below(static_cast<int>(b.size()) + 1));
            switch (rng.below(3)) {
                case 0: b.insert(b.begin() + static_cast<std::ptrdiff_t>(pos), "new " + std::to_string(rng.below(100)) + "\n"); break;
                case 1: if (pos < b.size()) b.erase(b.begin() + static_cast<std::ptrdiff_t>(pos)); break;
                default: if (pos < b.size()) b[pos] = "changed\n"; break;
            }
        }
        const auto e = diff(a, b);
        ZTH_EXPECT_TRUE(script_valid(a, b, e));
        ZTH_EXPECT_EQ(changes(e), reference_distance(a, b));
        ZTH_EXPECT_TRUE(changes(e) <= 16);
    }
}
ZTH_TEST("robust", "tricky shapes: all equal lines, a single repeated line, interleavings, overlapping prefix and suffix, reversal") {
    ZTH_EXPECT_EQ(changes(diff(letters("AAAA"), letters("AA"))), std::size_t{2});
    ZTH_EXPECT_EQ(changes(diff(letters("A"), letters("AAAAA"))), std::size_t{4});
    ZTH_EXPECT_EQ(changes(diff(letters("ABAB"), letters("ABABAB"))), std::size_t{2});     // prefix and suffix overlap if trimmed carelessly
    ZTH_EXPECT_EQ(changes(diff(letters("ABABAB"), letters("ABAB"))), std::size_t{2});
    ZTH_EXPECT_EQ(changes(diff(letters("AB"), letters("ABAB"))), std::size_t{2});
    ZTH_EXPECT_EQ(changes(diff(letters("ABC"), letters("CBA"))), std::size_t{4});
    ZTH_EXPECT_EQ(changes(diff(letters("ABCDEFGH"), letters("HGFEDCBA"))), std::size_t{14});
    ZTH_EXPECT_EQ(changes(diff(letters("AXBXCX"), letters("ABC"))), std::size_t{3});
    for (const char* s : {"AAAA", "ABAB", "A", "", "ABCDEFGH", "AXBXCX"})
        for (const char* t : {"AA", "ABABAB", "AAAAA", "", "HGFEDCBA", "ABC"}) {
            const Lines a = letters(s), b = letters(t);
            const auto e = diff(a, b);
            ZTH_EXPECT_TRUE(script_valid(a, b, e));
            ZTH_EXPECT_EQ(changes(e), reference_distance(a, b));
        }
}
ZTH_TEST("robust", "the round trip: apply_patch(A, unified(A, B)) == B for 2000 random pairs, every context size, with and without final newlines") {
    zth::Lcg rng(31);
    for (int round = 0; round < 2000; ++round) {
        Lines a, b;
        for (int i = 0, n = rng.below(16); i < n; ++i) a.push_back("l" + std::to_string(rng.below(6)) + "\n");
        for (int i = 0, n = rng.below(16); i < n; ++i) b.push_back("l" + std::to_string(rng.below(6)) + "\n");
        if (rng.below(3) == 0 && !a.empty()) a.back().pop_back();
        if (rng.below(3) == 0 && !b.empty()) b.back().pop_back();
        const std::size_t context = static_cast<std::size_t>(rng.below(5));
        const std::string patch = unified(a, b, diff(a, b), "a.txt", "b.txt", context);
        const auto r = apply_patch(a, patch);
        if (!r || *r != b) {
            zth::fail("round trip failed for context " + std::to_string(context) + ":\n" + patch);
            return;
        }
        if (a == b) ZTH_EXPECT_TRUE(patch.empty());
        // a patch for different input must not apply silently to unrelated text, unless that text really has the same lines
        if (!patch.empty()) {
            Lines other = a;
            if (!other.empty()) other[other.size() / 2] = "ZZZ\n";
            const auto wrong = apply_patch(other, patch);
            if (wrong) ZTH_EXPECT_TRUE(*wrong != b || other == a);
        }
    }
}
ZTH_TEST("robust", "patches written with real-world noise: CRLF lines, lines that look like hunk headers, tabs and trailing spaces") {
    const Lines a = L("@@ -1 +1 @@\r\n--- not a header\r\n\tindented  \r\n+++ also not\r\nend\r\n"), b = L("@@ -1 +1 @@\r\n--- changed\r\n\tindented  \r\n+++ also not\r\nend\r\nmore\r\n");
    const std::string patch = unified(a, b, diff(a, b), "a", "b");
    const auto r = apply_patch(a, patch);
    ZTH_EXPECT_TRUE(r.has_value() && *r == b);
}
ZTH_TEST("robust", "edit scripts handed to unified() need not come from diff(): any valid script is formatted faithfully") {
    const Lines a = letters("ABCD"), b = letters("ABCD");
    // delete everything and insert everything again (a valid but silly script)
    std::vector<Edit> silly;
    for (std::size_t i = 0; i < 4; ++i) silly.push_back({Op::Delete, i, 0});
    for (std::size_t i = 0; i < 4; ++i) silly.push_back({Op::Insert, 4, i});
    ZTH_EXPECT_EQ(unified(a, b, silly, "a", "b"), std::string("--- a\n+++ b\n@@ -1,4 +1,4 @@\n-A\n-B\n-C\n-D\n+A\n+B\n+C\n+D\n"));
    const auto r = apply_patch(a, unified(a, b, silly, "a", "b"));
    ZTH_EXPECT_TRUE(r.has_value() && *r == b);
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "100000 lines with 40 changes scattered through them: time follows the number of changes, not the product of the sizes", 20) {
    Lines a;
    for (int i = 0; i < 100000; ++i) a.push_back("this is line number " + std::to_string(i) + " of the file\n");
    Lines b = a;
    for (int k = 0; k < 20; ++k) b[static_cast<std::size_t>(k * 4999 + 17)] = "CHANGED " + std::to_string(k) + "\n";
    for (int k = 0; k < 10; ++k) b.insert(b.begin() + k * 9000 + 100, "inserted " + std::to_string(k) + "\n");
    for (int k = 0; k < 10; ++k) b.erase(b.begin() + k * 7000 + 3000);
    const auto e = diff(a, b);
    ZTH_EXPECT_TRUE(script_valid(a, b, e));
    ZTH_EXPECT_EQ(distance(a, b), changes(e));
    ZTH_EXPECT_TRUE(changes(e) <= 80);
    const std::string patch = unified(a, b, e, "a", "b");
    const auto r = apply_patch(a, patch);
    ZTH_EXPECT_TRUE(r.has_value() && *r == b);
}
ZTH_TEST_TIMEOUT("fast", "two files of 1500 completely different lines (D = 3000): the worst case for the algorithm still finishes", 20) {
    Lines a, b;
    for (int i = 0; i < 1500; ++i) {
        a.push_back("a" + std::to_string(i) + "\n");
        b.push_back("b" + std::to_string(i) + "\n");
    }
    const auto e = diff(a, b);
    ZTH_EXPECT_TRUE(script_valid(a, b, e));
    ZTH_EXPECT_EQ(changes(e), std::size_t{3000});
    ZTH_EXPECT_EQ(distance(a, b), std::size_t{3000});
}
ZTH_TEST("fast", "identical large inputs cost a comparison per line and no big allocations; distance() keeps no history") {
    Lines a;
    for (int i = 0; i < 200000; ++i) a.push_back("line " + std::to_string(i) + "\n");
    Lines b = a;
    b[100000] = "different\n";
    zth::mem::Scope s1;
    const std::size_t d = distance(a, b);
    ZTH_EXPECT_ALLOCS_AT_MOST(s1, 8);   // the working array of the algorithm and nothing proportional to D squared
    ZTH_EXPECT_EQ(d, std::size_t{2});
    zth::mem::Scope s2;
    const auto same = diff(a, a);
    ZTH_EXPECT_ALLOCS_AT_MOST(s2, 30);   // the output vector growing, not one allocation per line
    ZTH_EXPECT_EQ(same.size(), a.size());
}
ZTH_TEST_TIMEOUT("fast", "lines of a megabyte: comparisons are not repeated more than the algorithm needs", 20) {
    const std::string big(1 << 20, 'x');
    Lines a, b;
    for (int i = 0; i < 40; ++i) {
        a.push_back(big + std::to_string(i) + "\n");
        b.push_back(big + std::to_string(i % 37) + "\n");
    }
    const auto e = diff(a, b);
    ZTH_EXPECT_TRUE(script_valid(a, b, e));
    ZTH_EXPECT_EQ(changes(e), reference_distance(a, b));
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("diff.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("diff.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "Myers' algorithm, written out: no regex, no string streams, no standard-library shortcuts for the comparison") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("diff.hpp"));
    for (const char* word : {"regex", "stringstream", "istringstream", "getline", "set_difference", "mismatch", "lcs", "LCS", "boost", "lexicographical_compare", "inner_product"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
