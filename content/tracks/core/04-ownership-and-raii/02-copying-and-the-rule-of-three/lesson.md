# Copying and the rule of three

A class that owns a resource through a raw pointer has a problem the last lesson hid: **what does it mean to copy it?**

```cpp
class Text {
    char* data_;                 // owned: allocated by the constructor, freed by the destructor
    ...
};

Text a("hello");
Text b = a;                      // what happens to data_?
```

If you write nothing, the compiler writes a copy constructor for you that copies every member. For a pointer member that means **the
pointer**, not what it points to. This is a **shallow copy**:

```
 a.data_ ──┐
           ├──►  "hello"       two owners of ONE buffer
 b.data_ ──┘
```

Now `b.at(0) = 'J'` changes `a` too, and when both objects are destroyed the destructor runs `delete[] data_` **twice on the same
buffer**: a **double free**, undefined behavior (in practice a crash or silent heap corruption). Press Run on the starter to see the
sanitizer's report of exactly that.

## The rule of three

> If a class needs a **destructor**, a **copy constructor** or a **copy assignment operator**, it almost certainly needs **all
> three**.

They are one decision, "how does this class own its resource", written three times:

| Function | Job | Called for |
|---|---|---|
| destructor `~T()` | release the resource | the end of the object's life |
| copy constructor `T(const T&)` | make a NEW object that owns its own copy | `T b = a;` `T b(a);` passing/returning by value |
| copy assignment `T& operator=(const T&)` | make an EXISTING object become a copy | `b = a;` |

### The copy constructor

Allocate a new buffer and copy the contents (a **deep copy**):

```cpp
Text(const Text& other) : size_(other.size_), data_(new char[other.size_ + 1]) {
    std::memcpy(data_, other.data_, size_ + 1);       // +1: the terminating '\0'
}
```

### The copy assignment operator

Harder, because the target already owns a buffer, and because of two traps:

```cpp
Text& operator=(const Text& other) {
    if (this == &other) return *this;                 // trap 1: self-assignment
    char* copy = new char[other.size_ + 1];           // build the copy FIRST ...
    std::memcpy(copy, other.data_, other.size_ + 1);
    delete[] data_;                                   // ... then release the old buffer ...
    data_ = copy;                                     // ... and take the new one
    size_ = other.size_;
    return *this;                                     // by reference: so `a = b = c` works
}
```

- **Self-assignment** (`a = a`, or `a = v[i]` where `v[i]` *is* `a`) is legal and you cannot rule it out. A version that begins with
  `delete[] data_;` frees the very buffer it is about to copy from: a use-after-free.
- **Allocate before you release.** If `new` throws (out of memory), the object must still be valid and unchanged. Releasing the old
  buffer first would leave it pointing at freed memory. (This is the **strong exception guarantee**.)
- **Return `*this` by reference**, like the built-in `=`. Returning by value makes an extra copy and breaks `(a = b).append("!")`.

### Aliasing in other functions

The same trap hides in `append`: `a.append(a.c_str())` hands the function a pointer **into the buffer it is about to replace**.
Build the new buffer, copy from the old one *and* from `s`, and only then free the old buffer.

## Your task

Complete `text.hpp`. `Text` already has its constructors and destructor.

| Member | Behavior |
|---|---|
| copy constructor | A deep copy: its own buffer, same characters, same size, the final `'\0'` included. |
| copy assignment | Safe for self-assignment, releases the old buffer, builds the copy first, returns `Text&`. |
| `append(const char*)` | Adds the characters at the end; `nullptr` and `""` change nothing; works when `s` points into this very `Text`. |

The tests copy and assign in every combination (empty, long, into itself), put `Text`s in a `std::vector` (which copies them as it
grows), and run with the sanitizers and allocation tracking on, so a leak, a double free or a use-after-free is a precise report.
Note: the next lesson shows that copying is often *not what you want* (it can be expensive): C++ also lets you **move**.
