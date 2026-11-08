# Fast path: what's different in C++

- A constructor must leave the object **valid**; throw if it cannot. Initialize members in the **member initializer list** (`: cents_(cents)`), not by assignment in the body (`const`/reference members need it). Members are initialized in **declaration order**, whatever order you write the list in (`-Wreorder`).
- An unlisted member is **uninitialized garbage** for built-in types: use default member initializers (`long long cents_ = 0;`) and `Foo() = default;`.
- **Delegating constructors** (`Foo(a, b) : Foo(helper(a, b)) {}`) avoid duplicated code; validate by calling a `static` helper in the argument.
- Mark single-argument constructors **`explicit`** unless the conversion is genuinely natural, so `charge(500)` does not compile by accident.
- Check signed overflow *before* computing (it is UB); never negate the minimum value: use an unsigned magnitude.
