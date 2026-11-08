# Fast path: what's different in C++

- **Namespaces** prevent name collisions. Put a library's whole interface in one namespace; put helpers in a nested `detail` namespace (convention for "not public"). Nothing at file scope in a header.
- **Unnamed namespace** (`namespace { }`) is for `.cpp` files (internal linkage). In a header each includer gets its own copy: don't.
- **Never `using namespace std;` in a header** (it leaks into every includer). In headers write `std::`.
- **ADL**: an unqualified function or operator call also searches the namespaces of its arguments' types. So an operator (`<<`, `==`, `swap`, `to_string`) must live **in the same namespace as its type** (or be a hidden `friend` inside the class), or generic code (`std::` templates, your own templates) will not find it.
- A function defined in a header must be **`inline`** (or a template / defined in a class), or the linker sees duplicate definitions.
- Namespace alias: `namespace g = geo;`.
