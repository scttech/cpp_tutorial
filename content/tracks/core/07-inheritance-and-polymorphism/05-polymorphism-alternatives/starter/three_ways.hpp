#pragma once

#include <cmath>
#include <concepts>
#include <cstddef>
#include <memory>
#include <numbers>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// The same three shapes, four ways. Shape number i of every collection is built by the same rule:
//   i % 3 == 0: a circle,    radius i % 5 + 1
//   i % 3 == 1: a rectangle, width i % 4 + 1, height i % 3 + 1
//   i % 3 == 2: a triangle (a right triangle: base and height are the two legs), base i % 6 + 1, height i % 2 + 1
// Part 1 (virtual functions) is the GIVEN baseline. You write parts 2, 3 and 4, then compare them in the Fast tier.
constexpr double kPi = std::numbers::pi;

// ==== 1. Virtual functions (given): the open set of shapes, one heap object each ======================================================================
class VShape {
public:
    virtual ~VShape() = default;
    virtual double area() const = 0;
    virtual double perimeter() const = 0;
    virtual std::string name() const = 0;
};
class VCircle final : public VShape {
public:
    explicit VCircle(double r) : r_(r) {}
    double area() const override { return kPi * r_ * r_; }
    double perimeter() const override { return 2 * kPi * r_; }
    std::string name() const override { return "circle"; }

private:
    double r_;
};
class VRect final : public VShape {
public:
    VRect(double w, double h) : w_(w), h_(h) {}
    double area() const override { return w_ * h_; }
    double perimeter() const override { return 2 * (w_ + h_); }
    std::string name() const override { return "rect"; }

private:
    double w_, h_;
};
class VTri final : public VShape {
public:
    VTri(double b, double h) : b_(b), h_(h) {}
    double area() const override { return 0.5 * b_ * h_; }
    double perimeter() const override { return b_ + h_ + std::sqrt(b_ * b_ + h_ * h_); }
    std::string name() const override { return "triangle"; }

private:
    double b_, h_;
};

using VShapes = std::vector<std::unique_ptr<VShape>>;

inline VShapes make_virtual(std::size_t n) {
    VShapes out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double a = static_cast<double>(i % 5 + 1), w = static_cast<double>(i % 4 + 1), h = static_cast<double>(i % 3 + 1);
        const double b = static_cast<double>(i % 6 + 1), th = static_cast<double>(i % 2 + 1);
        if (i % 3 == 0) out.push_back(std::make_unique<VCircle>(a));
        else if (i % 3 == 1) out.push_back(std::make_unique<VRect>(w, h));
        else out.push_back(std::make_unique<VTri>(b, th));
    }
    return out;
}
inline double total_area_virtual(const VShapes& shapes) {
    double sum = 0;
    for (const auto& s : shapes) sum += s->area();
    return sum;
}

// ==== 2. std::variant: a CLOSED set of plain data types, no base class, no pointers ================================================================
struct Circle {
    double radius;
};
struct Rect {
    double width, height;
};
struct Tri {
    double base, height;
};

// TODO 1: the operations live OUTSIDE the data, as plain overloaded functions (the type-erased wrapper in part 4 will use them too).
//         area: circle pi r^2, rect w * h, triangle base * height / 2.   perimeter: circle 2 pi r, rect 2 (w + h), triangle base + height + hypotenuse.
inline double area(const Circle& c) {
    (void)c;
    return 0;
}
inline double area(const Rect& r) {
    (void)r;
    return 0;
}
inline double area(const Tri& t) {
    (void)t;
    return 0;
}
inline double perimeter(const Circle& c) {
    (void)c;
    return 0;
}
inline double perimeter(const Rect& r) {
    (void)r;
    return 0;
}
inline double perimeter(const Tri& t) {
    (void)t;
    return 0;
}

using Shape = std::variant<Circle, Rect, Tri>;

// The "overloaded" helper: one function object from several lambdas, for std::visit. (Given.)
template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

// TODO 2: area_of and perimeter_of: std::visit with a GENERIC lambda (`[](const auto& shape) { return area(shape); }`: the same code for every
//         alternative). name_of: std::visit with Overloaded{...}: one lambda per alternative returning "circle", "rect" or "triangle".
//         (Leave an alternative out of an Overloaded visitor and the program does not compile: a closed set is checked for you.)
inline double area_of(const Shape& s) {
    (void)s;
    return 0;
}
inline double perimeter_of(const Shape& s) {
    (void)s;
    return 0;
}
inline std::string name_of(const Shape& s) {
    (void)s;
    return "";
}

