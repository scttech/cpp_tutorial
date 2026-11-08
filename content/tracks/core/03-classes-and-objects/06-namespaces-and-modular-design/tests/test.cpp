#include <algorithm>
#include <cctype>
#include <climits>
#include <concepts>
#include <cstddef>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "zth_test.hpp"

// These two checks are written BEFORE geometry.hpp is included, on purpose. A function template only sees the
// declarations that came before it, plus whatever argument-dependent lookup finds in the namespaces of the ARGUMENTS.
// So an operator that was written outside namespace geo (and after this point) is invisible to them: exactly what happens
// inside std::ostream_iterator, std::find, or any generic code of your own.
namespace probe {
template <class T> constexpr bool printable_via_adl = requires(std::ostream& os, const T& v) { { os << v } -> std::same_as<std::ostream&>; };
template <class T> constexpr bool comparable_via_adl = requires(const T& a, const T& b) { { a == b } -> std::convertible_to<bool>; };
}  // namespace probe

#include "geometry.hpp"

namespace {

template <class Ex, class F>
bool throws(F&& f) {
    try {
        f();
    } catch (const Ex&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

// Text for failure messages and comparisons that does not depend on the learner's operators.
std::string txt(const geo::Point& p) { return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")"; }
std::string txt(const geo::Rect& r) { return "[" + txt(r.origin) + " " + std::to_string(r.width) + "x" + std::to_string(r.height) + "]"; }

geo::Point P(int x, int y) { return geo::Point{x, y}; }
geo::Rect R(int x, int y, int w, int h) { return geo::Rect{geo::Point{x, y}, w, h}; }

// ---- what is at file scope in geometry.hpp? --------------------------------------------------------------------
struct Layout {
    std::vector<std::string> top_level_namespaces;
    std::string leftover;      // whatever is not inside a namespace (other than preprocessor lines)
    bool using_directive = false;
    bool unnamed_namespace = false;
    bool has_detail = false;
};

bool is_word_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

Layout analyze(const std::string& code) {
    Layout L;
    std::string out;
    const std::size_t n = code.size();
    std::size_t i = 0;
    while (i < n) {
        if (code[i] == '#') {  // preprocessor line
            while (i < n && code[i] != '\n') ++i;
            continue;
        }
        if (code.compare(i, 9, "namespace") == 0 && (i == 0 || !is_word_char(code[i - 1])) && (i + 9 >= n || !is_word_char(code[i + 9]))) {
            std::size_t j = out.size();
            while (j > 0 && std::isspace(static_cast<unsigned char>(out[j - 1]))) --j;
            const bool after_using = j >= 5 && out.compare(j - 5, 5, "using") == 0;
            std::size_t k = i + 9;
            while (k < n && code[k] != '{' && code[k] != ';' && code[k] != '=') ++k;
            if (after_using) {
                L.using_directive = true;
            } else if (k < n && code[k] == '{') {
                std::string name;
                for (std::size_t m = i + 9; m < k; ++m)
                    if (!std::isspace(static_cast<unsigned char>(code[m]))) name += code[m];
                if (name.empty()) L.unnamed_namespace = true;
                const std::string first = name.substr(0, name.find("::"));
                if (!name.empty()) L.top_level_namespaces.push_back(first);
                // find this namespace's closing brace; note `detail` namespaces on the way
                int depth = 0;
                std::size_t m = k;
                for (; m < n; ++m) {
                    if (code[m] == '{') ++depth;
                    else if (code[m] == '}' && --depth == 0) { ++m; break; }
                }
                if (code.substr(k, m - k).find("namespace detail") != std::string::npos || name.find("detail") != std::string::npos) L.has_detail = true;
                i = m;
                continue;
            }
        }
        // `namespace {` anywhere inside, e.g. nested in geo, is also an unnamed namespace
        out += code[i];
        ++i;
    }
    // nested unnamed namespaces (inside geo) are not seen above: look for them textually
    for (std::size_t p = 0; (p = code.find("namespace", p)) != std::string::npos; p += 9) {
        std::size_t q = p + 9;
        while (q < n && std::isspace(static_cast<unsigned char>(code[q]))) ++q;
        if (q < n && code[q] == '{') L.unnamed_namespace = true;
        std::size_t b = p;
        while (b > 0 && std::isspace(static_cast<unsigned char>(code[b - 1]))) --b;
        if (b >= 5 && code.compare(b - 5, 5, "using") == 0) L.using_directive = true;
    }
    for (std::size_t p = 0; (p = code.find("namespace detail", p)) != std::string::npos; p += 16) L.has_detail = true;
    std::size_t a = 0;
    while (a < out.size() && std::isspace(static_cast<unsigned char>(out[a]))) ++a;
    std::size_t z = out.size();
    while (z > a && std::isspace(static_cast<unsigned char>(out[z - 1]))) --z;
    L.leftover = out.substr(a, z - a);
    return L;
}

Layout my_layout() { return analyze(::zth::strip_comments_and_literals(::zth::read_file("geometry.hpp"))); }

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "area is width times height") {
    ZTH_EXPECT_EQ(geo::area(R(0, 0, 8, 5)), 40LL);
    ZTH_EXPECT_EQ(geo::area(R(3, -2, 1, 1)), 1LL);
    ZTH_EXPECT_EQ(geo::area(R(0, 0, 0, 7)), 0LL);
    ZTH_EXPECT_EQ(geo::area(R(0, 0, 7, 0)), 0LL);
}
ZTH_TEST("works", "contains: the top and left edges are inside, the right and bottom edges are outside") {
    const geo::Rect r = R(2, 3, 4, 5);  // x in [2, 6), y in [3, 8)
    ZTH_EXPECT_TRUE(geo::contains(r, P(2, 3)));
    ZTH_EXPECT_TRUE(geo::contains(r, P(5, 7)));
    ZTH_EXPECT_TRUE(geo::contains(r, P(4, 5)));
    ZTH_EXPECT_FALSE(geo::contains(r, P(6, 5)));
    ZTH_EXPECT_FALSE(geo::contains(r, P(4, 8)));
    ZTH_EXPECT_FALSE(geo::contains(r, P(1, 5)));
    ZTH_EXPECT_FALSE(geo::contains(r, P(4, 2)));
    ZTH_EXPECT_FALSE(geo::contains(r, P(100, 100)));
}
ZTH_TEST("works", "overlaps is true when two rectangles share a point, and symmetric") {
    const geo::Rect a = R(0, 0, 8, 5), b = R(6, 3, 4, 4), c = R(20, 20, 2, 2);
    ZTH_EXPECT_TRUE(geo::overlaps(a, b));
    ZTH_EXPECT_TRUE(geo::overlaps(b, a));
    ZTH_EXPECT_FALSE(geo::overlaps(a, c));
    ZTH_EXPECT_FALSE(geo::overlaps(c, a));
    ZTH_EXPECT_TRUE(geo::overlaps(a, R(1, 1, 2, 2)));  // one inside the other
    ZTH_EXPECT_TRUE(geo::overlaps(R(1, 1, 2, 2), a));
    ZTH_EXPECT_TRUE(geo::overlaps(a, a));
}
ZTH_TEST("works", "bounding_box is the smallest rectangle that contains all the points") {
    const geo::Rect bb = geo::bounding_box({{2, 1}, {9, -2}, {4, 6}});
    ZTH_EXPECT_EQ(txt(bb), txt(R(2, -2, 8, 9)));
}
ZTH_TEST("works", "a Point prints as (x, y) and a Rect as [(x, y) WxH]") {
    std::ostringstream os;
    os << P(3, -4);
    ZTH_EXPECT_EQ(os.str(), std::string("(3, -4)"));
    std::ostringstream os2;
    os2 << R(1, 2, 3, 4);
    ZTH_EXPECT_EQ(os2.str(), std::string("[(1, 2) 3x4]"));
}
ZTH_TEST("works", "== compares every member of a Point and of a Rect") {
    ZTH_EXPECT_TRUE(P(1, 2) == P(1, 2));
    ZTH_EXPECT_FALSE(P(1, 2) == P(1, 3));
    ZTH_EXPECT_FALSE(P(1, 2) == P(0, 2));
    ZTH_EXPECT_TRUE(R(1, 2, 3, 4) == R(1, 2, 3, 4));
    ZTH_EXPECT_FALSE(R(1, 2, 3, 4) == R(0, 2, 3, 4));
    ZTH_EXPECT_FALSE(R(1, 2, 3, 4) == R(1, 2, 4, 4));
    ZTH_EXPECT_FALSE(R(1, 2, 3, 4) == R(1, 2, 3, 5));
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "rectangles that only touch along an edge or a corner do not overlap") {
    const geo::Rect a = R(0, 0, 4, 4);
    ZTH_EXPECT_FALSE(geo::overlaps(a, R(4, 0, 4, 4)));  // right neighbour
    ZTH_EXPECT_FALSE(geo::overlaps(a, R(0, 4, 4, 4)));  // below
    ZTH_EXPECT_FALSE(geo::overlaps(a, R(4, 4, 4, 4)));  // corner
    ZTH_EXPECT_FALSE(geo::overlaps(R(4, 0, 4, 4), a));
    ZTH_EXPECT_TRUE(geo::overlaps(a, R(3, 3, 4, 4)));   // one column and one row of overlap
    ZTH_EXPECT_FALSE(geo::overlaps(a, R(3, 4, 4, 4)));  // overlaps in x only
    ZTH_EXPECT_FALSE(geo::overlaps(a, R(4, 3, 4, 4)));  // overlaps in y only
}
ZTH_TEST("robust", "an empty rectangle overlaps nothing, not even a rectangle around it") {
    const geo::Rect big = R(0, 0, 10, 10);
    ZTH_EXPECT_FALSE(geo::overlaps(big, R(5, 5, 0, 3)));
    ZTH_EXPECT_FALSE(geo::overlaps(big, R(5, 5, 3, 0)));
    ZTH_EXPECT_FALSE(geo::overlaps(R(5, 5, 0, 0), big));
    ZTH_EXPECT_FALSE(geo::overlaps(R(5, 5, 0, 0), R(5, 5, 0, 0)));
    ZTH_EXPECT_FALSE(geo::contains(R(5, 5, 0, 3), P(5, 5)));
}
ZTH_TEST("robust", "bounding_box: one point, negative coordinates, a first point that is not the smallest") {
    ZTH_EXPECT_EQ(txt(geo::bounding_box({{3, 4}})), txt(R(3, 4, 1, 1)));
    ZTH_EXPECT_EQ(txt(geo::bounding_box({{-5, -9}, {-2, -3}})), txt(R(-5, -9, 4, 7)));
    ZTH_EXPECT_EQ(txt(geo::bounding_box({{5, 5}, {1, 8}, {3, 2}})), txt(R(1, 2, 5, 7)));
    ZTH_EXPECT_EQ(txt(geo::bounding_box({{-1, -1}, {-1, -1}})), txt(R(-1, -1, 1, 1)));
}
ZTH_TEST("robust", "bounding_box of no points throws std::invalid_argument") {
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([] { (void)geo::bounding_box({}); }));
    ZTH_EXPECT_FALSE(throws<std::exception>([] { (void)geo::bounding_box({{0, 0}}); }));
}
ZTH_TEST("robust", "huge coordinates do not overflow an int") {
    ZTH_EXPECT_EQ(geo::area(R(0, 0, 100000, 100000)), 10000000000LL);
    ZTH_EXPECT_TRUE(geo::contains(R(INT_MAX, 0, 1, 1), P(INT_MAX, 0)));
    ZTH_EXPECT_TRUE(geo::contains(R(INT_MAX - 2, 0, 2, 1), P(INT_MAX - 1, 0)));
    ZTH_EXPECT_FALSE(geo::contains(R(INT_MAX - 2, 0, 2, 1), P(INT_MAX, 0)));
    ZTH_EXPECT_TRUE(geo::overlaps(R(INT_MAX, 0, 1, 1), R(INT_MAX, 0, 1, 1)));
    ZTH_EXPECT_FALSE(geo::overlaps(R(INT_MAX - 1, 0, 1, 1), R(INT_MAX, 0, 1, 1)));
    ZTH_EXPECT_TRUE(geo::overlaps(R(INT_MIN, 0, 5, 1), R(INT_MIN + 4, 0, 5, 1)));
}
ZTH_TEST("robust", "operator<< for Point and Rect returns the stream and is found by argument-dependent lookup (it lives in namespace geo)") {
    ZTH_EXPECT_TRUE(probe::printable_via_adl<geo::Point>);
    ZTH_EXPECT_TRUE(probe::printable_via_adl<geo::Rect>);
}
ZTH_TEST("robust", "operator== for Point and Rect is found by argument-dependent lookup (inside the class, or in namespace geo)") {
    ZTH_EXPECT_TRUE(probe::comparable_via_adl<geo::Point>);
    ZTH_EXPECT_TRUE(probe::comparable_via_adl<geo::Rect>);
}
ZTH_TEST("robust", "contains, overlaps and area agree with a brute-force count over many small random rectangles") {
    unsigned long long state = 7;
    auto next = [&](int mod) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<int>((state >> 33) % static_cast<unsigned>(mod));
    };
    for (int round = 0; round < 400; ++round) {
        const geo::Rect a = R(next(11) - 5, next(11) - 5, next(7), next(7));
        const geo::Rect b = R(next(11) - 5, next(11) - 5, next(7), next(7));
        long long count_a = 0;
        bool shared = false;
        for (int x = -6; x <= 12; ++x)
            for (int y = -6; y <= 12; ++y) {
                const bool in_a = x >= a.origin.x && x < a.origin.x + a.width && y >= a.origin.y && y < a.origin.y + a.height;
                const bool in_b = x >= b.origin.x && x < b.origin.x + b.width && y >= b.origin.y && y < b.origin.y + b.height;
                count_a += in_a;
                shared = shared || (in_a && in_b);
                if (geo::contains(a, P(x, y)) != in_a) {
                    ::zth::fail("contains(" + txt(a) + ", " + txt(P(x, y)) + ") should be " + (in_a ? "true" : "false"));
                    return;
                }
            }
        ZTH_EXPECT_EQ(geo::area(a), count_a);
        if (geo::overlaps(a, b) != shared) {
            ::zth::fail("overlaps(" + txt(a) + ", " + txt(b) + ") should be " + (shared ? "true" : "false"));
            return;
        }
    }
}
ZTH_TEST("robust", "bounding_box contains every point and is as small as possible, for many random point sets") {
    unsigned long long state = 11;
    auto next = [&](int mod) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<int>((state >> 33) % static_cast<unsigned>(mod));
    };
    for (int round = 0; round < 200; ++round) {
        std::vector<geo::Point> pts;
        const int n = 1 + next(8);
        for (int i = 0; i < n; ++i) pts.push_back(geo::Point{next(41) - 20, next(41) - 20});
        int lo_x = pts[0].x, hi_x = pts[0].x, lo_y = pts[0].y, hi_y = pts[0].y;
        for (const auto& p : pts) {
            lo_x = std::min(lo_x, p.x);
            hi_x = std::max(hi_x, p.x);
            lo_y = std::min(lo_y, p.y);
            hi_y = std::max(hi_y, p.y);
        }
        const geo::Rect bb = geo::bounding_box(pts);
        ZTH_EXPECT_EQ(txt(bb), txt(R(lo_x, lo_y, hi_x - lo_x + 1, hi_y - lo_y + 1)));
        for (const auto& p : pts) ZTH_EXPECT_TRUE(geo::contains(bb, p));
    }
}

