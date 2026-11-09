# Fast path: what's different in C++

- **Inheritance**: `class Square : public Rectangle` means a Square *is a* Rectangle: it can be used wherever a `Rectangle` is expected. Members are inherited; the derived constructor must start the base: `Square(double s) : Rectangle(s, s) {}`.
- **`virtual`** makes a call go to the **real** type of the object at run time (dynamic dispatch), through a pointer or reference to the base. `= 0` makes a function **pure virtual**: the class is **abstract** (cannot be instantiated) and derived classes must implement it. Always write **`override`** on the derived side: the compiler then rejects a function that does not really override anything. `final` forbids further overriding or deriving.
- **A base class with virtual functions needs a `virtual` destructor.** `delete base_ptr` (and so `unique_ptr<Base>`) on an object of a derived class is undefined behavior when `~Base` is not virtual: the derived destructor does not run. `virtual ~Shape() = default;` is the whole fix.
- **Polymorphic objects live behind pointers**: `std::vector<std::unique_ptr<Shape>>`. A `std::vector<Shape>` would *slice* (and an abstract class cannot even be a value). The owner of the vector owns the shapes.
- **The clone idiom**: copying through a base pointer needs a virtual function, `virtual std::unique_ptr<Shape> clone() const = 0;`, because the copy constructor is chosen by the *static* type. Every concrete class overrides it: `return std::make_unique<Square>(*this);`. A subclass that forgets copies as its base class.
- **Validate in the constructor**: an object that cannot be built wrongly never needs checking again. Throw `std::invalid_argument` for an argument that makes no sense.
- `if (!(x >= 0))` also rejects NaN, `if (x < 0)` does not.
