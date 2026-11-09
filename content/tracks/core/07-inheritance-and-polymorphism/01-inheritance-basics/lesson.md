# Inheritance basics

**Inheritance** lets a class build on another: `Dog` *is an* `Animal`, with everything an animal has plus what makes a dog a dog. Used well it models a real "is-a" relationship; used carelessly it is the most overused feature of C++. This lesson covers the mechanics, and the three things that surprise people: the **order** things are built in, **access** specifiers, and **slicing**.

## A derived class contains its base

```cpp
class Animal {
public:
    Animal(std::string name, int legs);
    const std::string& name() const;
protected:
    int energy_ = 10;        // visible to derived classes, hidden from everyone else
private:
    std::string name_;       // visible to Animal only
};

class Dog : public Animal {  // "public": a Dog IS an Animal, from outside too
public:
    explicit Dog(std::string name) : Animal(std::move(name), 4) {}   // the base is started FIRST, here
    bool fetch() { if (energy_ < 3) return false; energy_ -= 3; return true; }
};
```

A `Dog` object **contains an `Animal` subobject**, followed by the `Dog` members. That is why:

- The **base constructor runs first** (it must: the `Dog` part uses the `Animal` part), then the derived constructor body. **Destruction is the reverse**: `~Dog` first, then `~Animal`. The base is started in the **member-initializer list**; without one, the base's *default* constructor is used (a compile error if it has none).
- `Dog` can use `Animal`'s `public` and `protected` members, not its `private` ones: even a derived class goes through `name()`.
- A `Dog*` converts to an `Animal*` (and a `Dog&` to an `Animal&`): an *up-cast*, implicit and safe. The other way needs a cast and a promise.

### Access: public, protected, private

| | the class | derived classes | everyone else |
|---|---|---|---|
| `public` | ✔ | ✔ | ✔ |
| `protected` | ✔ | ✔ | ✘ |
| `private` | ✔ | ✘ | ✘ |

The **kind of inheritance** is an access specifier too, and it caps what the base's members are outside: `class Dog : public Animal` keeps them, `: protected` makes public members protected, `: private` makes all of them private. **`class` defaults to private inheritance** (`struct` to public): forgetting `public` makes `Dog` *not convertible* to `Animal`.

Private inheritance means **"is implemented in terms of"**, not "is a". `IntStack : private std::vector<int>` uses a vector inside without *being* one: nobody can call `insert` or `begin` on it, and a `std::vector<int>*` cannot be made from it. A `using Base::size;` declaration re-exposes just the members you choose. (A *member* `std::vector<int> items_;` does the same job and is usually the better choice: lesson 4 comes back to it.)

## Name hiding is not overriding

```cpp
class Dog : public Animal {
public:
    std::string describe() const { return Animal::describe() + " (dog)"; }   // calls the BASE version by its qualified name
};
Dog d("Rex");
const Animal& a = d;
a.describe();   // the ANIMAL version: describe is not virtual, so the STATIC type (Animal&) decides
d.describe();   // the Dog version
```

`Dog::describe` **hides** `Animal::describe` (and every overload of that name): it does not *replace* it. Which function a call reaches is decided by the type of the expression, at compile time. Making `Animal::describe` **virtual** changes that, and that is the next lesson.

## Slicing

```cpp
void feed(Animal animal, int food) { animal.eat(food); }   // BUG: animal is a COPY of the argument
Dog rex("Rex");
feed(rex, 5);        // copies only the Animal part of rex into the parameter; rex itself is unchanged
Animal a = rex;      // the same: `a` is an Animal, the Dog part is "sliced off"
```

Copying a derived object into a *base-type variable* copies just the base part: the derived members are gone, and (as you will see) so is the derived **behavior**. It is legal, it compiles without a warning, and it is almost never what you meant. The cure is simple: **pass and keep polymorphic objects by reference or pointer** (`Animal&`, `const Animal&`, `Animal*`, `std::unique_ptr<Animal>`), never by value.

## Your task

Fill in `animals.hpp` (every TODO is a numbered step): `Animal`, `Dog` and `Cat`, the by-reference `feed` and `feed_all`, a deliberate slice (`sliced_copy`) and the privately-inheriting `IntStack`. `main.cpp` (read-only) prints what they do, including the constructor/destructor trace.

The tiers: **Works** (the classes behave), **Robust** (construction and destruction **order**, `energy_` really protected, boundary values, no slicing in `feed`, and `IntStack` really private).