// ---- Clean: the header's hygiene (the test reads your geometry.hpp) -----------------------------------------
ZTH_TEST("clean", "geometry.hpp has no `using namespace` directive (it would force std's names on every includer)") {
    ZTH_EXPECT_FALSE(my_layout().using_directive);
}
ZTH_TEST("clean", "geometry.hpp has no unnamed namespace (in a header, every file would get its own copy)") {
    ZTH_EXPECT_FALSE(my_layout().unnamed_namespace);
}
ZTH_TEST("clean", "everything in geometry.hpp is inside namespace geo: nothing at file scope, and no second top-level namespace") {
    const Layout L = my_layout();
    if (!L.leftover.empty()) {
        ::zth::fail("there is code outside any namespace: `" + L.leftover.substr(0, 70) + "...` (move it inside namespace geo)");
        return;
    }
    for (const auto& name : L.top_level_namespaces) {
        if (name != "geo") {
            ::zth::fail("`namespace " + name + "` is at file scope: put it inside namespace geo (as geo::" + name + ")");
            return;
        }
    }
    ZTH_EXPECT_TRUE(!L.top_level_namespaces.empty());
}
ZTH_TEST("clean", "the helper functions live in a nested namespace geo::detail") {
    ZTH_EXPECT_TRUE(my_layout().has_detail);
}
