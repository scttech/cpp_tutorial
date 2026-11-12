#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "cleaning.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;
using Rs = std::vector<Record>;
using Names = std::vector<S>;

Record rec(const char* name, int age, double score) { return Record{name, age, score}; }

Names names_of(const Rs& rs) {
    Names n;
    for (const Record& r : rs) n.push_back(r.name);
    return n;
}

// counts comparisons
struct Counted {
    int v = 0;
    static inline long compares = 0;
    Counted() = default;
    Counted(int x) : v(x) {}  // NOLINT
    bool operator<(const Counted& o) const {
        ++compares;
        return v < o.v;
    }
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "remove_invalid removes empty names and impossible ages, keeping the order of the rest") {
    Rs rs = {rec("a", 30, 1), rec("", 30, 2), rec("b", -1, 3), rec("c", 151, 4), rec("d", 0, 5), rec("e", 150, 6)};
    remove_invalid(rs);
    ZTH_EXPECT_TRUE((names_of(rs) == Names{"a", "d", "e"}));
}
ZTH_TEST("works", "normalize_names capitalizes: first letter upper, the rest lower") {
    Rs rs = {rec("aDA", 1, 1), rec("GRACE", 1, 1), rec("alan", 1, 1), rec("X", 1, 1)};
    normalize_names(rs);
    ZTH_EXPECT_TRUE((names_of(rs) == Names{"Ada", "Grace", "Alan", "X"}));
}
ZTH_TEST("works", "sort_records: highest score first") {
    Rs rs = {rec("a", 1, 50), rec("b", 1, 90), rec("c", 1, 70)};
    sort_records(rs);
    ZTH_EXPECT_TRUE((names_of(rs) == Names{"b", "c", "a"}));
}
ZTH_TEST("works", "sort_records: equal scores by name, equal names by age") {
    Rs rs = {rec("zed", 20, 80), rec("amy", 40, 80), rec("amy", 25, 80), rec("bob", 30, 90)};
    sort_records(rs);
    ZTH_EXPECT_TRUE((names_of(rs) == Names{"bob", "amy", "amy", "zed"}));
    ZTH_EXPECT_EQ(rs[1].age, 25);
    ZTH_EXPECT_EQ(rs[2].age, 40);
}
ZTH_TEST("works", "partition_adults puts the adults first and returns how many there are") {
    Rs rs = {rec("a", 10, 1), rec("b", 20, 1), rec("c", 17, 1), rec("d", 18, 1), rec("e", 50, 1)};
    const std::size_t n = partition_adults(rs);
    ZTH_EXPECT_EQ(n, std::size_t{3});
    ZTH_EXPECT_TRUE((names_of(rs) == Names{"b", "d", "e", "a", "c"}));  // each group keeps its original order
}
ZTH_TEST("works", "average_score") {
    ZTH_EXPECT_NEAR(average_score({rec("a", 1, 80), rec("b", 1, 90)}), 85.0, 1e-9);
    ZTH_EXPECT_NEAR(average_score({rec("a", 1, 7.5)}), 7.5, 1e-9);
    ZTH_EXPECT_NEAR(average_score({rec("a", 1, 1), rec("b", 1, 2), rec("c", 1, 2)}), 5.0 / 3.0, 1e-9);
}
ZTH_TEST("works", "unique_names_sorted: sorted, without repeats") {
    const Rs rs = {rec("bob", 1, 1), rec("amy", 1, 1), rec("bob", 2, 2), rec("cat", 1, 1), rec("amy", 3, 3)};
    ZTH_EXPECT_TRUE((unique_names_sorted(rs) == Names{"amy", "bob", "cat"}));
}
ZTH_TEST("works", "find_by_name returns the first record with that name, or nothing") {
    const Rs rs = {rec("amy", 20, 1), rec("bob", 30, 2), rec("bob", 40, 3)};
    const auto bob = find_by_name(rs, "bob");
    ZTH_EXPECT_TRUE(bob.has_value());
    if (bob) ZTH_EXPECT_EQ(bob->age, 30);
    ZTH_EXPECT_FALSE(find_by_name(rs, "zoe").has_value());
}
ZTH_TEST("works", "count_between counts the values with lo <= x < hi") {
    const std::vector<int> v{1, 3, 3, 5, 8, 13, 21};
    ZTH_EXPECT_EQ(count_between(v, 3, 13), std::size_t{4});  // 3, 3, 5, 8
    ZTH_EXPECT_EQ(count_between(v, 0, 100), std::size_t{7});
    ZTH_EXPECT_EQ(count_between(v, 4, 5), std::size_t{0});
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "remove_invalid on an empty vector, on all-valid and on all-invalid records") {
    Rs e;
    remove_invalid(e);
    ZTH_EXPECT_TRUE(e.empty());
    Rs ok = {rec("a", 1, 1), rec("b", 2, 2)};
    remove_invalid(ok);
    ZTH_EXPECT_EQ(ok.size(), std::size_t{2});
    Rs bad = {rec("", 1, 1), rec("x", -5, 2)};
    remove_invalid(bad);
    ZTH_EXPECT_TRUE(bad.empty());
}
ZTH_TEST("robust", "remove_invalid really SHRINKS the vector (size, not just the first elements)") {
    Rs rs = {rec("", 1, 1), rec("a", 1, 1), rec("", 1, 1), rec("b", 1, 1), rec("", 1, 1)};
    remove_invalid(rs);
    ZTH_EXPECT_EQ(rs.size(), std::size_t{2});
    ZTH_EXPECT_TRUE((names_of(rs) == Names{"a", "b"}));
}
ZTH_TEST("robust", "normalize_names: empty names, names with digits and punctuation, already normalized names") {
    Rs rs = {rec("", 1, 1), rec("o'NEIL", 1, 1), rec("r2D2", 1, 1), rec("Ada", 1, 1), rec("-x", 1, 1)};
    normalize_names(rs);
    ZTH_EXPECT_TRUE((names_of(rs) == Names{"", "O'neil", "R2d2", "Ada", "-x"}));
}
ZTH_TEST("robust", "normalize_names with bytes above 127 does not crash") {
    Rs rs = {rec("\xC3\x89mile", 1, 1), rec("\xFF" "abc", 1, 1)};
    normalize_names(rs);
    ZTH_EXPECT_EQ(rs.size(), std::size_t{2});
}
ZTH_TEST("robust", "normalize_names keeps the other fields") {
    Rs rs = {rec("aDA", 36, 91.5)};
    normalize_names(rs);
    ZTH_EXPECT_EQ(rs[0].age, 36);
    ZTH_EXPECT_NEAR(rs[0].score, 91.5, 1e-12);
}
ZTH_TEST("robust", "sort_records of an empty vector and of one record") {
    Rs e;
    sort_records(e);
    ZTH_EXPECT_TRUE(e.empty());
    Rs one = {rec("a", 1, 1)};
    sort_records(one);
    ZTH_EXPECT_EQ(one.size(), std::size_t{1});
}
ZTH_TEST("robust", "sort_records with MANY equal records (a comparator that is not a strict weak ordering fails here)") {
    Rs rs;
    for (int i = 0; i < 300; ++i) rs.push_back(rec("same", 30, 50.0));
    for (int i = 0; i < 40; ++i) rs.push_back(rec("other", 30, 50.0));
    sort_records(rs);
    ZTH_EXPECT_EQ(rs.size(), std::size_t{340});
    ZTH_EXPECT_EQ(rs.front().name, S("other"));
    ZTH_EXPECT_EQ(rs.back().name, S("same"));
}
ZTH_TEST("robust", "sort_records orders a long random list correctly") {
    zth::Lcg rng(31);
    Rs rs;
    for (int i = 0; i < 2000; ++i) rs.push_back(rec(rng.below(2) ? "alpha" : "beta", rng.below(60), rng.below(5) * 10.0));
    sort_records(rs);
    for (std::size_t i = 1; i < rs.size(); ++i) {
        const Record& a = rs[i - 1];
        const Record& b = rs[i];
        const bool ok = a.score > b.score || (a.score == b.score && (a.name < b.name || (a.name == b.name && a.age <= b.age)));
        if (!ok) {
            ::zth::fail("records " + std::to_string(i - 1) + " and " + std::to_string(i) + " are out of order");
            return;
        }
    }
}
ZTH_TEST("robust", "partition_adults: nobody is an adult, everybody is, and the boundary at 18 (off-by-one)") {
    Rs minors = {rec("a", 5, 1), rec("b", 17, 1)};
    ZTH_EXPECT_EQ(partition_adults(minors), std::size_t{0});
    Rs adults = {rec("a", 18, 1), rec("b", 90, 1)};
    ZTH_EXPECT_EQ(partition_adults(adults), std::size_t{2});
    ZTH_EXPECT_TRUE((names_of(adults) == Names{"a", "b"}));
    Rs e;
    ZTH_EXPECT_EQ(partition_adults(e), std::size_t{0});
}
ZTH_TEST("robust", "partition_adults is STABLE for a longer, alternating list") {
    Rs rs;
    for (int i = 0; i < 40; ++i) rs.push_back(rec((std::to_string(i)).c_str(), i % 2 == 0 ? 30 : 10, 1));
    const std::size_t n = partition_adults(rs);
    ZTH_EXPECT_EQ(n, std::size_t{20});
    for (int i = 0; i < 20; ++i) ZTH_EXPECT_EQ(rs[static_cast<std::size_t>(i)].name, std::to_string(2 * i));
    for (int i = 0; i < 20; ++i) ZTH_EXPECT_EQ(rs[static_cast<std::size_t>(20 + i)].name, std::to_string(2 * i + 1));
}
ZTH_TEST("robust", "average_score of no records is 0, not NaN") {
    ZTH_EXPECT_NEAR(average_score({}), 0.0, 1e-12);
}
ZTH_TEST("robust", "average_score keeps the fractions (a double sum, not an int one)") {
    ZTH_EXPECT_NEAR(average_score({rec("a", 1, 0.25), rec("b", 1, 0.25), rec("c", 1, 0.25), rec("d", 1, 0.25)}), 0.25, 1e-12);
    ZTH_EXPECT_NEAR(average_score({rec("a", 1, 0.5), rec("b", 1, 0.5)}), 0.5, 1e-12);
}
ZTH_TEST("robust", "unique_names_sorted: empty input, case sensitivity, and repeats that were not adjacent") {
    ZTH_EXPECT_TRUE(unique_names_sorted({}).empty());
    const Rs rs = {rec("b", 1, 1), rec("a", 1, 1), rec("b", 1, 1), rec("a", 1, 1), rec("B", 1, 1)};
    ZTH_EXPECT_TRUE((unique_names_sorted(rs) == Names{"B", "a", "b"}));
}
ZTH_TEST("robust", "find_by_name is case sensitive and finds the first of several, in an empty list nothing") {
    ZTH_EXPECT_FALSE(find_by_name({}, "a").has_value());
    const Rs rs = {rec("Amy", 20, 1), rec("amy", 30, 2)};
    const auto r = find_by_name(rs, "amy");
    ZTH_EXPECT_TRUE(r.has_value());
    if (r) ZTH_EXPECT_EQ(r->age, 30);
}
ZTH_TEST("robust", "count_between: hi below lo, equal bounds, and bounds outside the data") {
    const std::vector<int> v{1, 3, 3, 5, 8};
    ZTH_EXPECT_EQ(count_between(v, 8, 3), std::size_t{0});
    ZTH_EXPECT_EQ(count_between(v, 3, 3), std::size_t{0});
    ZTH_EXPECT_EQ(count_between(v, -10, 2), std::size_t{1});
    ZTH_EXPECT_EQ(count_between(v, 5, 1000), std::size_t{2});
    ZTH_EXPECT_EQ(count_between(std::vector<int>{}, 0, 5), std::size_t{0});
}
ZTH_TEST("robust", "count_between works for strings (only operator< is needed)") {
    const std::vector<S> v{"apple", "banana", "cherry", "date"};
    ZTH_EXPECT_EQ(count_between(v, S("b"), S("d")), std::size_t{2});
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "remove_invalid handles 200000 records (erase-remove, not an erase per bad record)") {
    Rs rs;
    rs.reserve(200000);
    for (int i = 0; i < 200000; ++i) rs.push_back(rec(i % 2 == 0 ? "ok" : "", 30, 1.0));
    remove_invalid(rs);
    ZTH_EXPECT_EQ(rs.size(), std::size_t{100000});
}
ZTH_TEST("fast", "count_between on 100000 values makes about 2 * log2(n) comparisons, not n") {
    std::vector<Counted> v;
    v.reserve(100000);
    for (int i = 0; i < 100000; ++i) v.push_back(Counted(i * 2));
    Counted::compares = 0;
    const std::size_t n = count_between(v, Counted(1000), Counted(5000));
    const long compares = Counted::compares;
    ZTH_EXPECT_EQ(n, std::size_t{2000});
    if (compares > 100) ::zth::fail("count_between made " + std::to_string(compares) + " comparisons: two binary searches take about 34");
}
ZTH_TEST("fast", "sorting and partitioning 100000 records finishes quickly") {
    zth::Lcg rng(7);
    Rs rs;
    rs.reserve(100000);
    for (int i = 0; i < 100000; ++i) rs.push_back(rec("n", rng.below(90), rng.below(1000)));
    sort_records(rs);
    const std::size_t adults = partition_adults(rs);
    ZTH_EXPECT_TRUE(adults > 50000);
    ZTH_EXPECT_TRUE(rs.front().score >= rs[1].score || rs.front().score == rs[1].score);
}

