// MUTANT: no check.
#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// ==== Part A: the fragile base class ==================================================================================================================
// IntBag is a class from a library: you cannot change it. add_all happens to call add (an implementation detail it never promised).
class IntBag {
public:
    virtual ~IntBag() = default;
    virtual void add(int value) { items_.push_back(value); }
    virtual void add_all(const std::vector<int>& values) {
        for (int v : values) add(v);
    }
    std::size_t size() const { return items_.size(); }
    long long sum() const {
        long long total = 0;
        for (int v : items_) total += v;
        return total;
    }

private:
    std::vector<int> items_;
};

// A bag that also counts how many values were added. By INHERITANCE (override add and add_all) every value of add_all is counted twice, because the
// base's add_all calls the overridden add. By COMPOSITION the bag is a private member, and its call structure is none of our business.
class CountingBag {
public:
    void add(int value) {
        bag_.add(value);
        ++added_;
    }
    void add_all(const std::vector<int>& values) {
        bag_.add_all(values);
        added_ += values.size();
    }
    std::size_t size() const { return bag_.size(); }
    long long sum() const { return bag_.sum(); }
    std::size_t added() const { return added_; }

private:
    IntBag bag_;
    std::size_t added_ = 0;
};

// ==== Part B: behavior from parts instead of from subclasses ==========================================================================================
// A deep hierarchy (Creature > Walker > Fighter > Knight, and then Flying knights, Archers that swim ...) multiplies with every new ability.
// Instead a Character HAS a Movement and an Attack, small interchangeable parts, and they can be swapped while the program runs.
class Movement {
public:
    virtual ~Movement() = default;
    virtual int speed() const = 0;  // distance per tick
    virtual std::string name() const = 0;
};

class Attack {
public:
    virtual ~Attack() = default;
    virtual int damage(int level) const = 0;
    virtual std::string name() const = 0;
};

class Walk final : public Movement {
public:
    int speed() const override { return 1; }
    std::string name() const override { return "walk"; }
};

class Fly final : public Movement {
public:
    int speed() const override { return 3; }
    std::string name() const override { return "fly"; }
};

class Sword final : public Attack {
public:
    int damage(int level) const override { return 5 + 2 * level; }
    std::string name() const override { return "sword"; }
};

class Bow final : public Attack {
public:
    int damage(int level) const override { return 3 + level; }
    std::string name() const override { return "bow"; }
};

class Character {
public:
    // hp must be positive and both parts must exist: std::invalid_argument otherwise.
    Character(std::string name, int hp, std::unique_ptr<Movement> movement, std::unique_ptr<Attack> attack)
        : name_(std::move(name)), hp_(hp), movement_(std::move(movement)), attack_(std::move(attack)) {
        if (hp <= 0) throw std::invalid_argument("hp must be positive");
        if (!movement_ || !attack_) throw std::invalid_argument("a character needs a movement and an attack");
    }

    const std::string& name() const { return name_; }
    int hp() const { return hp_; }
    bool alive() const { return hp_ > 0; }
    void take_hit(int damage) {
        hp_ = std::max(0, hp_ - damage);
    }
    int distance(int ticks) const { return ticks > 0 ? movement_->speed() * ticks : 0; }
    int strike(int level) const { return alive() ? attack_->damage(level) : 0; }  // the dead do not fight

    void set_movement(std::unique_ptr<Movement> movement) {
        if (!movement) throw std::invalid_argument("a character needs a movement");
        movement_ = std::move(movement);
    }
    std::string describe() const { return name_ + " (" + std::to_string(hp_) + " hp, " + movement_->name() + " / " + attack_->name() + ")"; }

private:
    std::string name_;
    int hp_;
    std::unique_ptr<Movement> movement_;
    std::unique_ptr<Attack> attack_;
};

inline Character make_knight(std::string name) { return Character(std::move(name), 30, std::make_unique<Walk>(), std::make_unique<Sword>()); }
inline Character make_dragon(std::string name) { return Character(std::move(name), 100, std::make_unique<Fly>(), std::make_unique<Sword>()); }

// ==== Part C: the diamond (awareness) =================================================================================================================
// Scanner and Printer are both a Device; Copier is both. With ordinary inheritance a Copier contains TWO Devices (two ids, and `copier.id` is ambiguous).
// With VIRTUAL inheritance the shared base exists once, and it is constructed by the MOST DERIVED class.
struct Device {
    static inline int constructed = 0;  // how many Device objects were built (a test reads it)
    int id;
    explicit Device(int i) : id(i) { ++constructed; }
};

struct Scanner : virtual Device {
    explicit Scanner(int i) : Device(i) {}
    std::string scan() const { return "scan:" + std::to_string(id); }
};

struct Printer : virtual Device {
    explicit Printer(int i) : Device(i) {}
    std::string print() const { return "print:" + std::to_string(id); }
};

struct Copier : Scanner, Printer {
    explicit Copier(int i) : Device(i), Scanner(i), Printer(i) {}
    std::string copy() const { return scan() + "+" + print(); }
};
