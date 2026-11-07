# Pointers and the stack/heap

A **pointer** is a variable that holds the *address* of another object.

```cpp
int x = 10;
int* p = &x;     // &x : "the address of x"; p holds it
*p = 20;         // *p : "the object p points at": x is now 20
int* none = nullptr;   // points at nothing
```

Unlike a reference, a pointer can be null, can be re-pointed, and can be moved with arithmetic.

- **`nullptr`** means "no object". Dereferencing it (`*none`) is undefined behavior and usually a crash (the
  "segmentation fault"). Check before you dereference anything that might be null.
- **Pointer arithmetic**: `p + 1` is the address of the *next element*, not the next byte. A pair of pointers
  `[first, last)` describes a range of elements: `first` is the first element and **`last` is one past the end**. That
  is exactly how the standard algorithms work. An empty range is `first == last`. The element at `last` is not part of
  the range: do not read or write it.

```cpp
for (int* p = first; p != last; ++p) { *p += 1; }
```

## The stack and the heap

Your program's memory has two main areas for objects:

| | **Stack** | **Heap** |
|---|---|---|
| Holds | local variables, parameters | objects you create with `new` |
| Created | automatically, when a function is entered | explicitly: `new T(...)` |
| Destroyed | **automatically**, when the function returns | explicitly: `delete p;` |
| Size | small (about 8 MB in total) | large |
| Speed | very fast | slower: the allocator has to find room |

```cpp
int* p = new int(42);    // allocate an int on the heap; p points at it
// ... use *p ...
delete p;                // free it. Every `new` needs exactly one `delete`.
p = nullptr;             // good habit: the old address is no longer valid
```

(`new T[n]` pairs with `delete[]`. Mixing them up is a bug.)

`new` and `delete` are *raw* tools: **you** are responsible for the lifetime. In real C++ you rarely write them,
because `std::vector`, `std::string` and the smart pointers you will meet in Module 4 do it for you. But you need to
understand what they are doing, because they do exactly this underneath. This lesson builds a linked list by hand so
you can feel every one of the classic mistakes.

## The classic mistakes

- **Memory leak:** you lose the last pointer to a heap object, or forget to `delete` it. The memory is never returned.
- **Use after free (dangling pointer):** you `delete p;` and then read or write `*p`, or `p->next`. The memory may
  already belong to somebody else. In this course, freed memory is overwritten with `0xDD` bytes, so the bug shows up
  as obvious garbage (a value of `-572662307`, or a crash) instead of "mysteriously working".
- **Double free:** `delete` the same pointer twice.
- **Wrong deleter:** `delete` on something from `new[]`, or `delete[]` on something from `new`.
- **Null dereference.**

The tests in this lesson check for all of these: they count every allocation and free your code performs.

## A singly linked list

```cpp
struct Node { int value; Node* next; };     // in list.hpp (read-only)
```

A list is just a pointer to its first node, the **head**. Each node points at the next one; the last one's `next` is
`nullptr`. An empty list is a null head. Because adding or removing the first node *changes the head*, every function
below takes the head and **returns the new head**: `list = push_front(list, 7);`.

```
head -> [3|*] -> [2|*] -> [1|null]
```

Two rules prevent most bugs:

1. **Read what you need from a node before you delete it.** `delete node; node = node->next;` uses freed memory.
2. **Unlink first, then delete.** Make sure nothing still points at a node when you free it.

Also think about long lists: a loop uses a fixed amount of stack; a recursive function that handles one node and calls
itself for the rest uses stack proportional to the list length, and 500 000 nodes will overflow it.

## Your task

Implement in `main.cpp` (the starter's `main` shows a stack variable next to a heap variable):

| Function | Behavior |
|---|---|
| `Node* push_front(Node* head, int value)` | New node at the front; returns the new head. |
| `Node* push_back(Node* head, int value)` | New node at the end; returns the head. |
| `std::size_t list_length(const Node* head)` | Number of nodes. |
| `Node* list_find(Node* head, int value)` | The first node holding `value`, or `nullptr`. |
| `Node* remove_first(Node* head, int value)` | Unlink **and delete** the first node holding `value`; returns the head. |
| `Node* list_reverse(Node* head)` | Reverse by relinking the existing nodes; returns the new head. |
| `void free_list(Node* head)` | Delete every node. |
| `int* find_max(int* first, int* last)` | Pointer to the first largest element of `[first, last)`, or `last` if it is empty. |
| `void increment_all(int* first, int* last)` | Add 1 to each element of `[first, last)`. |

**Tiers:** *Works* and *Robust* test behavior (including empty lists and half a million nodes). *Fast* requires that
`reverse`, `remove_first`, `find` and `length` allocate nothing, and that `push_front` allocates one node. *Clean* is the
memory-safety tier: no leaks, no double frees, no wrong deleters.
