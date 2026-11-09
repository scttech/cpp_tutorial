# Composition over inheritance

Inheritance is the strongest coupling C++ offers: a derived class is bound to its base's *implementation*, not just its interface. Use it for a real **is-a** relationship with a stable base (`Circle` is a `Shape`). For everything else, prefer **composition**: a class **has** a part, as a member, and uses it. The saying is "favor composition over inheritance", and this lesson shows three reasons.

## 1. The fragile base class

```cpp
class IntBag {                          // from a library: you cannot change it
public:
    virtual void add(int v);
    virtual void add_all(const std::vector<int>& vs) { for (int v : vs) add(v); }   // calls the virtual add: an implementation detail
};

class CountingBag : public IntBag {
    void add(int v) override     { ++added_; IntBag::add(v); }
    void add_all(const std::vector<int>& vs) override { added_ += vs.size(); IntBag::add_all(vs); }   // BUG: counted twice
};
```

`CountingBag::add_all` counts the values, then calls `IntBag::add_all`, which calls the *overridden* `add` for each, which counts them again. It looks right and it is wrong, and whether it is wrong depends on how the base is **implemented**: if `add_all` did not call `add`, the code would be right; if the library changes `add_all` next year, it breaks again. That is a **fragile base class**: derived classes rely on details the base never promised, so changing the base can break them. Overriding virtual functions that the base calls internally is a dependency on those internals.

**Composition fixes it**: `CountingBag` has a private `IntBag bag_` and *forwards* to it (`bag_.add(v); ++added_;`). It uses only the public contract, the base's inner calls are none of its business, and it is not an `IntBag` any more (and cannot be mistaken for one, or sliced).

## 2. Behavior from parts, not from subclasses

A game has knights who walk and fight with swords, dragons who fly, archers who walk and shoot... With inheritance each combination is a class (`FlyingSwordsman`, `SwimmingArcher`...) and the number grows with every new ability, and a character can never *change* its abilities: its class is fixed when it is created.

With composition a `Character` **has a** `Movement` and **has an** `Attack`: small interface types (lesson 3) with their own implementations (`Walk`, `Fly`, `Sword`, `Bow`). Any combination works without a new class, a new part (`Swim`) plugs in without touching `Character` (**open for extension, closed for modification**), and `set_movement` can swap a part **while the program runs**. This is the *strategy* pattern. The parts are owned through `std::unique_ptr` (they are polymorphic, so they live on the heap), which makes `Character` move-only: a copy would need a virtual `clone()`.

Rules of thumb:

- **Is-a for interfaces, has-a for implementation.** Inherit to be *used as* the base (substitutable). Compose to *reuse*.
- **Deep hierarchies are a smell.** Three levels or more usually means behavior is being shared by inheritance that wants to be a part.
- **`protected` data is a smell too**: every derived class becomes coupled to the base's layout. (The Clean tier checks that this file has none.)

## 3. The diamond (awareness)

```cpp
struct Device  { int id; };
struct Scanner : Device {};
struct Printer : Device {};
struct Copier  : Scanner, Printer {};     // TWO Device subobjects: copier.id is ambiguous
```

With multiple inheritance a base reached through two paths is *duplicated*: a `Copier` contains a `Scanner::Device` and a `Printer::Device` with different ids. **Virtual inheritance** (`struct Scanner : virtual Device`) makes the shared base exist **once**. The price: a small run-time overhead, a more complicated object layout, and the rule that the **most derived class constructs the virtual base** (`Copier(int i) : Device(i), Scanner(i), Printer(i)`; the `Device(i)` that `Scanner`'s own initializer list names is ignored when a `Copier` is built). That is why the diamond is best avoided: if you can compose the shared piece instead, do. Interfaces with no data are the usual, harmless case.

## Your task

Fill in `composition.hpp` (numbered TODOs): make `CountingBag` use composition (`IntBag` is given and must not change), write `Walk`, `Fly`, `Sword`, `Bow`, the `Character` class and its two factories, and fix the diamond with virtual inheritance.

The tiers: **Works**, **Robust** (`CountingBag` really is not an `IntBag`; validation and boundaries; swapping and destroying parts; the parts of someone else plug in; one shared `Device`), **Clean** (no raw `new`/`delete`, no `protected`, `override` everywhere).
