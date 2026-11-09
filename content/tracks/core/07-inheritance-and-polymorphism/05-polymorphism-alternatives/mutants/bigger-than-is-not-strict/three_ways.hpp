// MUTANT: >=.
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

// Plain functions on the plain types: the operations live OUTSIDE the data (also what the type-erased wrapper below uses).
inline double area(const Circle& c) { return kPi * c.radius * c.radius; }
inline double area(const Rect& r) { return r.width * r.height; }
inline double area(const Tri& t) { return 0.5 * t.base * t.height; }
inline double perimeter(const Circle& c) { return 2 * kPi * c.radius; }
inline double perimeter(const Rect& r) { return 2 * (r.width + r.height); }
inline double perimeter(const Tri& t) { return t.base + t.height + std::sqrt(t.base * t.base + t.height * t.height); }

using Shape = std::variant<Circle, Rect, Tri>;

// The "overloaded" helper: one function object from several lambdas, for std::visit.
template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

// A generic lambda visits every alternative with the same code ...
inline double area_of(const Shape& s) {
    return std::visit([](const auto& shape) { return area(shape); }, s);
}
inline double perimeter_of(const Shape& s) {
    return std::visit([](const auto& shape) { return perimeter(shape); }, s);
}
// ... an overload set handles each alternative separately. Leave one out and the program does not compile: that is the strength of a closed set.
inline std::string name_of(const Shape& s) {
    return std::visit(Overloaded{[](const Circle&) { return std::string("circle"); }, [](const Rect&) { return std::string("rect"); },
                                 [](const Tri&) { return std::string("triangle"); }},
                      s);
}

inline std::vector<Shape> make_variants(std::size_t n) {
    std::vector<Shape> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (i % 3 == 0) out.push_back(Circle{static_cast<double>(i % 5 + 1)});
        else if (i % 3 == 1) out.push_back(Rect{static_cast<double>(i % 4 + 1), static_cast<double>(i % 3 + 1)});
        else out.push_back(Tri{static_cast<double>(i % 6 + 1), static_cast<double>(i % 2 + 1)});
    }
    return out;
}
inline double total_area_variant(const std::vector<Shape>& shapes) {
    double sum = 0;
    for (const Shape& s : shapes) sum += area_of(s);
    return sum;
}

// ==== 3. CRTP: static polymorphism, resolved at COMPILE time =========================================================================================
// The base class is a TEMPLATE on its own derived class, so it can call the derived class's functions without virtual. No vptr, no vtable, and
// everything can be inlined; the price: one concrete type per container, and shapes of different types share no common base.
template <class Derived>
class StaticShape {
public:
    double area() const { return self().area_impl(); }
    std::string name() const { return self().name_impl(); }
    // Behavior written once for every shape:
    double scaled_area(double factor) const { return area() * factor * factor; }
    bool bigger_than(const Derived& other) const { return area() >= other.area(); }

private:
    StaticShape() = default;
    friend Derived;  // only the Derived class can construct its StaticShape base: `class SRect : StaticShape<SCircle>` is a compile error
    const Derived& self() const { return static_cast<const Derived&>(*this); }
};

class SCircle final : public StaticShape<SCircle> {
public:
    explicit SCircle(double r) : r_(r) {}
    double area_impl() const { return kPi * r_ * r_; }
    std::string name_impl() const { return "circle"; }

private:
    double r_;
};
class SRect final : public StaticShape<SRect> {
public:
    SRect(double w, double h) : w_(w), h_(h) {}
    double area_impl() const { return w_ * h_; }
    std::string name_impl() const { return "rect"; }

private:
    double w_, h_;
};

// Works for ANY container element type that has area(): but a vector holds one type, so no mixed shapes.
template <class S>
double total_area_static(const std::vector<S>& shapes) {
    double sum = 0;
    for (const S& s : shapes) sum += s.area();
    return sum;
}

// n circles, radius i % 5 + 1 (the homogeneous collection that static polymorphism can hold).
inline std::vector<SCircle> make_static_circles(std::size_t n) {
    std::vector<SCircle> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) out.emplace_back(static_cast<double>(i % 5 + 1));
    return out;
}

// ==== 4. Type erasure: value semantics + run-time polymorphism, NO common base class ==============================================================
// AnyShape holds a copy of ANY type T for which `area(const T&)` can be called: Circle, Rect, Tri, or a type someone else wrote, with no inheritance
// and no virtual functions in T. Inside, a small hidden interface (Concept) plus a template that implements it for T (Model): inheritance and virtual
// calls are an implementation detail, invisible to the user.
namespace detail {
// A free function so that the lookup of `area` finds the non-member overloads (also by argument-dependent lookup), not Model::area.
template <class T>
double area_via_adl(const T& value) {
    return area(value);
}
}  // namespace detail

class AnyShape {
public:
    template <class T>
        requires(!std::same_as<std::remove_cvref_t<T>, AnyShape>)
    AnyShape(T&& value) : impl_(std::make_unique<Model<std::remove_cvref_t<T>>>(std::forward<T>(value))) {}

    AnyShape(const AnyShape& other) : impl_(other.impl_ ? other.impl_->clone() : nullptr) {}
    AnyShape(AnyShape&&) noexcept = default;
    AnyShape& operator=(const AnyShape& other) {
        AnyShape copy(other);  // copy first: if it throws, *this is unchanged
        impl_ = std::move(copy.impl_);
        return *this;
    }
    AnyShape& operator=(AnyShape&&) noexcept = default;

    double area() const { return impl_->area(); }

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual double area() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;  // copying a Concept* needs a virtual function: the copy constructor is static
    };
    template <class T>
    struct Model final : Concept {
        explicit Model(T v) : value(std::move(v)) {}
        double area() const override { return detail::area_via_adl(value); }
        std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }
        T value;
    };

    std::unique_ptr<Concept> impl_;
};

inline std::vector<AnyShape> make_erased(std::size_t n) {
    std::vector<AnyShape> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (i % 3 == 0) out.emplace_back(Circle{static_cast<double>(i % 5 + 1)});
        else if (i % 3 == 1) out.emplace_back(Rect{static_cast<double>(i % 4 + 1), static_cast<double>(i % 3 + 1)});
        else out.emplace_back(Tri{static_cast<double>(i % 6 + 1), static_cast<double>(i % 2 + 1)});
    }
    return out;
}
inline double total_area_erased(const std::vector<AnyShape>& shapes) {
    double sum = 0;
    for (const AnyShape& s : shapes) sum += s.area();
    return sum;
}
