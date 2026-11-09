#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "three_ways.hpp"
#include "zth_test.hpp"

namespace {

// The rule at the top of three_ways.hpp, written once more here, independently.
double expected_area(std::size_t i) {
    switch (i % 3) {
        case 0: {
            const double r = static_cast<double>(i % 5 + 1);
            return kPi * r * r;
        }
        case 1: return static_cast<double>(i % 4 + 1) * static_cast<double>(i % 3 + 1);
        default: return 0.5 * static_cast<double>(i % 6 + 1) * static_cast<double>(i % 2 + 1);
    }
}
double expected_total(std::size_t n) {
    double sum = 0;
    for (std::size_t i = 0; i < n; ++i) sum += expected_area(i);
    return sum;
}

// A type from "somebody else": no base class, no virtual function, an `area` found by argument-dependent lookup.
struct Square {
    double side;
};
double area(const Square& s) { return s.side * s.side; }

// Counts its own lifetime: how many exist, and how many were COPIED.
struct Counted {
    static inline int alive = 0, copies = 0, moves = 0;
    double size = 2;
    Counted() { ++alive; }
    Counted(const Counted& o) : size(o.size) { ++alive, ++copies; }
    Counted(Counted&& o) noexcept : size(o.size) { ++alive, ++moves; }
    Counted& operator=(const Counted&) = default;
    ~Counted() { --alive; }
};
double area(const Counted& c) { return c.size; }

// A class that derives from the WRONG StaticShape (the CRTP mistake): it must not even be constructible.
struct Bad : StaticShape<SCircle> {};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "the virtual baseline: total area of the rule's shapes") {
    ZTH_EXPECT_NEAR(total_area_virtual(make_virtual(30)), expected_total(30), 1e-9);
    const auto v = make_virtual(3);
    ZTH_EXPECT_EQ(v[0]->name(), std::string("circle"));
    ZTH_EXPECT_EQ(v[1]->name(), std::string("rect"));
    ZTH_EXPECT_EQ(v[2]->name(), std::string("triangle"));
}
ZTH_TEST("works", "area and perimeter overloads for the plain structs") {
    ZTH_EXPECT_NEAR(area(Circle{2}), 4 * kPi, 1e-9);
    ZTH_EXPECT_NEAR(area(Rect{3, 4}), 12, 1e-9);
    ZTH_EXPECT_NEAR(area(Tri{3, 4}), 6, 1e-9);
    ZTH_EXPECT_NEAR(perimeter(Circle{2}), 4 * kPi, 1e-9);
    ZTH_EXPECT_NEAR(perimeter(Rect{3, 4}), 14, 1e-9);
    ZTH_EXPECT_NEAR(perimeter(Tri{3, 4}), 12, 1e-9);
}
ZTH_TEST("works", "variant: area_of, perimeter_of and name_of look at the alternative that is held") {
    const Shape c = Circle{1}, r = Rect{2, 5}, t = Tri{6, 8};
    ZTH_EXPECT_NEAR(area_of(c), kPi, 1e-9);
    ZTH_EXPECT_NEAR(area_of(r), 10, 1e-9);
    ZTH_EXPECT_NEAR(area_of(t), 24, 1e-9);
    ZTH_EXPECT_NEAR(perimeter_of(r), 14, 1e-9);
    ZTH_EXPECT_NEAR(perimeter_of(t), 24, 1e-9);
    ZTH_EXPECT_EQ(name_of(c), std::string("circle"));
    ZTH_EXPECT_EQ(name_of(r), std::string("rect"));
    ZTH_EXPECT_EQ(name_of(t), std::string("triangle"));
}
ZTH_TEST("works", "make_variants follows the rule") {
    const auto v = make_variants(8);
    ZTH_EXPECT_EQ(v.size(), std::size_t{8});
    ZTH_EXPECT_EQ(v[0].index(), std::size_t{0});
    ZTH_EXPECT_EQ(v[1].index(), std::size_t{1});
    ZTH_EXPECT_EQ(v[2].index(), std::size_t{2});
    ZTH_EXPECT_EQ(v[3].index(), std::size_t{0});
    ZTH_EXPECT_NEAR(std::get<Circle>(v[3]).radius, 4, 1e-12);
    ZTH_EXPECT_NEAR(std::get<Rect>(v[7]).width, 4, 1e-12);
    ZTH_EXPECT_NEAR(std::get<Rect>(v[7]).height, 2, 1e-12);
    ZTH_EXPECT_NEAR(std::get<Tri>(v[5]).base, 6, 1e-12);
    ZTH_EXPECT_NEAR(std::get<Tri>(v[5]).height, 2, 1e-12);
}
ZTH_TEST("works", "total_area_variant") {
    ZTH_EXPECT_NEAR(total_area_variant(make_variants(30)), expected_total(30), 1e-9);
    ZTH_EXPECT_NEAR(total_area_variant({Circle{1}, Rect{2, 3}}), kPi + 6, 1e-9);
}
ZTH_TEST("works", "CRTP: SCircle and SRect") {
    SCircle c(2);
    SRect r(2, 3);
    ZTH_EXPECT_NEAR(c.area(), 4 * kPi, 1e-9);
    ZTH_EXPECT_NEAR(r.area(), 6, 1e-9);
    ZTH_EXPECT_EQ(c.name(), std::string("circle"));
    ZTH_EXPECT_EQ(r.name(), std::string("rect"));
}
ZTH_TEST("works", "CRTP: behavior written once in the base: scaled_area and bigger_than") {
    SRect r(2, 3);
    ZTH_EXPECT_NEAR(r.scaled_area(2), 24, 1e-9);
    ZTH_EXPECT_NEAR(r.scaled_area(0.5), 1.5, 1e-9);
    ZTH_EXPECT_NEAR(SCircle(1).scaled_area(3), 9 * kPi, 1e-9);
    ZTH_EXPECT_TRUE(SRect(3, 3).bigger_than(SRect(2, 3)));
    ZTH_EXPECT_FALSE(SRect(2, 3).bigger_than(SRect(3, 3)));
}
ZTH_TEST("works", "CRTP: total_area_static of a vector of one type") {
    const std::vector<SRect> rects{SRect(1, 2), SRect(3, 4)};
    ZTH_EXPECT_NEAR(total_area_static(rects), 14, 1e-9);
    ZTH_EXPECT_NEAR(total_area_static(make_static_circles(5)), 55 * kPi, 1e-9);
    ZTH_EXPECT_EQ(make_static_circles(7).size(), std::size_t{7});
}
ZTH_TEST("works", "type erasure: AnyShape holds Circle, Rect and Tri by value") {
    ZTH_EXPECT_NEAR(AnyShape(Circle{1}).area(), kPi, 1e-9);
    ZTH_EXPECT_NEAR(AnyShape(Rect{2, 3}).area(), 6, 1e-9);
    ZTH_EXPECT_NEAR(AnyShape(Tri{3, 4}).area(), 6, 1e-9);
}
ZTH_TEST("works", "type erasure: a type with no base class and no virtual function joins in") {
    ZTH_EXPECT_NEAR(AnyShape(Square{4}).area(), 16, 1e-9);
    std::vector<AnyShape> mixed;
    mixed.emplace_back(Circle{1});
    mixed.emplace_back(Square{3});
    ZTH_EXPECT_NEAR(total_area_erased(mixed), kPi + 9, 1e-9);
}
ZTH_TEST("works", "make_erased and total_area_erased follow the rule") {
    const auto v = make_erased(30);
    ZTH_EXPECT_EQ(v.size(), std::size_t{30});
    ZTH_EXPECT_NEAR(total_area_erased(v), expected_total(30), 1e-9);
    ZTH_EXPECT_NEAR(v[4].area(), expected_area(4), 1e-9);
}
ZTH_TEST("works", "all four ways agree") {
    for (std::size_t n : {0u, 1u, 2u, 3u, 10u, 100u}) {
        const double t = expected_total(n);
        ZTH_EXPECT_NEAR(total_area_virtual(make_virtual(n)), t, 1e-9);
        ZTH_EXPECT_NEAR(total_area_variant(make_variants(n)), t, 1e-9);
        ZTH_EXPECT_NEAR(total_area_erased(make_erased(n)), t, 1e-9);
    }
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "variant: the alternative is chosen by what it holds, also after assignment") {
    Shape s = Circle{1};
    ZTH_EXPECT_EQ(name_of(s), std::string("circle"));
    s = Rect{2, 2};
    ZTH_EXPECT_EQ(name_of(s), std::string("rect"));
    ZTH_EXPECT_NEAR(area_of(s), 4, 1e-9);
    s = Tri{2, 2};
    ZTH_EXPECT_EQ(name_of(s), std::string("triangle"));
    ZTH_EXPECT_NEAR(perimeter_of(s), 4 + std::sqrt(8.0), 1e-9);
}
ZTH_TEST("robust", "variant: empty input, and a variant copies by value") {
    ZTH_EXPECT_TRUE(make_variants(0).empty());
    ZTH_EXPECT_NEAR(total_area_variant({}), 0, 1e-12);
    auto a = make_variants(5);
    auto b = a;
    b[0] = Rect{10, 10};
    ZTH_EXPECT_EQ(name_of(a[0]), std::string("circle"));
}
ZTH_TEST("robust", "CRTP has no virtual functions: a SCircle is exactly one double") {
    ZTH_EXPECT_FALSE(std::is_polymorphic_v<SCircle>);
    ZTH_EXPECT_FALSE(std::is_polymorphic_v<SRect>);
    ZTH_EXPECT_EQ(sizeof(SCircle), sizeof(double));
    ZTH_EXPECT_EQ(sizeof(SRect), 2 * sizeof(double));
}
ZTH_TEST("robust", "CRTP: the base is a template on the derived class, and different shapes share no base") {
    ZTH_EXPECT_TRUE((std::is_base_of_v<StaticShape<SCircle>, SCircle>));
    ZTH_EXPECT_TRUE((std::is_base_of_v<StaticShape<SRect>, SRect>));
    ZTH_EXPECT_FALSE((std::is_base_of_v<StaticShape<SCircle>, SRect>));
    ZTH_EXPECT_FALSE((std::is_convertible_v<SRect*, StaticShape<SCircle>*>));
}
ZTH_TEST("robust", "CRTP: deriving from the wrong StaticShape<> is rejected (private constructor, friend Derived)") {
    ZTH_EXPECT_FALSE(std::is_default_constructible_v<Bad>);
    ZTH_EXPECT_TRUE((std::is_constructible_v<SCircle, double>));
    ZTH_EXPECT_TRUE((std::is_constructible_v<SRect, double, double>));
}
ZTH_TEST("robust", "CRTP: bigger_than is a strict comparison") {
    ZTH_EXPECT_FALSE(SRect(2, 3).bigger_than(SRect(3, 2)));
    ZTH_EXPECT_NEAR(SRect(0, 5).scaled_area(10), 0, 1e-12);
}
ZTH_TEST("robust", "AnyShape built from an rvalue moves the value in, from an lvalue copies it once") {
    Counted::alive = Counted::copies = Counted::moves = 0;
    {
        AnyShape a(Counted{});
        ZTH_EXPECT_EQ(Counted::copies, 0);
        ZTH_EXPECT_EQ(Counted::alive, 1);
        Counted c;
        AnyShape b(c);
        ZTH_EXPECT_EQ(Counted::copies, 1);
        ZTH_EXPECT_EQ(Counted::alive, 3);
        ZTH_EXPECT_NEAR(a.area() + b.area(), 4, 1e-12);
    }
    ZTH_EXPECT_EQ(Counted::alive, 0);
}
ZTH_TEST("robust", "copying an AnyShape copies the HELD value (value semantics), destroying it destroys it") {
    Counted::alive = Counted::copies = Counted::moves = 0;
    {
        AnyShape a(Counted{});
        AnyShape b = a;
        ZTH_EXPECT_EQ(Counted::alive, 2);
        ZTH_EXPECT_EQ(Counted::copies, 1);
        ZTH_EXPECT_NEAR(b.area(), 2, 1e-12);
    }
    ZTH_EXPECT_EQ(Counted::alive, 0);
}
ZTH_TEST("robust", "AnyShape assignment: replaces the held value, self-assignment is safe") {
    Counted::alive = Counted::copies = Counted::moves = 0;
    {
        AnyShape a(Square{3});
        AnyShape b(Counted{});
        ZTH_EXPECT_EQ(Counted::alive, 1);
        b = a;  // the Counted that b held is destroyed
        ZTH_EXPECT_EQ(Counted::alive, 0);
        ZTH_EXPECT_NEAR(b.area(), 9, 1e-12);
        AnyShape& self = a;
        a = self;
        ZTH_EXPECT_NEAR(a.area(), 9, 1e-12);
    }
    ZTH_EXPECT_EQ(Counted::alive, 0);
}
ZTH_TEST("robust", "AnyShape move: the held value is not copied, and the shapes keep their area") {
    Counted::alive = Counted::copies = Counted::moves = 0;
    {
        AnyShape a(Counted{});
        const int moves_before = Counted::moves;
        AnyShape b = std::move(a);
        ZTH_EXPECT_EQ(Counted::copies, 0);
        ZTH_EXPECT_EQ(Counted::moves, moves_before);  // a pointer move, not a value move
        ZTH_EXPECT_NEAR(b.area(), 2, 1e-12);
    }
    ZTH_EXPECT_EQ(Counted::alive, 0);
}
ZTH_TEST("robust", "AnyShape in a vector: copying the vector copies every held shape independently") {
    auto a = make_erased(9);
    auto b = a;
    ZTH_EXPECT_NEAR(total_area_erased(a), total_area_erased(b), 1e-12);
    b.emplace_back(Square{5});
    ZTH_EXPECT_NEAR(total_area_erased(b), total_area_erased(a) + 25, 1e-9);
}
ZTH_TEST("robust", "no leaks or double frees in any of the ways") {
    zth::mem::Scope scope;
    {
        auto v = make_virtual(20);
        auto e = make_erased(20);
        auto e2 = e;
        e2.emplace_back(Square{1});
        e = e2;
        total_area_virtual(v);
        total_area_erased(e);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}

// ================================================ Fast ========================================================
// The same thousand shapes, four ways: count the heap allocations and look at the sizes. That is the cost the choice of technique makes visible.
ZTH_TEST("fast", "a vector of variants is ONE allocation: the shapes live inside the vector") {
    zth::mem::Scope scope;
    const auto v = make_variants(1000);
    ZTH_EXPECT_EQ(v.size(), std::size_t{1000});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
}
ZTH_TEST("fast", "a vector of CRTP shapes is ONE allocation too") {
    zth::mem::Scope scope;
    const auto v = make_static_circles(1000);
    ZTH_EXPECT_EQ(v.size(), std::size_t{1000});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
}
ZTH_TEST("fast", "virtual and type-erased shapes cost one allocation per shape, plus one for the vector (reserve)") {
    {
        zth::mem::Scope scope;
        const auto v = make_virtual(1000);
        ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1001);
    }
    {
        zth::mem::Scope scope;
        const auto v = make_erased(1000);
        ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1001);
    }
}
ZTH_TEST("fast", "adding up the areas allocates nothing, whichever way the shapes are held") {
    const auto virt = make_virtual(300);
    const auto vari = make_variants(300);
    const auto stat = make_static_circles(300);
    const auto eras = make_erased(300);
    zth::mem::Scope scope;
    double sum = total_area_virtual(virt) + total_area_variant(vari) + total_area_static(stat) + total_area_erased(eras);
    ZTH_EXPECT_TRUE(sum > 0);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "copying a vector of type-erased shapes: one clone per shape plus the vector") {
    const auto a = make_erased(100);
    zth::mem::Scope scope;
    const auto b = a;
    ZTH_EXPECT_EQ(b.size(), std::size_t{100});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 101);
}
ZTH_TEST("fast", "sizes: a variant shape has no pointer to chase, a CRTP shape has no vptr") {
    ZTH_EXPECT_TRUE(sizeof(Shape) <= 24);
    ZTH_EXPECT_EQ(sizeof(SCircle), sizeof(double));
    ZTH_EXPECT_TRUE(sizeof(VCircle) > sizeof(double));  // the vptr
}
ZTH_TEST("fast", "a million areas through the variant and through the static shapes") {
    const auto v = make_variants(1000);
    const auto s = make_static_circles(1000);
    double sum = 0;
    for (int round = 0; round < 1000; ++round) sum += total_area_variant(v) + total_area_static(s);
    ZTH_EXPECT_TRUE(sum > 0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "three_ways.hpp does not use new, delete, malloc or free") {
    ZTH_EXPECT_NO_RAW_MEMORY("three_ways.hpp");
}
ZTH_TEST("clean", "no dynamic_cast or typeid: none of the four ways needs run-time type inspection") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("three_ways.hpp"));
    ZTH_EXPECT_EQ(::zth::find_word(code, "dynamic_cast").size(), std::size_t{0});
    ZTH_EXPECT_EQ(::zth::find_word(code, "typeid").size(), std::size_t{0});
}
ZTH_TEST("clean", "the variant functions use std::visit (at least 3 times), not index() or holds_alternative chains") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("three_ways.hpp"));
    const std::size_t visits = ::zth::find_word(code, "visit").size();
    if (visits < 3) {
        ::zth::fail("three_ways.hpp uses std::visit " + std::to_string(visits) + " time(s): area_of, perimeter_of and name_of each visit");
        return;
    }
    ZTH_EXPECT_EQ(::zth::find_word(code, "holds_alternative").size(), std::size_t{0});
    ZTH_EXPECT_EQ(::zth::find_word(code, "index").size(), std::size_t{0});
}
ZTH_TEST("clean", "three_ways.hpp has no `using namespace`") {
    ZTH_EXPECT_FALSE(::zth::source_layout_of("three_ways.hpp").using_directive);
}
