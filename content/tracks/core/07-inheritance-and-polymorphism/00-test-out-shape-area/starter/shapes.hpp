#pragma once

#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// A small shape hierarchy. Read the task in the lesson text, then replace every stub below.
//
// The interface: Shape has exactly these four pure virtual functions (the hidden tests derive their own shape from it).
// TODO: look at the destructor. What happens when a std::unique_ptr<Shape> deletes a Circle?
class Shape {
public:
    ~Shape() = default;
    virtual double area() const = 0;
    virtual double perimeter() const = 0;
    virtual std::string name() const = 0;
    virtual std::unique_ptr<Shape> clone() const = 0;  // a copy of the REAL object
};

using Shapes = std::vector<std::unique_ptr<Shape>>;

// TODO: Circle(radius): negative (or NaN) radius -> std::invalid_argument. name "circle".
class Circle final : public Shape {
public:
    explicit Circle(double radius) { (void)radius; }
    double area() const override { return 0; }
    double perimeter() const override { return 0; }
    std::string name() const override { return ""; }
    std::unique_ptr<Shape> clone() const override { return nullptr; }
};

// TODO: Rectangle(width, height): a negative side -> std::invalid_argument. name "rectangle".
class Rectangle : public Shape {
public:
    Rectangle(double width, double height) {
        (void)width;
        (void)height;
    }
    double area() const override { return 0; }
    double perimeter() const override { return 0; }
    std::string name() const override { return ""; }
    std::unique_ptr<Shape> clone() const override { return nullptr; }
};

// TODO: Square(side) IS a Rectangle with equal sides. name "square", and clone() must give back a SQUARE.
class Square final : public Rectangle {
public:
    explicit Square(double side) : Rectangle(side, side) {}
};

// TODO: Triangle(a, b, c): sides that are not all positive, or that do not satisfy the triangle inequality (a degenerate
// triangle counts) -> std::invalid_argument. area: Heron's formula. name "triangle".
class Triangle final : public Shape {
public:
    Triangle(double a, double b, double c) {
        (void)a;
        (void)b;
        (void)c;
    }
    double area() const override { return 0; }
    double perimeter() const override { return 0; }
    std::string name() const override { return ""; }
    std::unique_ptr<Shape> clone() const override { return nullptr; }
};

// TODO: "circle" (radius), "rectangle" (width, height), "square" (side), "triangle" (a, b, c).
// An unknown kind or the wrong number of arguments: std::invalid_argument.
inline std::unique_ptr<Shape> make_shape(const std::string& kind, const std::vector<double>& args) {
    (void)kind;
    (void)args;
    return nullptr;
}

// TODO: the sum of the areas. A null pointer in the vector is skipped.
inline double total_area(const Shapes& shapes) {
    (void)shapes;
    return 0;
}

// TODO: the shape with the largest area (the FIRST of equal ones); nullptr for an empty vector. Null entries are skipped.
inline const Shape* largest(const Shapes& shapes) {
    (void)shapes;
    return nullptr;
}

// TODO: a deep copy: every shape cloned (a null stays null). Allocate no more than one block per shape plus the vector's own.
inline Shapes clone_all(const Shapes& shapes) {
    (void)shapes;
    return {};
}