// TODO 3: make_variants(n) by the rule at the top of the file; one vector allocation (reserve). total_area_variant adds the areas.
inline std::vector<Shape> make_variants(std::size_t n) {
    (void)n;
    return {};
}
inline double total_area_variant(const std::vector<Shape>& shapes) {
    (void)shapes;
    return 0;
}

// ==== 3. CRTP: static polymorphism, resolved at COMPILE time =========================================================================================
// TODO 4: StaticShape<Derived> is a base class TEMPLATE on its own derived class, so it can call the derived class's functions WITHOUT virtual
//         (static_cast<const Derived&>(*this)). It provides: area() (calls Derived::area_impl()), name() (Derived::name_impl()),
//         scaled_area(factor) = area * factor * factor, and bigger_than(const Derived& other). Make its constructor private and `friend Derived;`
//         so that only the derived class itself can derive from it (`class SRect : StaticShape<SCircle>` is then a compile error).
//         SCircle(radius) and SRect(width, height) are final and provide area_impl() and name_impl() ("circle", "rect").
template <class Derived>
class StaticShape {
public:
    double area() const { return 0; }
    std::string name() const { return ""; }
    double scaled_area(double factor) const {
        (void)factor;
        return 0;
    }
    bool bigger_than(const Derived& other) const {
        (void)other;
        return false;
    }
};

class SCircle final : public StaticShape<SCircle> {
public:
    explicit SCircle(double r) { (void)r; }
    double area_impl() const { return 0; }
    std::string name_impl() const { return ""; }
};
class SRect final : public StaticShape<SRect> {
public:
    SRect(double w, double h) {
        (void)w;
        (void)h;
    }
    double area_impl() const { return 0; }
    std::string name_impl() const { return ""; }
};

// TODO 5: total_area_static for ANY vector of shapes that have area() (a template), and make_static_circles(n): n circles, radius i % 5 + 1.
//         (A std::vector holds ONE type: static polymorphism cannot mix circles and rectangles in a container.)
template <class S>
double total_area_static(const std::vector<S>& shapes) {
    (void)shapes;
    return 0;
}
inline std::vector<SCircle> make_static_circles(std::size_t n) {
    (void)n;
    return {};
}

// ==== 4. Type erasure: value semantics + run-time polymorphism, NO common base class ==============================================================
// AnyShape holds a copy of ANY type T for which `area(const T&)` can be called: Circle, Rect, Tri, or a type someone else wrote, with no
// inheritance and no virtual functions in T. Inside, a small hidden interface (Concept) and a template that implements it for T (Model).
namespace detail {
// A free function so that the lookup of `area` finds the non-member overloads (also by argument-dependent lookup), not Model::area.
template <class T>
double area_via_adl(const T& value) {
    return area(value);
}
}  // namespace detail

class AnyShape {
public:
    // TODO 6: the converting constructor: takes ANY T (not an AnyShape: that is what the requires says) by forwarding reference, decays it
    //         (std::remove_cvref_t) and stores a Model<T> in impl_ (std::make_unique). The copy operations must copy the HELD value:
    //         use Concept::clone(). Moves just move the pointer.
    template <class T>
        requires(!std::same_as<std::remove_cvref_t<T>, AnyShape>)
    AnyShape(T&& value) {
        (void)value;
    }

    AnyShape(const AnyShape& other) { (void)other; }
    AnyShape(AnyShape&&) noexcept = default;
    AnyShape& operator=(const AnyShape& other) {
        (void)other;
        return *this;
    }
    AnyShape& operator=(AnyShape&&) noexcept = default;

    double area() const { return impl_ ? impl_->area() : 0; }

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual double area() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;  // copying through a Concept* needs a virtual function: the copy constructor is static
    };
    // TODO 7: Model<T> implements Concept for a T: it stores a `T value`; area() is detail::area_via_adl(value); clone() copies the Model.
    template <class T>
    struct Model final : Concept {
        explicit Model(T v) : value(std::move(v)) {}
        double area() const override { return 0; }
        std::unique_ptr<Concept> clone() const override { return nullptr; }
        T value;
    };

    std::unique_ptr<Concept> impl_;
};

// TODO 8: make_erased(n) by the rule at the top of the file (the plain Circle/Rect/Tri structs, wrapped in AnyShape; reserve first), and
//         total_area_erased adds the areas.
inline std::vector<AnyShape> make_erased(std::size_t n) {
    (void)n;
    return {};
}
inline double total_area_erased(const std::vector<AnyShape>& shapes) {
    (void)shapes;
    return 0;
}
