# Fast path: what's different in C++

- **`class Dog : public Animal`**: a `Dog` contains an `Animal` subobject. No `extends`, no implicit "everything is virtual" like Java: C++ inheritance is by value, and what is *dispatched* is opt-in (`virtual`, next lesson). Multiple inheritance exists (`class A : public B, public C`).
- **Construction order**: bases (in the order they are listed), then members (in declaration order), then the constructor body. **Destruction**: the exact reverse. Start the base in the **member-initializer list**: `Dog(std::string n) : Animal(std::move(n), 4) {}`.
- **Access**: `public` / `protected` / `private` members; and the base-class specifier (`: public Animal`) caps the access of the inherited members. **`class` defaults to private inheritance**: always write `public` when you mean "is-a".
- **Private inheritance** = "implemented in terms of" (no implicit conversion to the base, the base's interface hidden). Re-expose pieces with `using Base::name;`. A member is the usual alternative.
- **Hiding**: a name declared in the derived class hides *all* base members of that name. Reach the base's with `Base::name()`, or bring them back with `using Base::name;`. Hiding is **not** overriding: only `virtual` makes a call follow the real object.
- **Object slicing**: `Animal a = dog;` or a by-value `Animal` parameter copies only the base part. Use references, pointers or smart pointers for polymorphic objects.
- **An up-cast is implicit** (`Animal* p = &dog;`), a down-cast is not: `static_cast` (unchecked, you promise) or `dynamic_cast` (checked, needs a virtual function; lesson 2).
- A base that is meant to be used through pointers also needs a **virtual destructor**: the next lesson's first rule.
