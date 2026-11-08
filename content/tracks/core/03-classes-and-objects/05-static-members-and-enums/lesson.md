# Static members and enums

Two tools for modelling things with a fixed set of states, and for data that belongs to the *class* rather than to any
one object.

## `enum class`: a closed set of named values

The old C enum has two problems:

```cpp
enum Color { Red, Green };          // plain (unscoped) enum
enum Light { Red, Amber, Green };   // error: Red and Green are already taken: the names leak into the surrounding scope
int n = Green;                      // compiles: a Color silently becomes an int
```

`enum class` fixes both:

```cpp
enum class Status : std::uint8_t { Created, Paid, Shipped };   // names live inside Status

Status s = Status::Paid;     // always qualified
int n = s;                   // error: no implicit conversion
int m = static_cast<int>(s); // fine, and visibly deliberate
```

`: std::uint8_t` fixes the **underlying type**: without it the compiler picks (usually `int`). Say it when the size or
representation matters (stored in a file, sent over a network, packed into a struct). Always prefer `enum class`.

### Switching over an enum

```cpp
switch (s) {
    case Status::Created: return "Created";
    case Status::Paid:    return "Paid";
    case Status::Shipped: return "Shipped";
}
return "Unknown";    // for a value that is not one of the enumerators (possible via static_cast)
```

**Leave out `default:`** when every enumerator has a case. Then if somebody later adds `Status::Refunded` and forgets this
function, the compiler warns (`-Wswitch`: "enumeration value 'Refunded' not handled in switch"). A `default:` branch
would swallow the new value silently: the warning you wanted is gone.

## `static` members: one per class, not one per object

A normal data member exists in **every object**. A `static` data member exists **once**, shared by all of them:

```cpp
class Order {
public:
    Order() : id_(first_id + created_++) {}     // every constructor call bumps the ONE shared counter
    static int created() { return created_; }   // a static function: no object needed
    static constexpr int first_id = 1;          // a constant shared by the class
private:
    inline static int created_ = 0;             // the one counter
    int id_;                                    // one per order
};

Order a, b;
Order::created();     // 2: you ask the class, not an object
```

- A **static member function** has no `this`: it cannot use non-static members. Call it as `Order::created()`.
- A **static data member** must be *defined* somewhere. In a header, write **`inline static`** (C++17) and you are done.
  Plain `static int created_;` declares it only: you also need `int Order::created_ = 0;` in exactly one `.cpp` file,
  or the linker reports an undefined reference.
- **`static constexpr`** is for compile-time constants that belong to the class (implicitly inline).
- A per-object counter (`int created_ = 0;` without `static`) is the classic mistake: every order sees its own zero,
  so every id is the same. (Also remember that copies of an `Order` copy the id: giving each copy its own is a job for
  the copy constructor, in Module 4.)

## A state machine

An order moves `Created -> Paid -> Shipped -> Delivered`, and may be cancelled until it ships. Writing the rules as an enum plus
**one function that knows the table** keeps them in one place: `pay()`, `ship()`, ... only ask `allowed(from, to)`.
That is far easier to get right (and to change) than `if` checks scattered over four functions.

```
Created   -> Paid | Cancelled
Paid      -> Shipped | Cancelled
Shipped   -> Delivered
Delivered, Cancelled: final
```

## Your task

Complete `order.hpp`; `main.cpp` (read-only) is a demo.

| Member | Behavior |
|---|---|
| `enum class Status : std::uint8_t` | `Created, Paid, Shipped, Delivered, Cancelled` (in this order). `to_string(Status)` is written for you. |
| `Order::first_id` | `static constexpr int`, `1`. |
| `Order()` | The order is `Created`, with the next id: `first_id`, `first_id + 1`, ... from **one counter shared by all orders**. |
| `id()`, `status()` | `const`. |
| `pay()`, `ship()`, `deliver()`, `cancel()` | Move to `Paid`, `Shipped`, `Delivered`, `Cancelled`. Return `true` if the step was allowed, otherwise return `false` and change nothing. |
| `static bool allowed(from, to)` | The transition table above. |
| `static int created()` | How many orders have ever been created. Only creating an order counts. |

The tests walk **every state with every step**, so a single wrong cell of the table shows up.
