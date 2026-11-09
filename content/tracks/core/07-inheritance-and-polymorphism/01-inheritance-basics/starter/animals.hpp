#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "trace.hpp"

// Inheritance basics. Animal is the base class; Dog and Cat derive from it; IntStack practices private inheritance.
// Every function below is a stub that compiles but is wrong: fix the TODOs.

// ---- Animal: the base class ---------------------------------------------------------------------------------------------------------------
// TODO 1: the constructor and the destructor each append a line to the trace: "Animal(<name>)" and "~Animal(<name>)".
// TODO 2: `energy_` is public. Derived classes must be able to change it, but the outside world must not: make it protected.
//         (name_ and legs_ stay private: even Dog reads them through name() and legs().)
// TODO 3: eat(food) adds the food to the energy, but only when it is positive.
class Animal {
public:
    Animal(std::string name, int legs) : name_(std::move(name)), legs_(legs) {}

    const std::string& name() const { return name_; }
    int legs() const { return legs_; }
    int energy() const { return energy_; }
    void eat(int food) { (void)food; }
    std::string describe() const { return name_ + " has " + std::to_string(legs_) + " legs"; }

    int energy_ = 10;

private:
    std::string name_;
    int legs_;
};

// ---- Dog and Cat ---------------------------------------------------------------------------------------------------------------------------
// TODO 4: a Dog is an Animal with 4 legs: the constructor starts the base class in its member-initializer list (`: Animal(std::move(name), 4)`)
//         and then appends "Dog(<name>)" to the trace; the destructor appends "~Dog(<name>)" (it runs BEFORE ~Animal).
//         speak() is "Woof". fetch() costs 3 energy and returns true; with less than 3 energy it does nothing and returns false.
//         learn(trick) adds to tricks(). describe() calls the BASE version and adds " (dog)": "Rex has 4 legs (dog)".
class Dog : public Animal {
public:
    explicit Dog(std::string name) : Animal(std::move(name), 0) {}

    std::string speak() const { return ""; }
    bool fetch() { return false; }
    void learn(std::string trick) { (void)trick; }
    const std::vector<std::string>& tricks() const { return tricks_; }

private:
    std::vector<std::string> tricks_;
};

// TODO 5: a Cat has 4 legs too. Trace "Cat(<name>)" and "~Cat(<name>)". speak() is "Meow". climb() costs 2 energy (true, or false when
//         there is not enough). describe() is the base description plus " (cat)".
class Cat : public Animal {
public:
    explicit Cat(std::string name) : Animal(std::move(name), 0) {}

    std::string speak() const { return ""; }
    bool climb() { return false; }
};

// ---- Slicing ---------------------------------------------------------------------------------------------------------------------------------
// TODO 6: feed takes its Animal BY VALUE: it feeds a COPY (a sliced one, if the argument is a Dog). Take a reference instead.
inline void feed(Animal animal, int food) { animal.eat(food); }

// TODO 7: feed every animal in the list (a null pointer is skipped).
inline void feed_all(const std::vector<Animal*>& animals, int food) {
    (void)animals;
    (void)food;
}

// TODO 8: slicing on purpose: return an Animal that keeps the name, legs and energy of the dog (return the dog: it is copied as an Animal).
inline Animal sliced_copy(const Dog& dog) {
    (void)dog;
    return Animal("?", 0);
}

// ---- Private inheritance ---------------------------------------------------------------------------------------------------------------------
// TODO 9: an IntStack uses a std::vector<int> inside but is NOT a vector. Inherit PRIVATELY, and expose only empty() and size()
//         with using-declarations (`using Base::size;`). push adds a value; pop removes the last one (false when there is none);
//         top() is the last value (std::out_of_range when empty).
class IntStack : public std::vector<int> {
public:
    void push(int value) { (void)value; }
    bool pop() { return false; }
    int top() const { return 0; }
};
