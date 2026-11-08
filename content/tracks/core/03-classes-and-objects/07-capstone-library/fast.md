# Fast path: what's different in C++

Model it like this:

- **Data:** `std::vector<Book>` and `std::vector<Member>` (a private struct). Id n lives at index n - 1; one private helper does the bounds check and throws `std::out_of_range`.
- **`Book`**: validating constructor, `const` getters, private status/borrower/due, `friend class Library`. `operator<<` as a non-member, in the same namespace. `enum class Status : std::uint8_t`.
- **A member's loans are derived** from the books (count the `OnLoan` ones with that borrower), not stored twice.
- **Validate, then commit** in `checkout` and `pay_fine`; the exception types are part of the contract (`out_of_range`, `logic_error`, `invalid_argument`) and so is the order of the checks.
- `long long` for money and fines; the due day is `today + loan_days`, and returning on the due day is on time.
- Everything inside `namespace lib`; no `using namespace`; no raw `new`/`delete`.
