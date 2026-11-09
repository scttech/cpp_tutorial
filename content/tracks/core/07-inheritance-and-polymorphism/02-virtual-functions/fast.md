# Fast path: what's different in C++

- **Virtual dispatch is opt-in per function.** No `virtual`, no dynamic dispatch: the call is bound at compile time by the static type (name *hiding*, last lesson). Compare Java/C# (methods virtual by default) and Python (all dynamic).
- **`override`** (always) catches signature mismatches: a missing `const`, a different parameter type, a typo. `final` stops a further override or derivation. A derived function with a matching signature overrides without `virtual`, but writing `override` is the contract.
- **Virtual destructor** in any base class that will be deleted through a base pointer. Without it: undefined behavior, in practice a leak. `virtual ~Base() = default;`. (Alternative: a *protected non-virtual* destructor forbids deleting through the base at all.)
- **Constructors and destructors are never virtual** and, while they run, the object's dynamic type is the class being constructed/destroyed: a virtual call there reaches *that* class's version, not the final override. Never rely on it.
- **Slicing** (last lesson) is what the "polymorphic objects live behind pointers/references" rule is for. `std::vector<std::unique_ptr<Base>>` is the standard owner of a collection of polymorphic objects.
- **RTTI**: `dynamic_cast<T*>(p)` (null on failure) / `dynamic_cast<T&>(r)` (throws `std::bad_cast`), and `typeid(x)` with `std::type_info` (`==`, `name()` is implementation-defined and mangled). Needs a polymorphic type. Some builds disable it (`-fno-rtti`, common in games and embedded): another reason to prefer a virtual function.
- **Cost**: a virtual call is an indirect call through the vtable pointer stored in each object (+8 bytes per object); it defeats inlining unless the compiler can devirtualize (e.g. a `final` class or a known concrete type).
- **Covariant return types**: an override may return a more derived pointer/reference (`Square* clone() const override`), awareness only.
