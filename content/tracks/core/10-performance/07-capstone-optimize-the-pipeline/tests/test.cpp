#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "pipeline.hpp"
#include "types.hpp"
#include "zth_test.hpp"

namespace {

// The reference: slow, simple, obviously right (std::map, streams).
Report reference(const std::vector<std::string>& lines) {
    Report r;
    std::map<std::string, long long> totals;
    for (const std::string& line : lines) {
        std::istringstream in(line);
        std::vector<std::string> parts;
        std::string p;
        while (in >> p) parts.push_back(p);
        bool ok = parts.size() == 3;
        int level = -1;
        if (ok) {
            for (std::size_t i = 0; i < 3; ++i)
                if (parts[0] == kLevels[i]) level = static_cast<int>(i);
            ok = level >= 0 && !parts[2].empty() && parts[2].size() <= 9 && parts[2].find_first_not_of("0123456789") == std::string::npos;
        }
        if (!ok) {
            ++r.skipped;
            continue;
        }
        ++r.lines;
        ++r.by_level[static_cast<std::size_t>(level)];
        totals[parts[1]] += std::stoll(parts[2]);
    }
    r.distinct_users = totals.size();
    std::vector<std::pair<std::string, long long>> v(totals.begin(), totals.end());
    std::stable_sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.second > b.second; });  // the map is sorted by name: ties keep that order
    for (std::size_t i = 0; i < v.size() && i < 3; ++i) r.top.push_back(v[i]);
    return r;
}

