#include <algorithm>
#include <cctype>
#include <climits>
#include <cstddef>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "library.hpp"
#include "zth_test.hpp"

namespace {

using lib::Library;
using lib::Status;
using Ids = std::vector<int>;

// A copy of a book (so that the tests do not depend on Book's getters being const: another test checks that).
lib::Book B(Library& L, int id) { return L.book(id); }

// true only for a plain std::logic_error: std::out_of_range and std::invalid_argument are logic_errors too, but they
// mean something else here.
template <class F>
bool throws_logic_error(F&& f) {
    try {
        f();
    } catch (const std::out_of_range&) {
        return false;
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::logic_error&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

std::string str(const lib::Book& b) {
    std::ostringstream os;
    os << b;
    return os.str();
}

// A library with `members` members (Ada, Bob, Cy, ...) and `books` books (Book 1, Book 2, ...).
Library make(int members, int books) {
    Library L;
    const char* names[] = {"Ada", "Bob", "Cy", "Di", "Ed"};
    for (int i = 0; i < members; ++i) L.add_member(names[i % 5]);
    for (int i = 1; i <= books; ++i) L.add_book("Book " + std::to_string(i), "Author " + std::to_string(i));
    return L;
}

// ---- the const interface: everything that only looks must work on a const Library ---------------------------------
template <class L>
constexpr bool all_const_queries = requires(const L& l) {
    l.member_count();
    l.book_count();
    l.book(1);
    l.fines_owed(1);
    l.loans(1);
    l.loans_of(1);
    l.overdue(1);
};
template <class Bk>
constexpr bool book_is_const = requires(const Bk& b) {
    b.id();
    b.title();
    b.author();
    b.status();
    b.borrower();
    b.due();
};
template <class L>
void const_queries_test() {
    if constexpr (!all_const_queries<L>) {
        ::zth::fail("member_count, book_count, book, fines_owed, loans, loans_of and overdue must all be const member functions (a const Library must be usable)");
    } else {
        L l;
        const int m = l.add_member("Ada");
        const int b = l.add_book("Dune", "Frank Herbert");
        l.checkout(m, b, 5);
        const L& c = l;
        ZTH_EXPECT_EQ(c.member_count(), std::size_t{1});
        ZTH_EXPECT_EQ(c.book_count(), std::size_t{1});
        ZTH_EXPECT_EQ(c.loans(m), 1);
        ZTH_EXPECT_EQ(c.fines_owed(m), 0LL);
        ZTH_EXPECT_EQ(c.loans_of(m), (Ids{b}));
        ZTH_EXPECT_EQ(c.overdue(100), (Ids{b}));
        lib::Book copy = c.book(b);
        ZTH_EXPECT_EQ(copy.id(), b);
    }
}
template <class Bk>
void const_book_test() {
    if constexpr (!book_is_const<Bk>) {
        ::zth::fail("Book's id, title, author, status, borrower and due must be const member functions");
    } else {
        const Bk b("Dune", "Frank Herbert", 7);
        ZTH_EXPECT_EQ(b.id(), 7);
        ZTH_EXPECT_EQ(b.title(), std::string("Dune"));
        ZTH_EXPECT_EQ(b.author(), std::string("Frank Herbert"));
        ZTH_EXPECT_TRUE(b.status() == Status::Available);
        ZTH_EXPECT_EQ(b.borrower(), 0);
        ZTH_EXPECT_EQ(b.due(), 0);
    }
}

// ---- source scan of library.hpp (zth::source_layout reads the learner's header) ---------------------------------
zth::SourceLayout my_layout() { return zth::source_layout_of("library.hpp"); }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "add_member and add_book hand out the ids 1, 2, 3, ... and the library counts them") {
    Library L;
    ZTH_EXPECT_EQ(L.add_member("Ada"), 1);
    ZTH_EXPECT_EQ(L.add_member("Bob"), 2);
    ZTH_EXPECT_EQ(L.add_book("Dune", "Frank Herbert"), 1);
    ZTH_EXPECT_EQ(L.add_book("Emma", "Jane Austen"), 2);
    ZTH_EXPECT_EQ(L.add_book("Ulysses", "James Joyce"), 3);
    ZTH_EXPECT_EQ(L.member_count(), std::size_t{2});
    ZTH_EXPECT_EQ(L.book_count(), std::size_t{3});
}
ZTH_TEST("works", "a new book knows its title, author and id, and is Available with nobody holding it") {
    Library L = make(0, 0);
    const int id = L.add_book("Dune", "Frank Herbert");
    lib::Book b = L.book(id);
    ZTH_EXPECT_EQ(b.id(), id);
    ZTH_EXPECT_EQ(b.title(), std::string("Dune"));
    ZTH_EXPECT_EQ(b.author(), std::string("Frank Herbert"));
    ZTH_EXPECT_TRUE(b.status() == Status::Available);
    ZTH_EXPECT_EQ(b.borrower(), 0);
    ZTH_EXPECT_EQ(b.due(), 0);
}
ZTH_TEST("works", "checkout puts the book on loan to the member, due loan_days later") {
    Library L = make(2, 3);
    ZTH_EXPECT_TRUE(L.checkout(2, 3, 10));
    lib::Book b = L.book(3);
    ZTH_EXPECT_TRUE(b.status() == Status::OnLoan);
    ZTH_EXPECT_EQ(b.borrower(), 2);
    ZTH_EXPECT_EQ(b.due(), 10 + Library::loan_days);
    ZTH_EXPECT_EQ(L.loans(2), 1);
    ZTH_EXPECT_EQ(L.loans(1), 0);
    ZTH_EXPECT_EQ(L.loans_of(2), (Ids{3}));
}
ZTH_TEST("works", "the loan rules are the documented numbers") {
    ZTH_EXPECT_EQ(Library::max_loans, 3);
    ZTH_EXPECT_EQ(Library::loan_days, 14);
    ZTH_EXPECT_EQ(Library::fine_per_day, 25LL);
    ZTH_EXPECT_EQ(Library::lost_fee, 2500LL);
}
ZTH_TEST("works", "a book that is already on loan cannot be checked out again, and nothing changes") {
    Library L = make(2, 1);
    ZTH_EXPECT_TRUE(L.checkout(1, 1, 5));
    ZTH_EXPECT_FALSE(L.checkout(2, 1, 6));
    ZTH_EXPECT_EQ(B(L, 1).borrower(), 1);
    ZTH_EXPECT_EQ(B(L, 1).due(), 5 + Library::loan_days);
    ZTH_EXPECT_EQ(L.loans(2), 0);
    ZTH_EXPECT_FALSE(L.checkout(1, 1, 7));  // not even by the member who has it
    ZTH_EXPECT_EQ(B(L, 1).due(), 5 + Library::loan_days);
}
ZTH_TEST("works", "returning a book on time costs nothing and makes it Available again") {
    Library L = make(1, 1);
    L.checkout(1, 1, 0);
    ZTH_EXPECT_EQ(L.return_book(1, 10), 0LL);
    ZTH_EXPECT_TRUE(B(L, 1).status() == Status::Available);
    ZTH_EXPECT_EQ(B(L, 1).borrower(), 0);
    ZTH_EXPECT_EQ(B(L, 1).due(), 0);
    ZTH_EXPECT_EQ(L.loans(1), 0);
    ZTH_EXPECT_EQ(L.fines_owed(1), 0LL);
    ZTH_EXPECT_TRUE(L.checkout(1, 1, 11));  // and it can go out again
}
ZTH_TEST("works", "a late return costs fine_per_day for every day after the due day, and the member owes it") {
    Library L = make(1, 1);
    L.checkout(1, 1, 0);                                      // due day 14
    ZTH_EXPECT_EQ(L.return_book(1, 17), 3 * 25LL);
    ZTH_EXPECT_EQ(L.fines_owed(1), 75LL);
}
ZTH_TEST("works", "returning on the due day is on time; one day later costs one day's fine") {
    Library a = make(1, 1);
    a.checkout(1, 1, 3);
    ZTH_EXPECT_EQ(a.return_book(1, 17), 0LL);
    Library b = make(1, 1);
    b.checkout(1, 1, 3);
    ZTH_EXPECT_EQ(b.return_book(1, 18), 25LL);
}
ZTH_TEST("works", "a member can have at most max_loans books at once; returning one makes room") {
    Library L = make(1, 5);
    ZTH_EXPECT_TRUE(L.checkout(1, 1, 0));
    ZTH_EXPECT_TRUE(L.checkout(1, 2, 0));
    ZTH_EXPECT_TRUE(L.checkout(1, 3, 0));
    ZTH_EXPECT_FALSE(L.checkout(1, 4, 0));
    ZTH_EXPECT_TRUE(B(L, 4).status() == Status::Available);
    ZTH_EXPECT_EQ(L.loans(1), 3);
    L.return_book(2, 1);
    ZTH_EXPECT_TRUE(L.checkout(1, 4, 1));
    ZTH_EXPECT_EQ(L.loans(1), 3);
}
ZTH_TEST("works", "a Book prints as #id \"title\" by author [status], with the due day while on loan") {
    Library L = make(1, 0);
    const int id = L.add_book("Dune", "Frank Herbert");
    ZTH_EXPECT_EQ(str(L.book(id)), std::string("#1 \"Dune\" by Frank Herbert [Available]"));
    L.checkout(1, id, 1);
    ZTH_EXPECT_EQ(str(L.book(id)), std::string("#1 \"Dune\" by Frank Herbert [OnLoan, due day 15]"));
    L.mark_lost(id);
    ZTH_EXPECT_EQ(str(L.book(id)), std::string("#1 \"Dune\" by Frank Herbert [Lost]"));
}
ZTH_TEST("works", "Status is a scoped enum with a one-byte underlying type") {
    ZTH_EXPECT_FALSE((std::is_convertible_v<Status, int>));
    ZTH_EXPECT_EQ(sizeof(Status), std::size_t{1});
}
ZTH_TEST("works", "to_string gives each status its name") {
    ZTH_EXPECT_EQ(lib::to_string(Status::Available), std::string("Available"));
    ZTH_EXPECT_EQ(lib::to_string(Status::OnLoan), std::string("OnLoan"));
    ZTH_EXPECT_EQ(lib::to_string(Status::Lost), std::string("Lost"));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "an unknown member or book is std::out_of_range, in every function that takes an id") {
    Library L = make(1, 1);
    ZTH_EXPECT_THROWS(std::out_of_range, L.checkout(9, 1, 0));
    ZTH_EXPECT_THROWS(std::out_of_range, L.checkout(1, 9, 0));
    ZTH_EXPECT_THROWS(std::out_of_range, L.checkout(0, 1, 0));
    ZTH_EXPECT_THROWS(std::out_of_range, L.checkout(1, 0, 0));
    ZTH_EXPECT_THROWS(std::out_of_range, L.checkout(-1, 1, 0));
    ZTH_EXPECT_THROWS(std::out_of_range, L.checkout(1, -1, 0));
    ZTH_EXPECT_THROWS(std::out_of_range, (void)L.return_book(9, 0));
    ZTH_EXPECT_THROWS(std::out_of_range, (void)L.return_book(0, 0));
    ZTH_EXPECT_THROWS(std::out_of_range, L.mark_lost(9));
    ZTH_EXPECT_THROWS(std::out_of_range, (void)L.book(9));
    ZTH_EXPECT_THROWS(std::out_of_range, (void)L.book(0));
    ZTH_EXPECT_THROWS(std::out_of_range, (void)L.fines_owed(9));
    ZTH_EXPECT_THROWS(std::out_of_range, (void)L.loans(9));
    ZTH_EXPECT_THROWS(std::out_of_range, (void)L.loans_of(9));
    ZTH_EXPECT_THROWS(std::out_of_range, L.pay_fine(9, 5));
    ZTH_EXPECT_NO_THROW((void)L.book(1));
    ZTH_EXPECT_NO_THROW((void)L.loans(1));
    ZTH_EXPECT_TRUE(B(L, 1).status() == Status::Available);  // none of that changed anything
}
ZTH_TEST("robust", "a failed checkout with an unknown id changes nothing") {
    Library L = make(1, 1);
    try {
        L.checkout(1, 9, 0);
    } catch (const std::out_of_range&) {
    }
    try {
        L.checkout(9, 1, 0);
    } catch (const std::out_of_range&) {
    }
    ZTH_EXPECT_TRUE(B(L, 1).status() == Status::Available);
    ZTH_EXPECT_EQ(L.loans(1), 0);
}
ZTH_TEST("robust", "returning a book that is not on loan is std::logic_error and changes nothing") {
    Library L = make(1, 2);
    ZTH_EXPECT_TRUE(throws_logic_error([&] { (void)L.return_book(1, 5); }));  // Available
    ZTH_EXPECT_TRUE(L.checkout(1, 2, 0));
    ZTH_EXPECT_EQ(L.return_book(2, 3), 0LL);
    ZTH_EXPECT_TRUE(throws_logic_error([&] { (void)L.return_book(2, 4); }));  // already returned
    ZTH_EXPECT_EQ(L.fines_owed(1), 0LL);
    L.mark_lost(1);
    ZTH_EXPECT_TRUE(throws_logic_error([&] { (void)L.return_book(1, 5); }));  // Lost
    ZTH_EXPECT_TRUE(B(L, 1).status() == Status::Lost);
}
ZTH_TEST("robust", "a member who owes money cannot borrow until it is paid; the refused checkout changes nothing") {
    Library L = make(2, 3);
    L.checkout(1, 1, 0);
    ZTH_EXPECT_EQ(L.return_book(1, 15), 25LL);
    ZTH_EXPECT_FALSE(L.checkout(1, 2, 20));
    ZTH_EXPECT_TRUE(B(L, 2).status() == Status::Available);
    ZTH_EXPECT_EQ(L.loans(1), 0);
    ZTH_EXPECT_TRUE(L.checkout(2, 2, 20));  // somebody else is not affected
    L.pay_fine(1, 25);
    ZTH_EXPECT_EQ(L.fines_owed(1), 0LL);
    ZTH_EXPECT_TRUE(L.checkout(1, 3, 21));
}
ZTH_TEST("robust", "partial payments reduce what is owed; paying more than owed or zero or less is std::invalid_argument") {
    Library L = make(1, 1);
    L.checkout(1, 1, 0);
    L.return_book(1, 24);  // 10 days late: 250
    ZTH_EXPECT_EQ(L.fines_owed(1), 250LL);
    L.pay_fine(1, 100);
    ZTH_EXPECT_EQ(L.fines_owed(1), 150LL);
    ZTH_EXPECT_THROWS(std::invalid_argument, L.pay_fine(1, 151));
    ZTH_EXPECT_THROWS(std::invalid_argument, L.pay_fine(1, 0));
    ZTH_EXPECT_THROWS(std::invalid_argument, L.pay_fine(1, -5));
    ZTH_EXPECT_EQ(L.fines_owed(1), 150LL);
    ZTH_EXPECT_NO_THROW(L.pay_fine(1, 150));  // exactly what is owed
    ZTH_EXPECT_EQ(L.fines_owed(1), 0LL);
    ZTH_EXPECT_THROWS(std::invalid_argument, L.pay_fine(1, 1));  // nothing owed: any payment is too much
}
ZTH_TEST("robust", "pay_fine checks the member before the amount") {
    Library L = make(1, 0);
    ZTH_EXPECT_THROWS(std::out_of_range, L.pay_fine(5, -1));
    ZTH_EXPECT_THROWS(std::out_of_range, L.pay_fine(0, 0));
}
ZTH_TEST("robust", "a lost book that was on loan costs its borrower the lost fee and leaves their loans") {
    Library L = make(2, 2);
    L.checkout(1, 1, 0);
    L.checkout(1, 2, 0);
    L.mark_lost(1);
    ZTH_EXPECT_TRUE(B(L, 1).status() == Status::Lost);
    ZTH_EXPECT_EQ(B(L, 1).borrower(), 0);
    ZTH_EXPECT_EQ(B(L, 1).due(), 0);
    ZTH_EXPECT_EQ(L.fines_owed(1), Library::lost_fee);
    ZTH_EXPECT_EQ(L.fines_owed(2), 0LL);
    ZTH_EXPECT_EQ(L.loans(1), 1);
    ZTH_EXPECT_EQ(L.loans_of(1), (Ids{2}));
    ZTH_EXPECT_EQ(L.overdue(1000), (Ids{2}));
}
ZTH_TEST("robust", "a lost book that was on the shelf costs nobody anything, but is lost for good") {
    Library L = make(1, 1);
    L.mark_lost(1);
    ZTH_EXPECT_TRUE(B(L, 1).status() == Status::Lost);
    ZTH_EXPECT_EQ(L.fines_owed(1), 0LL);
    ZTH_EXPECT_FALSE(L.checkout(1, 1, 0));
    ZTH_EXPECT_TRUE(B(L, 1).status() == Status::Lost);
    ZTH_EXPECT_TRUE(throws_logic_error([&] { L.mark_lost(1); }));
    ZTH_EXPECT_EQ(L.fines_owed(1), 0LL);
}
ZTH_TEST("robust", "fines add up: late returns, a lost book, several times") {
    Library L = make(1, 3);
    L.checkout(1, 1, 0);
    ZTH_EXPECT_EQ(L.return_book(1, 16), 50LL);
    L.pay_fine(1, 50);
    L.checkout(1, 2, 20);
    L.checkout(1, 3, 20);
    ZTH_EXPECT_EQ(L.return_book(2, 36), 50LL);  // due 34
    L.mark_lost(3);
    ZTH_EXPECT_EQ(L.fines_owed(1), 50LL + Library::lost_fee);
}
ZTH_TEST("robust", "fines belong to the member who had the book, not the one returning it") {
    Library L = make(2, 2);
    L.checkout(1, 1, 0);
    L.checkout(2, 2, 0);
    L.return_book(2, 20);
    ZTH_EXPECT_EQ(L.fines_owed(1), 0LL);
    ZTH_EXPECT_EQ(L.fines_owed(2), 6 * 25LL);
}
ZTH_TEST("robust", "overdue lists the books on loan that were due BEFORE today, ascending") {
    Library L = make(2, 5);
    L.checkout(1, 4, 0);  // due 14
    L.checkout(2, 2, 3);  // due 17
    L.checkout(1, 5, 3);  // due 17
    L.checkout(2, 1, 10);  // due 24
    ZTH_EXPECT_EQ(L.overdue(14), (Ids{}));            // due today is not overdue
    ZTH_EXPECT_EQ(L.overdue(15), (Ids{4}));
    ZTH_EXPECT_EQ(L.overdue(17), (Ids{4}));
    ZTH_EXPECT_EQ(L.overdue(18), (Ids{2, 4, 5}));
    ZTH_EXPECT_EQ(L.overdue(100), (Ids{1, 2, 4, 5}));
    L.return_book(4, 20);
    ZTH_EXPECT_EQ(L.overdue(100), (Ids{1, 2, 5}));  // returned books are not overdue
    L.mark_lost(2);
    ZTH_EXPECT_EQ(L.overdue(100), (Ids{1, 5}));     // lost books are not overdue
}
ZTH_TEST("robust", "loans_of lists a member's books in ascending order, whatever order they were borrowed in") {
    Library L = make(2, 6);
    L.checkout(1, 5, 0);
    L.checkout(1, 2, 0);
    L.checkout(1, 6, 0);
    L.checkout(2, 1, 0);
    ZTH_EXPECT_EQ(L.loans_of(1), (Ids{2, 5, 6}));
    ZTH_EXPECT_EQ(L.loans_of(2), (Ids{1}));
    L.return_book(5, 1);
    ZTH_EXPECT_EQ(L.loans_of(1), (Ids{2, 6}));
}
ZTH_TEST("robust", "each member has their own loan count and their own fines") {
    Library L = make(2, 6);
    for (int b = 1; b <= 3; ++b) ZTH_EXPECT_TRUE(L.checkout(1, b, 0));
    ZTH_EXPECT_FALSE(L.checkout(1, 4, 0));
    ZTH_EXPECT_TRUE(L.checkout(2, 4, 0));  // Ada being full does not stop Bob
    ZTH_EXPECT_EQ(L.loans(1), 3);
    ZTH_EXPECT_EQ(L.loans(2), 1);
}
ZTH_TEST("robust", "an empty name, title or author is std::invalid_argument and the library is unchanged") {
    Library L;
    ZTH_EXPECT_THROWS(std::invalid_argument, L.add_member(""));
    ZTH_EXPECT_THROWS(std::invalid_argument, L.add_book("", "Someone"));
    ZTH_EXPECT_THROWS(std::invalid_argument, L.add_book("Something", ""));
    ZTH_EXPECT_EQ(L.member_count(), std::size_t{0});
    ZTH_EXPECT_EQ(L.book_count(), std::size_t{0});
    ZTH_EXPECT_EQ(L.add_member("Ada"), 1);  // the failed attempts did not use up an id
    ZTH_EXPECT_EQ(L.add_book("Dune", "Frank Herbert"), 1);
}
ZTH_TEST("robust", "a Book needs a positive id, a title and an author (std::invalid_argument)") {
    ZTH_EXPECT_THROWS(std::invalid_argument, lib::Book b("Dune", "Frank Herbert", 0));
    ZTH_EXPECT_THROWS(std::invalid_argument, lib::Book b("Dune", "Frank Herbert", -3));
    ZTH_EXPECT_THROWS(std::invalid_argument, lib::Book b("", "Frank Herbert", 1));
    ZTH_EXPECT_THROWS(std::invalid_argument, lib::Book b("Dune", "", 1));
    ZTH_EXPECT_NO_THROW(lib::Book b("Dune", "Frank Herbert", 1));
}
ZTH_TEST("robust", "huge fines do not overflow an int") {
    Library L = make(1, 1);
    L.checkout(1, 1, 0);
    ZTH_EXPECT_EQ(L.return_book(1, 2000000000), static_cast<long long>(2000000000 - 14) * 25);
    ZTH_EXPECT_EQ(L.fines_owed(1), static_cast<long long>(2000000000 - 14) * 25);
}
ZTH_TEST("robust", "everything that only looks works on a const Library") { const_queries_test<Library>(); }
ZTH_TEST("robust", "everything that only looks works on a const Book") { const_book_test<lib::Book>(); }
ZTH_TEST("robust", "a long random sequence of operations always agrees with a simple model of the rules") {
    // The model: plain arrays and the rules from the lesson.
    constexpr int M = 4, B = 8;
    Library L = make(M, B);
    struct MB { int status = 0; int borrower = 0; int due = 0; };  // 0 Available, 1 OnLoan, 2 Lost
    MB books[B + 1];
    long long fines[M + 1] = {};
    zth::Lcg rng(4242);
    auto next = [&](int mod) { return rng.below(mod); };
    int today = 0;
    auto loans_of_model = [&](int m) {
        int c = 0;
        for (int b = 1; b <= B; ++b) c += (books[b].status == 1 && books[b].borrower == m);
        return c;
    };
    for (int step = 0; step < 1500; ++step) {
        today += next(4);
        const int op = next(10);
        const int m = 1 + next(M), b = 1 + next(B);
        if (op <= 3) {
            const bool expect = fines[m] == 0 && loans_of_model(m) < 3 && books[b].status == 0;
            ZTH_EXPECT_EQ(L.checkout(m, b, today), expect);
            if (expect) books[b] = MB{1, m, today + 14};
        } else if (op <= 6) {
            if (books[b].status != 1) {
                ZTH_EXPECT_TRUE(throws_logic_error([&] { (void)L.return_book(b, today); }));
            } else {
                const long long fine = today > books[b].due ? static_cast<long long>(today - books[b].due) * 25 : 0;
                ZTH_EXPECT_EQ(L.return_book(b, today), fine);
                fines[books[b].borrower] += fine;
                books[b] = MB{};
            }
        } else if (op == 7) {
            if (next(3) != 0) continue;  // lose books rarely
            if (books[b].status == 2) {
                ZTH_EXPECT_TRUE(throws_logic_error([&] { L.mark_lost(b); }));
            } else {
                if (books[b].status == 1) fines[books[b].borrower] += 2500;
                L.mark_lost(b);
                books[b] = MB{2, 0, 0};
            }
        } else if (op == 8 && fines[m] > 0) {
            const long long pay = 1 + next(static_cast<int>(std::min<long long>(fines[m], 4000)));
            L.pay_fine(m, pay);
            fines[m] -= pay;
        } else {
            Ids want;
            for (int k = 1; k <= B; ++k)
                if (books[k].status == 1 && books[k].due < today) want.push_back(k);
            ZTH_EXPECT_EQ(L.overdue(today), want);
        }
        // after every step, the whole library matches the model
        for (int k = 1; k <= B; ++k) {
            lib::Book bk = L.book(k);
            if (static_cast<int>(bk.status()) != books[k].status || bk.borrower() != books[k].borrower || bk.due() != books[k].due) {
                ::zth::fail("after step " + std::to_string(step) + ", book " + std::to_string(k) + " is " + str(bk) + " (borrower " +
                            std::to_string(bk.borrower()) + ") but the rules say status " + std::to_string(books[k].status) +
                            ", borrower " + std::to_string(books[k].borrower) + ", due " + std::to_string(books[k].due));
                return;
            }
        }
        for (int k = 1; k <= M; ++k) {
            ZTH_EXPECT_EQ(L.fines_owed(k), fines[k]);
            ZTH_EXPECT_EQ(L.loans(k), loans_of_model(k));
        }
    }
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "library.hpp does not use new, delete, malloc or free") { ZTH_EXPECT_NO_RAW_MEMORY("library.hpp"); }
ZTH_TEST("clean", "library.hpp has no `using namespace` directive") { ZTH_EXPECT_FALSE(my_layout().using_directive); }
ZTH_TEST("clean", "everything in library.hpp is inside namespace lib") {
    const auto L = my_layout();
    if (!L.leftover.empty()) {
        ::zth::fail("there is code outside any namespace: `" + L.leftover.substr(0, 70) + "...` (move it inside namespace lib)");
        return;
    }
    ZTH_EXPECT_EQ(L.top_level_namespaces.size(), std::size_t{1});
    ZTH_EXPECT_TRUE(!L.top_level_namespaces.empty() && L.top_level_namespaces[0] == "lib");
}
