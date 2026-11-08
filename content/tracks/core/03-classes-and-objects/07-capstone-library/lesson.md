# Capstone: Library Management

Build the model of a small lending library: members borrow books, books come back late, fines pile up, books get lost.
Everything from Module 3 is in here: classes that protect their **invariants**, `const` where nothing changes, an
`enum class` state machine, an operator for printing, a `friend`, everything inside a **namespace**, and errors reported
with the right **exception**. There is no `main` to write and no input to parse: the tests drive your classes directly,
and `main.cpp` (read-only) is a small demo.

The names and signatures in `library.hpp` are fixed (the tests call exactly these); every stub compiles but does nothing yet.

## The rules

**Days** are plain `int`s (day 0, day 1, ...). **Money** is `long long` cents. Ids start at 1 (members and books have their own
sequences) and are handed out in the order added.

| Constant | Value | Meaning |
|---|---|---|
| `Library::max_loans` | 3 | most books one member can have at a time |
| `Library::loan_days` | 14 | a book checked out on day `d` is **due** on day `d + 14` |
| `Library::fine_per_day` | 25 | cents for each day after the due day |
| `Library::lost_fee` | 2500 | cents, charged when a book that someone had is lost |

### The Book

`Book(title, author, id)`: an `id` that is not positive, an empty title or an empty author throws `std::invalid_argument`.
A book has a **status**: `Available`, `OnLoan` or `Lost`. It also knows who `borrower()` is (a member id, or `0`) and the day it
is `due()` (or `0`). Those are only ever changed by the `Library`, so the `Library` is a `friend` of `Book`; nobody else
can touch them. All of `Book`'s getters are `const`.

`operator<<` prints `#7 "Dune" by Frank Herbert [Available]`, and while it is out `#7 "Dune" by Frank Herbert [OnLoan, due day 15]`.
A lost book prints `[Lost]`.

### The Library

| Function | What it does |
|---|---|
| `add_member(name)`, `add_book(title, author)` | Returns the new id. An empty name, title or author throws `std::invalid_argument` and **adds nothing** (the next id is not used up). |
| `member_count()`, `book_count()`, `book(id)`, `fines_owed(member)`, `loans(member)`, `loans_of(member)`, `overdue(today)` | Lookups, all `const`. An unknown member or book id throws `std::out_of_range`. `loans` counts a member's books on loan, `loans_of` lists their ids **ascending**, and `overdue(today)` lists the ids of books on loan whose due day is **before** `today`, ascending. |
| `checkout(member, book, today)` | Unknown member or book: `std::out_of_range`. Returns `false` and **changes nothing** if the member owes any money, already has `max_loans` books, or the book is not `Available`. Otherwise the book is `OnLoan`, its borrower is the member, and it is due `today + loan_days`. Returns `true`. |
| `return_book(book, today)` | Unknown book: `std::out_of_range`. A book that is not `OnLoan`: `std::logic_error`. Otherwise it is `Available` again (no borrower, no due day) and the function returns the **fine**: `fine_per_day` for each day after the due day (returning on the due day is on time), and the same amount is added to what the **borrower** owes (whoever they are, whoever brought it back). |
| `mark_lost(book)` | Unknown book: `std::out_of_range`. Already lost: `std::logic_error`. A book that was on loan costs its borrower `lost_fee`, and it is no longer one of their loans. Either way the book is `Lost` (no borrower, no due day) and can never be checked out again. |
| `pay_fine(member, cents)` | Unknown member: `std::out_of_range` (checked **first**). A payment of zero or less, or more than the member owes: `std::invalid_argument`. Otherwise it reduces what they owe. |

## Design hints

- Keep the data in two vectors (`std::vector<Book>`, and a small private `Member` struct with the name and the fines). The id of
  the n-th element is n + 1, so a lookup is a bounds check and an index. Write that check **once**, in a private helper, and
  use it everywhere: every function that takes an id needs it.
- A member's loans do not need to be stored separately: they are the books that are `OnLoan` with that borrower. Counting them
  is the single source of truth, so it can never disagree with the books.
- **Validate, then change.** `checkout` and `pay_fine` must check every rule before they change anything, so a refused
  operation leaves no trace. `add_book` must validate (the `Book` constructor does) *before* it stores the book.
- Be careful with the **int** arithmetic: fines are `long long`, and a fine for 2 billion days overflows an `int`.
- Mark everything that only reads as `const`, and keep `using namespace` out of the header.

## Tiers

- **Works**: the basic rules, one at a time.
- **Robust**: every error and the order of checks, the due-day boundary, fines that add up, lost books, `const` access, and a
  long random sequence of operations compared against a simple model of the rules.
- **Clean**: `library.hpp` has no `new`/`delete`/`malloc`/`free`, no `using namespace`, and everything is inside `namespace lib`.
