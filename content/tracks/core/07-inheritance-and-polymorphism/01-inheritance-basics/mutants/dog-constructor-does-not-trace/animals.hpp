// MUTANT: no trace line.
#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "trace.hpp"

// ---- Animal: the base class ---------------------------------------------------------------------------------------------------------------
class Animal {
public:
    Animal(std::string name, int legs) : name_(std::move(name)), legs_(legs) { trace("Animal(" + name_ + ")"); }
    ~Animal() { trace("~Animal(" + name_ + ")"); }

    const std::string& name() const { return name_; }
    int legs() const { return legs_; }
    int energy() const { return energy_; }
    void eat(int food) {
        if (food > 0) energy_ += food;  // eating a negative amount is not a thing
    }
    std::string describe() const { return name_ + " has " + std::to_string(legs_) + " legs"; }

protected:
    int energy_ = 10;  // derived classes spend it; the outside world can only read it (energy()) or add to it (eat())

private:
    std::string name_;
    int legs_;
};

// ---- Dog and Cat: public inheritance means "a Dog IS an Animal" ----------------------------------------------------------------------------
class Dog : public Animal {
public:
    explicit Dog(std::string name) : Animal(std::move(name), 4) {}
    ~Dog() { trace("~Dog(" + name() + ")"); }

    std::string speak() const { return "Woof"; }
    bool fetch() {  // costs 3 energy
        if (energy_ < 3) return false;
        energy_ -= 3;
        return true;
    }
    void learn(std::string trick) { tricks_.push_back(std::move(trick)); }
    const std::vector<std::string>& tricks() const { return tricks_; }
    std::string describe() const { return Animal::describe() + " (dog)"; }  // HIDES Animal::describe; it does not override it (not virtual)

private:
    std::vector<std::string> tricks_;
};

class Cat : public Animal {
public:
    explicit Cat(std::string name) : Animal(std::move(name), 4) { trace("Cat(" + this->name() + ")"); }
    ~Cat() { trace("~Cat(" + name() + ")"); }

    std::string speak() const { return "Meow"; }
    bool climb() {  // costs 2 energy
        if (energy_ < 2) return false;
        energy_ -= 2;
        return true;
    }
    std::string describe() const { return Animal::describe() + " (cat)"; }
};

// ---- Slicing ---------------------------------------------------------------------------------------------------------------------------------
// A by-REFERENCE parameter feeds the real animal. (By value, `Animal animal` would copy just the Animal part of a Dog: a "sliced" copy.)
inline void feed(Animal& animal, int food) { animal.eat(food); }

// Feeds every animal in the list (a null pointer is skipped).
inline void feed_all(const std::vector<Animal*>& animals, int food) {
    for (Animal* a : animals)
        if (a) feed(*a, food);
}

// Slicing on purpose: the result is an Animal that keeps the name, the legs and the energy of the dog, and nothing else.
inline Animal sliced_copy(const Dog& dog) { return dog; }

// ---- Private inheritance: "is implemented in terms of", not "is a" ---------------------------------------------------------------------------
// An IntStack uses a std::vector<int> inside, but it is NOT a vector: nobody can call insert, begin, operator[] or convert it to a vector.
class IntStack : private std::vector<int> {
    using Base = std::vector<int>;

public:
    using Base::empty;  // these two are exposed on purpose
    using Base::size;

    void push(int value) { Base::push_back(value); }
    bool pop() {  // false when there is nothing to remove
        if (Base::empty()) return false;
        Base::pop_back();
        return true;
    }
    int top() const {
        if (Base::empty()) throw std::out_of_range("top of an empty IntStack");
        return Base::back();
    }
};
