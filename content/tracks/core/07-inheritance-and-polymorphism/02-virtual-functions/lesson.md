# Virtual functions and dynamic dispatch

Last lesson ended with a puzzle: `Dog::describe` did not run when you called `describe()` through an `Animal&`. A **virtual function** fixes that: the call goes to the function of the object's **real** class, decided at **run time**. That is *polymorphism*: one interface, many behaviors.

## `virtual`, `override`, `final`

```cpp
class Employee {
public:
    virtual ~Employee() = default;                  // (1) the destructor of a polymorphic base is virtual
    virtual double monthly_pay() const { return 0; }
    virtual std::string title() const { return "employee"; }
};

class Manager final : public Salaried {             // `final` class: nothing can derive from it
public:
    double monthly_pay() const override { return Salaried::monthly_pay() + 250 * reports_; }   // (2) override
    std::string title() const override { return "manager"; }
};

void show(const Employee& e) { std::cout << e.title() << ": " << e.monthly_pay(); }   // which title()? Decided when it RUNS.
```

- **`virtual`** in the base declares a customization point. A derived class function with the **same name, parameters and `const`ness** overrides it, and stays virtual without repeating the keyword.
- **`override`** asks the compiler to check that the function really overrides one. **Always write it.** Without it, `double monthly_pay()` (no `const`) is a *different* function that silently **hides** the base one: the code compiles, and a call through `Employee&` quietly does the wrong thing. `final` on a function stops further overriding, on a class stops deriving.
- A derived function can **extend** the base behavior by calling it by its qualified name: `Salaried::monthly_pay()`.
- Under the hood each polymorphic object carries a hidden pointer to a table of function pointers (the **vtable**) of its real class; a virtual call is one extra indirection. That costs a little per call, and it blocks inlining; it is rarely your bottleneck, and lesson 5 measures it.

### (1) The virtual destructor rule

If you `delete` a derived object through a **base pointer** (which is exactly what `std::unique_ptr<Employee>` does) and the base destructor is **not** virtual, the behavior is **undefined**: in practice only `~Employee` runs, and whatever `Manager` owns **leaks**. **Any class meant to be used polymorphically gets a virtual destructor**, even an empty one. A class with no virtual functions and no public derived classes does not need one.

### Virtual calls in constructors and destructors

While the `Employee` part of a `Manager` is being constructed, the object's dynamic type **is `Employee`**: the `Manager` part does not exist yet. A virtual call inside a base constructor therefore reaches the **base** version, *not* the override (the same is true in the destructor, the other way around: the derived part is already gone). That is why caching `title()` in `Employee`'s constructor gives every manager the title "employee". Compute such things on demand, or pass the value to the constructor.

## RTTI: asking what an object really is

When the type system has forgotten (you hold an `Employee&`), **run-time type information** can recover it. It only works on **polymorphic** types (those with a virtual function).

```cpp
const Manager* m = dynamic_cast<const Manager*>(&e);   // a pointer cast: nullptr if e is not a Manager
Manager& r       = dynamic_cast<Manager&>(e);          // a reference cast: throws std::bad_cast, there is no "null reference"
typeid(a) == typeid(b)                                 // the EXACT dynamic types are equal (a Manager is not the same type as a Salaried)
```

- `dynamic_cast` also sees *through* inheritance: a cast to `Salaried*` succeeds for a `Manager`; `typeid` does not.
- It is a **checked down-cast**: the compiler cannot prove that the `Employee` is a `Manager`, so the program checks, at a cost. `static_cast` would skip the check (and be undefined behavior if you were wrong).
- **A `dynamic_cast` chain (`if Manager ... else if Hourly ...`) is usually a missing virtual function.** Ask first: could this be a virtual `monthly_pay()` instead? Casts are for the rare operation that truly belongs to one subtype only (here: `add_report`).

## Your task

Fill in `payroll.hpp` (the TODOs are numbered): the virtual interface, `Salaried`, `Hourly` (overtime), `Manager`, the functions that work on a `std::vector<std::unique_ptr<Employee>>`, and the four RTTI helpers.

The tiers: **Works** (the classes and functions behave, *through base references*), **Robust** (the **destructor trace** of every class, a `badge()` that is correct for a Manager, `final`, boundaries and invalid input), **Clean** (`override` on every overriding function and destructor, and no `virtual` repeated in the derived classes).