void check(const std::vector<std::string>& lines) { ZTH_EXPECT_EQ(describe(analyze(lines)), describe(reference(lines))); }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a small log: counts per level, users, and the top three") {
    const std::vector<std::string> log = {"INFO alice 100", "WARN bob 250", "ERROR alice 300", "INFO carol 50", "INFO bob 25", "ERROR dave 10"};
    const Report r = analyze(log);
    ZTH_EXPECT_EQ(r.lines, std::size_t{6});
    ZTH_EXPECT_EQ(r.skipped, std::size_t{0});
    ZTH_EXPECT_EQ(r.by_level[0], std::size_t{3});
    ZTH_EXPECT_EQ(r.by_level[1], std::size_t{1});
    ZTH_EXPECT_EQ(r.by_level[2], std::size_t{2});
    ZTH_EXPECT_EQ(r.distinct_users, std::size_t{4});
    ZTH_EXPECT_EQ(r.top.size(), std::size_t{3});
    ZTH_EXPECT_EQ(r.top[0].first, std::string("alice"));
    ZTH_EXPECT_EQ(r.top[0].second, 400LL);
    ZTH_EXPECT_EQ(r.top[1].first, std::string("bob"));
    ZTH_EXPECT_EQ(r.top[1].second, 275LL);
    ZTH_EXPECT_EQ(r.top[2].first, std::string("carol"));
}
ZTH_TEST("works", "a generated log of 3000 lines matches the reference") {
    check(make_log(3000));
    check(make_log(777, 5));
}
ZTH_TEST("works", "totals add up across many lines of the same user") {
    std::vector<std::string> log;
    for (int i = 1; i <= 100; ++i) log.push_back("INFO zed " + std::to_string(i));
    const Report r = analyze(log);
    ZTH_EXPECT_EQ(r.top.size(), std::size_t{1});
    ZTH_EXPECT_EQ(r.top[0].second, 5050LL);
    ZTH_EXPECT_EQ(r.distinct_users, std::size_t{1});
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "an empty log, and a log of only malformed lines") {
    check({});
    const Report r = analyze({"", "   ", "nonsense", "INFO", "INFO x"});
    ZTH_EXPECT_EQ(r.lines, std::size_t{0});
    ZTH_EXPECT_EQ(r.skipped, std::size_t{5});
    ZTH_EXPECT_TRUE(r.top.empty());
    ZTH_EXPECT_EQ(r.distinct_users, std::size_t{0});
}
ZTH_TEST("robust", "any amount of spaces and tabs separates the fields, also at the start and the end of the line") {
    const Report r = analyze({"INFO\talice\t10", "  WARN   alice    20  ", "\tERROR\t bob \t5\t", "INFO alice 1\r"});
    ZTH_EXPECT_EQ(r.lines, std::size_t{4});
    ZTH_EXPECT_EQ(r.top[0].first, std::string("alice"));
    ZTH_EXPECT_EQ(r.top[0].second, 31LL);
}
ZTH_TEST("robust", "levels are case-sensitive and must match exactly (no prefixes)") {
    const Report r = analyze({"info a 1", "Info a 1", "INFOX a 1", "ERRORS a 1", "WARNING a 1", "INF a 1", "WARN a 1"});
    ZTH_EXPECT_EQ(r.lines, std::size_t{1});
    ZTH_EXPECT_EQ(r.skipped, std::size_t{6});
    ZTH_EXPECT_EQ(r.by_level[1], std::size_t{1});
}
ZTH_TEST("robust", "the number is 1 to 9 digits: no sign, no letters, no decimal point, no hex; leading zeros are fine") {
    const Report r = analyze({"INFO a 10", "INFO a +5", "INFO a -5", "INFO a 12x", "INFO a x12", "INFO a 1.5", "INFO a 0x10", "INFO a 1234567890", "INFO a 007", "INFO a 999999999", "INFO a ."});
    ZTH_EXPECT_EQ(r.lines, std::size_t{3});
    ZTH_EXPECT_EQ(r.skipped, std::size_t{8});
    ZTH_EXPECT_EQ(r.top[0].second, 10LL + 7 + 999999999);
}
ZTH_TEST("robust", "a line must have exactly three fields: two or four are skipped") {
    const Report r = analyze({"INFO a", "INFO a 1 2", "INFO a 1 extra", "INFO a 1"});
    ZTH_EXPECT_EQ(r.lines, std::size_t{1});
    ZTH_EXPECT_EQ(r.skipped, std::size_t{3});
}
ZTH_TEST("robust", "totals do not overflow an int, and a user name can be any non-space text") {
    std::vector<std::string> log;
    for (int i = 0; i < 5; ++i) log.push_back("ERROR user-with.odd_chars!? 999999999");
    const Report r = analyze(log);
    ZTH_EXPECT_EQ(r.top[0].second, 4999999995LL);
    ZTH_EXPECT_EQ(r.top[0].first, std::string("user-with.odd_chars!?"));
}
ZTH_TEST("robust", "ties: the same total is ordered by name, and at most three users are reported") {
    const Report r = analyze({"INFO delta 5", "INFO bravo 5", "INFO echo 5", "INFO alpha 5", "INFO charlie 5", "INFO zulu 9"});
    ZTH_EXPECT_EQ(r.distinct_users, std::size_t{6});
    ZTH_EXPECT_EQ(r.top.size(), std::size_t{3});
    ZTH_EXPECT_EQ(r.top[0].first, std::string("zulu"));
    ZTH_EXPECT_EQ(r.top[1].first, std::string("alpha"));
    ZTH_EXPECT_EQ(r.top[2].first, std::string("bravo"));
}
ZTH_TEST("robust", "fewer than three users give a shorter top list; users that differ only in case are different users") {
    const Report r = analyze({"INFO Bob 1", "INFO bob 2"});
    ZTH_EXPECT_EQ(r.distinct_users, std::size_t{2});
    ZTH_EXPECT_EQ(r.top.size(), std::size_t{2});
    ZTH_EXPECT_EQ(r.top[0].first, std::string("bob"));
    check({"WARN only 3"});
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "UserId comparisons: 5000 lines (about 500 users) need at most 150000 (a linear search per line needs over a million)") {
    const auto log = make_log(5000);
    probe::reset();
    (void)analyze(log);
    ZTH_EXPECT_TRUE(probe::compares <= 150000);
}
ZTH_TEST("fast", "UserId copies: a user name is moved into its container, not copied (at most 150 copies for 5000 lines)") {
    const auto log = make_log(5000);
    probe::reset();
    (void)analyze(log);
    ZTH_EXPECT_TRUE(probe::copies <= 150);
}
ZTH_TEST("fast", "heap allocations: at most 1200 for 5000 lines (parsing with streams and vectors needs 4 or more per line)") {
    const auto log = make_log(5000);
    zth::mem::Scope scope;
    const Report r = analyze(log);
    ZTH_EXPECT_TRUE(r.lines > 4000);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1200);
}
ZTH_TEST("fast", "scaling: twice the log costs about twice the comparisons, not four times") {
    const auto small = make_log(2000);
    const auto large = make_log(4000);
    probe::reset();
    (void)analyze(small);
    const auto a = probe::compares;
    probe::reset();
    (void)analyze(large);
    const auto b = probe::compares;
    ZTH_EXPECT_TRUE(a > 0);
    ZTH_EXPECT_TRUE(b <= a * 5 / 2);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "pipeline.hpp has no raw new/delete/malloc/free") {
    ZTH_EXPECT_NO_RAW_MEMORY("pipeline.hpp");
}
ZTH_TEST("clean", "pipeline.hpp parses without streams (no stringstream, no std::endl, no sscanf)") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("pipeline.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "istringstream").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "stringstream").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "sscanf").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "endl").empty());
}
ZTH_TEST("clean", "pipeline.hpp looks at the line in place with std::string_view and converts numbers with std::from_chars") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("pipeline.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "string_view").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "from_chars").empty());
}
