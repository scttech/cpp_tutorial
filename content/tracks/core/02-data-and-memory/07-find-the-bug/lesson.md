# Debug challenge: Find the Bug

Six small functions. Each has **one** bug, and each bug is a classic. All of them compile (some with warnings), and most
of them even *look* like they work. Your job is to work out what is wrong, fix it, and understand why.

The functions are described in `bugs.hpp`; the bodies are in `main.cpp`. A bug can be in either file. Keep the
signatures as they are.

## How to debug these

1. **Run the tests and read the failure messages.** They describe a *symptom* ("expected 10, got -572662307"), not the
   cause.
2. **Read the compiler's warnings** in the Problems tab. Several of these bugs are warned about.
3. **Reason about lifetimes.** For every pointer, reference or view: *what does it point at, and is that thing still
   alive here?*
4. **Look at the values.** In this course the test runner makes undefined behavior visible:
   - memory you have freed is overwritten with bytes of `0xDD` (so a value of `-572662307`, which is `0xDDDDDDDD`,
     means "you read freed memory");
   - the stack is pre-filled with `0x5A` bytes (`1515870810` is `0x5A5A5A5A`: "an uninitialized variable");
   - arrays are followed by a *canary* value; if it changes, something wrote past the end.

On a normal development machine, a tool called **AddressSanitizer** (`-fsanitize=address`) finds most of these for you
with an exact line number. It is not available in this browser-based runner yet, so this challenge makes you do the
detective work by hand: that is a skill worth having.

## The six bugs

| # | Function | The symptom you will see | Family |
|---|---|---|---|
| 1 | `fill_squares` | the guard value after the array changes | off-by-one |
| 2 | `Label` / `make_label` | garbage text, or a crash | a *reference* that outlives its target |
| 3 | `take_and_release` | `-572662307`, or a crash | use after free |
| 4 | `first_after_growth` | `-572662307`, but only when the vector had to grow | iterator / pointer invalidation |
| 5 | `count_positive` | `1515870810` plus something | uninitialized read |
| 6 | `make_entry` | the name is garbage (often bytes shown as `?`) | a `string_view` that outlives its string |

Work through them in any order. The tests are grouped one per bug in the *Works* tier. The *Robust* tier checks that
your fix is real and not a lucky one: for example, that the data really is owned, that nothing is freed twice, and that
it still works when nothing reallocates.

## Useful reminders

- Valid indexes of an array with `n` elements are `0 .. n-1`.
- A reference member (or a `string_view` member) does **not** keep what it refers to alive. If the object must stay
  valid by itself, it has to **own** its data (a `std::string` member).
- `delete p;` makes `p` dangle: anything you need from `*p` must be read first.
- `push_back` (and anything that grows a vector) may move all the elements to a new block: pointers, references and
  iterators taken earlier are then dangling. Take them *after* the change, or use an index.
- Local variables of built-in types are **not** zero-initialized.
