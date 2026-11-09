#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Composition over inheritance. Three small parts, each with its own TODOs. The Clean tier reads this file: composition needs no `protected`.

// ==== Part A: the fragile base class ==================================================================================================================
// IntBag is a class from a library: DO NOT CHANGE IT. add_all happens to call add (an implementation detail it never promised).
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

// TODO 1: this CountingBag counts every value of add_all TWICE (add_all adds `values.size()`, then IntBag::add_all calls the overridden add for
//         each value). You cannot fix that from here without knowing how IntBag works inside: the base class is FRAGILE. Do not derive from
//         IntBag. Make it a class with a private IntBag MEMBER and forward add, add_all, size and sum to it; count the values yourself.
class CountingBag : public IntBag {
public:
    void add(int value) override {
        ++added_;
        IntBag::add(value);
    }
    void add_all(const std::vector<int>& values) override {
        added_ += values.size();
        IntBag::add_all(values);
    }
    std::size_t added() const { return added_; }

private:
    std::size_t added_ = 0;
};

// ==== Part B: behavior from parts instead of from subclasses ==========================================================================================
// A deep hierarchy (Creature > Walker > Fighter > Knight, then flying knights, swimming archers ...) multiplies with every new ability. Instead a
// Character HAS a Movement and an Attack: small interchangeable parts that can be swapped while the program runs.
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

// TODO 2: Walk: speed 1, name "walk". Fly: speed 3, name "fly". Sword: damage 5 + 2 * level, name "sword". Bow: damage 3 + level, name "bow".
//         All four are leaf classes (final) and mark their overrides with `override`.
class Walk final : public Movement {
public:
    int speed() const override { return 0; }
    std::string name() const override { return ""; }
};
class Fly final : public Movement {
public:
    int speed() const override { return 0; }
    std::string name() const override { return ""; }
};
class Sword final : public Attack {
public:
    int damage(int level) const override {
        (void)level;
        return 0;
    }
    std::string name() const override { return ""; }
};
class Bow final : public Attack {
public:
    int damage(int level) const override {
        (void)level;
        return 0;
    }
    std::string name() const override { return ""; }
};

// TODO 3: Character HAS a Movement and an Attack (owned through unique_ptr). It does NOT derive from anything.
//   - The constructor throws std::invalid_argument for hp <= 0 or a null movement or attack.
//   - alive() is hp > 0; take_hit(damage) lowers hp but never below 0, and ignores a negative damage.
//   - distance(ticks) is speed * ticks (0 for negative ticks); strike(level) is the attack's damage, but 0 for a dead character.
//   - set_movement(m) swaps the movement while the program runs (a null one is std::invalid_argument).
//   - describe() is "<name> (<hp> hp, <movement name> / <attack name>)", for example "Orc (10 hp, walk / sword)".
class Character {
public:
    Character(std::string name, int hp, std::unique_ptr<Movement> movement, std::unique_ptr<Attack> attack) {
        (void)name;
        (void)hp;
        (void)movement;
        (void)attack;
    }

    const std::string& name() const { return name_; }
    int hp() const { return 0; }
    bool alive() const { return false; }
    void take_hit(int damage) { (void)damage; }
    int distance(int ticks) const {
        (void)ticks;
        return 0;
    }
    int strike(int level) const {
        (void)level;
        return 0;
    }
    void set_movement(std::unique_ptr<Movement> movement) { (void)movement; }
    std::string describe() const { return ""; }

private:
    std::string name_;
};

// TODO 4: a knight: 30 hp, Walk and Sword. A dragon: 100 hp, Fly and Sword.
inline Character make_knight(std::string name) { return Character(std::move(name), 1, std::make_unique<Walk>(), std::make_unique<Sword>()); }
inline Character make_dragon(std::string name) { return Character(std::move(name), 1, std::make_unique<Walk>(), std::make_unique<Sword>()); }

// ==== Part C: the diamond (awareness) =================================================================================================================
// Scanner and Printer are both a Device; Copier is both. With ordinary inheritance a Copier contains TWO Devices (two ids, and `copier.id` would be
// ambiguous). TODO 5: make the shared base exist ONCE with VIRTUAL inheritance (`: virtual Device`), in BOTH Scanner and Printer.
//   A virtual base is constructed by the MOST DERIVED class, so Copier's initializer list must name it: `Copier(int i) : Device(i), Scanner(i), Printer(i)`.
struct Device {
    static inline int constructed = 0;  // how many Device objects were built (a test reads it)
    int id;
    explicit Device(int i) : id(i) { ++constructed; }
};

struct Scanner : Device {
    explicit Scanner(int i) : Device(i) {}
    std::string scan() const { return "scan:" + std::to_string(id); }
};

struct Printer : Device {
    explicit Printer(int i) : Device(i) {}
    std::string print() const { return "print:" + std::to_string(id); }
};

struct Copier : Scanner, Printer {
    explicit Copier(int i) : Scanner(i), Printer(i) {}
    std::string copy() const { return scan() + "+" + print(); }
};
