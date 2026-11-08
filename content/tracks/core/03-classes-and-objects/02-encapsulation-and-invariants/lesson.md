# Encapsulation and invariants

A class is more than a bag of data with functions attached. Its real job is to **keep a promise**: some statement about
its data that is true every time anyone looks at it. That statement is the class's **invariant**.

- A `Money` is a whole number of cents.
- A `BankAccount`'s balance is never negative.
- A `std::vector`'s size is never more than its capacity, and its elements are alive.

If the data is public, anyone can break the promise, and then *every* function that uses the class has to defend against
a broken object. **Encapsulation** reverses that: make the data `private`, so that only the class's own member functions
can touch it. Now the invariant only has to be protected in **one place**, the class, instead of at every call site in the
program.

```cpp
class Account {
public:
    bool withdraw(long long cents) {
        if (cents > balance_) return false;   // the invariant (balance >= 0) is protected HERE ...
        balance_ -= cents;
        return true;
    }
    long long balance() const { return balance_; }
private:
    long long balance_ = 0;                   // ... because nobody else can reach this
};
```

(`struct` and `class` are the same except the default access: members of a `struct` are public, members of a `class` are
private. By convention `struct` is for plain data with no invariant, `class` for types that protect one.)

## Getters that give the invariant away

A getter should let callers **look**, not **touch**. Watch the return type:

```cpp
long long&               balance()       { return balance_; }   // BAD: acct.balance() = -5000; compiles and works
const std::vector<int>&  history() const { return history_;  }  // OK: a read-only view
long long                balance() const { return balance_;  }  // OK: a copy
std::vector<int>&        history()       { return history_;  }  // BAD: acct.history().clear();
```

Returning a **mutable reference** to a private member is the same as making it public. Return a copy (cheap types), or a
**`const` reference** (to avoid copying something big, like a vector or a string).

## `const` member functions

The `const` after the parameter list is a promise that the function does not change the object:

```cpp
long long balance() const { return balance_; }
```

Without it, a `const BankAccount&` (what you get from `const&` parameters, and from `const` objects) cannot call the
function at all. Make every function that only reads **`const`**. Module 3 has a whole lesson on this; for now, remember
that getters should always be `const`.

## Three habits that keep invariants safe

**1. Validate, then commit.** An operation that can fail must check everything *before* it changes anything; then
"failed" really means "nothing happened". Look at a transfer between two accounts: if you take the money from the first
account and *then* discover that the second one cannot accept it, the money is gone. Check both sides first, change
second. (This is called the **strong exception guarantee**: all or nothing.)

**2. One place changes the state.** When an invariant ties several members together (here: *the balance equals the sum
of the history*), do not update them separately in every function. Write one private function that updates all of them
together, and make everything else call it. Then they cannot drift apart.

**3. Think about aliasing.** `a.transfer_to(a, 30)`: both "accounts" are the same object. Does your code still do the
right thing, or does it, say, record two history entries? The same trap shows up in copy assignment (`x = x`), which you
will meet in Module 4.

## Your task

Complete `account.hpp`. As given, the class is completely unprotected. `main.cpp` (read-only) is a demo: press **Run**.

| Member | Behavior |
|---|---|
| `BankAccount(std::string owner, long long opening_balance = 0)` | `explicit`. An empty owner or a negative opening balance throws `std::invalid_argument`. A non-zero opening balance is the **first history entry**. |
| `owner()`, `balance()`, `history()` | Read-only getters: `const` member functions returning a `const` reference or a copy. History entries are signed cents: deposits `+`, withdrawals `-`. |
| `deposit(cents)` | `cents` must be positive (`std::invalid_argument`). If the balance would overflow a `long long`, throw `std::overflow_error`. |
| `bool withdraw(cents)` | `cents` must be positive (`std::invalid_argument`). Not enough money: return `false`. |
| `bool transfer_to(BankAccount& to, cents)` | `cents` must be positive. Not enough money: `false`. All or nothing: if `to` would overflow, throw `std::overflow_error` and change **neither** account. Transferring to yourself changes nothing (but needs the funds). |

The rules that must hold at all times, however the account is used:

1. the owner is never empty
2. the balance is never negative
3. `balance()` equals the sum of `history()`
4. an operation that fails (throws, or returns `false`) changes nothing: not the balance, not the history

The tests check these by *using* your class in many different ways (including a long random sequence of operations), and
by trying to tamper with an account through its getters.
