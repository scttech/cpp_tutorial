# unique_ptr and the rule of zero

The last three lessons were about writing the special members by hand. The best news in modern C++ is that **you almost never have to**.

## `std::unique_ptr<T>`: one owner, automatic cleanup

A `unique_ptr<T>` owns one heap object. When the `unique_ptr` dies, it `delete`s the object: RAII, written once, correctly, in
the standard library. And it has exactly **one owner at a time**: it cannot be copied (that would be two owners), only **moved**.

```cpp
auto p = std::make_unique<Node>(42);   // allocate and wrap in one step; never write `new` yourself
std::unique_ptr<Node> q = p;           // error: copying is deleted
std::unique_ptr<Node> r = std::move(p);// fine: ownership moved; p is now null
if (!p) { /* empty */ }
p.reset();   // delete the owned object now (and become null)
raw = p.get();   // a NON-owning view of the pointer: do not delete it, do not keep it past the owner
p.release();     // give up ownership WITHOUT deleting (returns the raw pointer): rare, and a leak if you forget to delete it
```

`make_unique` is preferred over `unique_ptr<Node>(new Node(...))`: no `new` in your code at all, and it is exception-safe.

Use a raw pointer (or a reference) only to **look at** something someone else owns. Ownership is a decision about *who deletes*, and
`unique_ptr` makes the answer part of the type.

## The rule of zero

> Design your classes so that they need **none** of the five special members. Let members that own things (`std::vector`,
> `std::string`, `std::unique_ptr`) do the owning, and the compiler's generated destructor, copy and move do the right thing.

A `Tree` whose nodes are held by `unique_ptr`s needs no destructor (destroying `root_` destroys the whole tree), no move constructor
(moving `root_` moves the whole tree), and has a deleted copy (the members cannot be copied). That is what you want: 40 lines of
rule-of-five code, gone.

**Do not spoil it.** Declaring *any* special member suppresses the others: a user-written destructor means no implicit move
operations. Also watch for **cached state**: a member such as `size_t size_` is copied by the generated move and *not* reset, so a moved-from
`Tree` would still report its old size while owning nothing. Derive values (walk the tree to count it) instead of caching them,
or you are back to writing the special members.

## Pointer-to-pointer: editing a tree with unique_ptr

To insert or erase in a linked structure you need to modify *the pointer that points at a node*, not the node itself. With
`unique_ptr` the neat way is a pointer to the owning pointer:

```cpp
std::unique_ptr<Node>* link = &root_;                 // points at the unique_ptr that holds (or will hold) the node
while (*link && (*link)->value != value)
    link = value < (*link)->value ? &(*link)->left : &(*link)->right;
// now *link is either the node we want, or the empty slot where it belongs
*link = std::make_unique<Node>(value);                // insert: fill the empty slot
```

and erasing a node with one child is "the parent's pointer takes the child":

```cpp
*link = std::move(n.right);      // the child moves up; the old node is destroyed by this assignment
```

No raw `delete`, no dangling pointers: the old node is freed when the pointer that owned it is overwritten.

## Your task

Complete `tree.hpp`: a set of ints in a binary search tree (smaller values left, larger right). The `Node` struct and `root_` are given; add
the members. **Do not** add a destructor, a size member, or any special member.

| Member | Behavior |
|---|---|
| `insert(v)` | Adds `v`; returns `false` (no change) if it is already there. |
| `contains(v)` | `const`. |
| `erase(v)` | Removes `v`; `false` if absent. A node with two children takes the **smallest value of its right subtree**, and that node is cut out. |
| `size()`, `height()` | `const`. `height` is 0 for an empty tree, 1 for one node. Derive both by walking the tree. |
| `to_vector()` | `const`. The values in ascending order (an in-order walk). |
| `clone()` | `const`. An independent deep copy. |

The tests compare a long random sequence of inserts and erases with `std::set`, check that a moved-from tree is a valid empty one,
that nothing leaks, and (Clean tier) that `tree.hpp` contains no `new`, `delete`, `malloc` or `free`.