// ================================================ Clean =======================================================
// Source checks of cleaning.hpp (comments and string literals are blanked first).
namespace {

std::string cleaning_code() { return ::zth::strip_comments_and_literals(::zth::read_file("cleaning.hpp")); }

// Parameters of type Record taken BY VALUE, as in `(Record r)` or `, Record b)`: a silent copy of two strings per call.
std::size_t record_by_value_params(const std::string& code) {
    auto is_word = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
    std::size_t n = 0;
    for (std::size_t p : ::zth::find_word(code, "Record")) {
        std::size_t b = p;
        while (b > 0 && std::isspace(static_cast<unsigned char>(code[b - 1]))) --b;
        const bool after_open = b > 0 && (code[b - 1] == '(' || code[b - 1] == ',');
        std::size_t a = p + 6;
        while (a < code.size() && std::isspace(static_cast<unsigned char>(code[a]))) ++a;
        const bool named = a < code.size() && is_word(code[a]) && a > p + 6;
        if (after_open && named) ++n;
    }
    return n;
}

}  // namespace

ZTH_TEST("clean", "cleaning.hpp has no hand-written loop: no `for`, `while` or `do` (every step is a named algorithm)") {
    const std::string code = cleaning_code();
    for (const char* kw : {"for", "while", "do"}) {
        if (!::zth::find_word(code, kw).empty()) {
            ::zth::fail(std::string("found a `") + kw + "` loop. Use a named algorithm instead (std::for_each, std::transform, std::accumulate, std::find_if, ...).");
            return;
        }
    }
}
ZTH_TEST("clean", "predicates and comparators take a Record by const reference, not by value (no silent copies)") {
    ZTH_EXPECT_EQ(record_by_value_params(cleaning_code()), std::size_t{0});
}
