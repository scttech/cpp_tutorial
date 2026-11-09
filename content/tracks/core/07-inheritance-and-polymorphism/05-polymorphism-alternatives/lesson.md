# Runtime polymorphism vs alternatives

Virtual functions are one way to let *one* piece of code work with *many* types. They are the right one when the set of types is **open** (plugins, user-defined subclasses) and decided at **run time**. They cost: a pointer per object (the **vptr**), an indirect call that cannot be inlined, and usually a **heap allocation per object**, because polymorphic objects live behind pointers. This lesson builds the same shapes three more ways and measures what each one costs.

| | virtual functions | `std::variant` | CRTP | type erasure |
|---|---|---|---|---|
| the set of types | **open**: anyone can derive | **closed**: listed in the variant | open, but one type per container | **open**: any type with the right function |
| decided | run time | run time | **compile time** | run time |
| objects live | behind pointers (heap) | **inside** the container | inside the container | behind a pointer, hidden |
| common base class needed | yes | **no** | a template base | **no** |
| adding an operation | edit every class | **one new visitor** | edit every class | edit the wrapper |
| adding a type | **one new class** | edit every visitor (checked!) | one new class | nothing |

(That trade is called the *expression problem*: you can make adding types easy or adding operations easy, not both. Pick the one that changes more often.)

## 1. Virtual functions: the baseline (given)

One abstract base class, one class per shape, one heap object per shape: what the last three lessons did.

## 2. `std::variant`: a closed set, no pointers

```cpp
struct Circle { double radius; };   struct Rect { double width, height; };   struct Tri { double base, height; };
using Shape = std::variant<Circle, Rect, Tri>;            // holds exactly one of them, INSIDE the variant

double area_of(const Shape& s) {
    return std::visit([](const auto& shape) { return area(shape); }, s);   // a generic lambda: the same code for each alternative
}
std::string name_of(const Shape& s) {
    return std::visit(Overloaded{ [](const Circle&)   { return std::string("circle"); },     // one lambda per alternative
                                  [](const Rect&)     { return std::string("rect"); },
                                  [](const Tri&)      { return std::string("triangle"); } }, s);
}
```

The shapes are plain data (no base class, no vptr), and a `std::vector<Shape>` stores them **contiguously** in one allocation: fast to build, fast to iterate (cache-friendly). `std::visit` with an `Overloaded` visitor **must handle every alternative**: add a `Triangle`-like fourth shape to the variant and every incomplete visitor *fails to compile*. That is the closed set's strength (the compiler finds every place to update) and its limit (a plugin cannot add a shape). `Overloaded` is the standard helper `template <class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };`.

## 3. CRTP: polymorphism resolved at compile time

```cpp
template <class Derived>
class StaticShape {                                 // a base class that KNOWS its derived class
public:
    double area() const { return static_cast<const Derived&>(*this).area_impl(); }   // no virtual: a compile-time call
    double scaled_area(double f) const { return area() * f * f; }                  // behavior written ONCE, for every shape
};
class SCircle : public StaticShape<SCircle> { public: double area_impl() const { ... } };   // "curiously recurring": it derives from a base parameterized by itself
```

The **Curiously Recurring Template Pattern**: `Derived` derives from `Base<Derived>`. The base calls into the derived class with a `static_cast`, which the compiler resolves and *inlines*: there is **no vptr** (`sizeof(SCircle) == sizeof(double)`) and no indirect call. It is a way to give many classes shared behavior, or a common interface, at no run-time cost. The limits: `StaticShape<SCircle>` and `StaticShape<SRect>` are *different* types with no common base, so a container holds **one** kind of shape, and the code that uses them must be a template. Make the base constructor private with `friend Derived;`, so that the classic mistake `class SRect : StaticShape<SCircle>` does not compile.

## 4. Type erasure: value semantics with run-time flexibility (guided)

`std::function` and `std::any` are type-erased. The idea: give a class a *template constructor* that accepts **any** type with the needed operation, and hide a tiny virtual interface inside:

```cpp
class AnyShape {                                         // the USER sees: a value you can copy, with area()
    struct Concept { virtual ~Concept() = default; virtual double area() const = 0; virtual std::unique_ptr<Concept> clone() const = 0; };
    template <class T> struct Model final : Concept {    // knows the real T; implements the Concept for it
        T value;
        double area() const override { return detail::area_via_adl(value); }
        std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }
    };
    std::unique_ptr<Concept> impl_;
public:
    template <class T> requires (!std::same_as<std::remove_cvref_t<T>, AnyShape>) AnyShape(T&& v);   // wraps anything
    AnyShape(const AnyShape&);                           // copies the HELD value, through clone()
    double area() const;
};
```

`Circle`, `Rect`, a `Square` that someone else wrote years ago: none needs a base class or a `virtual`. They only need an `area(const T&)` that can be called (found by **argument-dependent lookup**, which is why it goes through the free helper: inside `Model::area` the unqualified name `area` would find the *member* and stop looking). You get value semantics (copy, assign, store in a `std::vector`) *and* different types behind one interface. The costs are what virtual functions cost: one heap allocation per held object, and an indirect call. The `requires` clause matters: without it the forwarding-reference constructor beats the copy constructor for a non-const `AnyShape` lvalue and wraps an `AnyShape` in an `AnyShape`.

## Which one?

- **Open set, decided at run time, objects of many types in one container** (plugins, GUI widgets): virtual functions, or type erasure if you want values instead of pointers.
- **Closed set known up front, mostly adding new operations**: `std::variant`. Often the fastest and simplest.
- **Behavior known at compile time, performance matters, one type per use**: templates / CRTP / concepts (module 5). *Static polymorphism wins when the compiler can see the real type*: it can inline, and there is no vptr.
- A virtual call costs little *per call*; it adds up as one allocation and one cache miss per object when you have a million of them. Measure first, then choose.

## Your task

Part 1 (virtual functions) is given as the baseline. Fill in parts 2, 3 and 4 in `three_ways.hpp` (the numbered TODOs): the variant functions, the CRTP base with two shapes, and the type-erased `AnyShape`. Every shape collection is built by the same rule (written at the top of the file), so the four ways can be compared.

The tiers: **Works** (every way gives the same answers), **Robust** (value semantics of `AnyShape`, forwarding without copies, no vptr, the CRTP safety check), **Fast** (allocation counts: *one* for a thousand variants or CRTP shapes, a thousand and one for virtual or erased ones; sizes; nothing allocates while summing), **Clean** (no raw `new`, no `dynamic_cast`, `std::visit` rather than `index()` chains).
