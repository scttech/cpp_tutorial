# shared_ptr and weak_ptr

`unique_ptr` is the default: one owner, decided at the point of creation. But sometimes ownership really **is** shared: several
parts of a program need an object to stay alive, and none of them knows who will finish last. A cache entry used by three
windows. A node held by the tree *and* by the code that is editing it. For that there is `std::shared_ptr<T>`.

## `shared_ptr`: shared ownership by counting

A `shared_ptr` keeps a **reference count** next to the object. Copying a `shared_ptr` adds one owner; destroying one removes it; when
the count reaches zero, the object is deleted.

```cpp
auto a = std::make_shared<Node>("a");   // count 1  (make_shared allocates the object and the count together)
auto b = a;                             // count 2: copying is allowed, and cheap
a.reset();                              // count 1: `a` no longer owns it
b.use_count();                          // 1
// b goes out of scope: count 0, the Node is deleted
```

It is **not free** (an atomic counter on every copy) and it makes lifetime harder to reason about ("who frees this, and when?").
Prefer `unique_ptr`; reach for `shared_ptr` when sharing is the actual design. Always create with `std::make_shared`.

## The cycle: how `shared_ptr` leaks

Reference counting has one famous hole. A tree where a parent owns its children **and** each child owns its parent:

```
 root ──shared──► child
   ▲                │
   └────shared──────┘      root's count never drops to 0 (child holds it), child's never drops to 0 (root holds it)
```

When the last outside pointer goes away, each node still has a count of 1 *from the other one*. Nothing is ever freed. This is not
exotic: parent/child links, observers, doubly-linked lists and graphs all do it. The starter has exactly this bug: press Run, and LeakSanitizer reports the
nodes, with the line that allocated them.

## `weak_ptr`: observing without owning

A `std::weak_ptr<T>` points at an object managed by `shared_ptr`s **without being one of its owners**: it does not change the count and does not keep
the object alive. To use it you ask for a temporary `shared_ptr`:

```cpp
std::weak_ptr<Node> parent_;
...
if (std::shared_ptr<Node> p = parent_.lock()) {   // lock(): a shared_ptr if the object still exists, an empty one if it is gone
    use(*p);                                      // while p is alive the object cannot disappear under you
}
```

This is the cure for cycles: **decide who owns whom**, and make the back-link a `weak_ptr`. Here a parent **owns** its children
(`shared_ptr`), and a child only **observes** its parent (`weak_ptr`). Then the root is freed as soon as nobody holds it, taking the
tree with it; a child you still hold has an *expired* parent, which `parent()` reports as null instead of a dangling pointer.

## Aliasing, again

`add_child(a, b->children()[0])` passes a *reference to an element of `b`'s vector*. `add_child` moves that node out of that very
vector (`erase`), and after the erase the reference points at a dead element. Any function that mutates a container its own
arguments live in must **copy the `shared_ptr`s into locals first** (a copy keeps the object alive, too). It is the same trap as `a.append(a.c_str())`,
and the sanitizer reports it as a heap-use-after-free.

## Your task

Complete `node.hpp`. The class is a tree of named nodes. Parent/child links must not form an ownership cycle.

| Member | Behavior |
|---|---|
| `parent()` | The parent, or null for a root or when the parent no longer exists. |
| `add_child(parent, child)` | Makes `child` the **last** child of `parent`. A node that already has a parent is **moved** (it leaves its old parent first; re-adding to the same parent moves it to the end). `std::invalid_argument` for a null node, and for a link that would put a node under itself or one of its own descendants. |
| `detach(node)` | Removes `node` from its parent's children; it becomes a root. A root or null is left alone. |
| `depth()`, `path()` | Number of ancestors; names from the root down, joined with `/`. |
| `subtree_size()` | The node and everything below it. |
| `find(root, name)` | The first node named `name`: `root` itself, then depth-first. Null if there is none. |

The tests check the **use counts** (the parent owns the child, the child does not own the parent), that nothing leaks (no cycle),
that a child outlives its parent safely, the aliasing cases above, and (Clean tier) that `node.hpp` has no `new` or `delete`.
