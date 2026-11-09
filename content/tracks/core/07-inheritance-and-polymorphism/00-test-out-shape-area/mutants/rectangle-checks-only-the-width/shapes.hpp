// MUTANT: a negative height is accepted.
#pragma once

#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// The interface: four pure virtual functions and a VIRTUAL destructor, so that deleting a Shape through a Shape pointer
// (which is what std::unique_ptr<Shape> does) runs the destructor of the real, derived object.
class Shape {
public:
    virtual ~Shape() = default;
    virtual double area() const = 0;
    virtual double perimeter() const = 0;
    virtual std::string name() const = 0;
    virtual std::unique_ptr<Shape> clone() const = 0;  // a copy of the REAL object (a "virtual constructor")
};

using Shapes = std::vector<std::unique_ptr<Shape>>;

class Circle final : public Shape {
public:
    explicit Circle(double radius) : radius_(radius) {
        if (!(radius >= 0)) throw std::invalid_argument("a radius must not be negative");
    }
    double area() const override { return std::numbers::pi * radius_ * radius_; }
    double perimeter() const override { return 2 * std::numbers::pi * radius_; }
    std::string name() const override { return "circle"; }
    std::unique_ptr<Shape> clone() const override { return std::make_unique<Circle>(*this); }

private:
    double radius_;
};

class Rectangle : public Shape {
public:
    Rectangle(double width, double height) : width_(width), height_(height) {
        if (!(width >= 0)) throw std::invalid_argument("a side must not be negative");
    }
    double area() const override { return width_ * height_; }
    double perimeter() const override { return 2 * (width_ + height_); }
    std::string name() const override { return "rectangle"; }
    std::unique_ptr<Shape> clone() const override { return std::make_unique<Rectangle>(*this); }

private:
    double width_;
    double height_;
};

class Square final : public Rectangle {
public:
    explicit Square(double side) : Rectangle(side, side) {}
    std::string name() const override { return "square"; }
    std::unique_ptr<Shape> clone() const override { return std::make_unique<Square>(*this); }
};

class Triangle final : public Shape {
public:
    Triangle(double a, double b, double c) : a_(a), b_(b), c_(c) {
        if (!(a > 0 && b > 0 && c > 0 && a + b > c && a + c > b && b + c > a)) throw std::invalid_argument("these sides do not make a triangle");
    }
    double area() const override {
        const double s = (a_ + b_ + c_) / 2;  // Heron's formula
        return std::sqrt(s * (s - a_) * (s - b_) * (s - c_));
    }
    double perimeter() const override { return a_ + b_ + c_; }
    std::string name() const override { return "triangle"; }
    std::unique_ptr<Shape> clone() const override { return std::make_unique<Triangle>(*this); }

private:
    double a_, b_, c_;
};

// "circle" (radius), "rectangle" (width, height), "square" (side), "triangle" (a, b, c).
// An unknown kind or the wrong number of arguments: std::invalid_argument.
inline std::unique_ptr<Shape> make_shape(const std::string& kind, const std::vector<double>& args) {
    const auto need = [&](std::size_t n) {
        if (args.size() != n) throw std::invalid_argument(kind + " needs " + std::to_string(n) + " number(s)");
    };
    if (kind == "circle") {
        need(1);
        return std::make_unique<Circle>(args[0]);
    }
    if (kind == "rectangle") {
        need(2);
        return std::make_unique<Rectangle>(args[0], args[1]);
    }
    if (kind == "square") {
        need(1);
        return std::make_unique<Square>(args[0]);
    }
    if (kind == "triangle") {
        need(3);
        return std::make_unique<Triangle>(args[0], args[1], args[2]);
    }
    throw std::invalid_argument("unknown shape: " + kind);
}

// The sum of the areas. A null pointer in the vector is skipped.
inline double total_area(const Shapes& shapes) {
    double sum = 0;
    for (const auto& s : shapes)
        if (s) sum += s->area();
    return sum;
}

// The shape with the largest area (the FIRST of equal ones); nullptr for an empty vector (null entries are skipped).
inline const Shape* largest(const Shapes& shapes) {
    const Shape* best = nullptr;
    for (const auto& s : shapes)
        if (s && (!best || s->area() > best->area())) best = s.get();
    return best;
}

// A deep copy: every shape cloned (a null stays null). One allocation per shape plus the vector's own.
inline Shapes clone_all(const Shapes& shapes) {
    Shapes copy;
    copy.reserve(shapes.size());
    for (const auto& s : shapes) copy.push_back(s ? s->clone() : nullptr);
    return copy;
}
