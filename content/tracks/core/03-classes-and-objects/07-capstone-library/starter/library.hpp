#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Everything lives in namespace lib. Read lesson.md for the rules; the names and signatures below are fixed (the tests
// call exactly these), the bodies are yours. Every stub compiles but does nothing useful yet.
namespace lib {

enum class Status : std::uint8_t { Available, OnLoan, Lost };

inline std::string to_string(Status s) {
    switch (s) {
        case Status::Available: return "Available";
        case Status::OnLoan: return "OnLoan";
        case Status::Lost: return "Lost";
    }
    return "Unknown";
}

class Library;

class Book {
public:
    // TODO: an id that is not positive, an empty title or an empty author: std::invalid_argument.
    Book(std::string title, std::string author, int id) : id_(id), title_(std::move(title)), author_(std::move(author)) {}

    // TODO: these are all read-only. Make them const, and return const references for the strings.
    int id() { return id_; }
    std::string title() { return title_; }
    std::string author() { return author_; }
    Status status() { return status_; }
    int borrower() { return borrower_; }  // the member who has it, or 0
    int due() { return due_; }            // the day it is due back, or 0

private:
    // TODO: only the Library may change a book's status, borrower and due day. (`friend class Library;`)
    int id_;
    std::string title_;
    std::string author_;
    Status status_ = Status::Available;
    int borrower_ = 0;
    int due_ = 0;
};

// TODO: print `#7 "Dune" by Frank Herbert [Available]`, or `#7 "Dune" by Frank Herbert [OnLoan, due day 15]`.
// (Use to_string(Status). Take the stream by reference and return it.)
inline std::ostream& operator<<(std::ostream& os, const Book&) { return os; }

class Library {
public:
    static constexpr int max_loans = 3;            // books one member may have at a time
    static constexpr int loan_days = 14;           // a book is due this many days after checkout
    static constexpr long long fine_per_day = 25;  // cents, per day late
    static constexpr long long lost_fee = 2500;    // cents, charged when a borrowed book is lost

    // TODO: ids are 1, 2, 3, ... in the order added. An empty name (or title, or author) throws std::invalid_argument
    // and leaves the library unchanged.
    int add_member(const std::string& name) { return 0; }
    int add_book(const std::string& title, const std::string& author) { return 0; }

    // TODO: all of these are lookups: const. An unknown id throws std::out_of_range.
    std::size_t member_count() { return 0; }
    std::size_t book_count() { return 0; }
    const Book& book(int id) { throw std::logic_error("book(): not written yet"); }
    long long fines_owed(int member_id) { return 0; }
    int loans(int member_id) { return 0; }                  // how many books the member has on loan now
    std::vector<int> loans_of(int member_id) { return {}; }  // their ids, ascending
    std::vector<int> overdue(int today) { return {}; }       // ids of books on loan that were due before `today`, ascending

    // TODO: unknown member or book: std::out_of_range. Returns false, changing nothing, if the member
    // owes any money, already has max_loans books, or the book is not Available. Otherwise the book is OnLoan, its
    // borrower is the member, and it is due today + loan_days.
    bool checkout(int member_id, int book_id, int today) { return false; }

    // TODO: unknown book: std::out_of_range. A book that is not OnLoan: std::logic_error. Returns the fine in cents:
    // fine_per_day for each day after the due day (0 if on time), which is added to what the borrower owes.
    // The book becomes Available again, with no borrower and no due day.
    long long return_book(int book_id, int today) { return 0; }

    // TODO: unknown book: std::out_of_range. Already lost: std::logic_error. A book that was on loan costs its borrower
    // lost_fee. The book becomes Lost (nobody has it, and it can never be checked out again).
    void mark_lost(int book_id) {}

    // TODO: unknown member: std::out_of_range (checked first). A payment that is zero, negative, or more than the member
    // owes: std::invalid_argument. Otherwise it reduces what they owe.
    void pay_fine(int member_id, long long cents) {}

private:
    // TODO: your data (books, members with the fines they owe) and any helpers. For example:
    //   struct Member { std::string name; long long fines; };
    //   std::vector<Book> books_;
    //   std::vector<Member> members_;
};

}  // namespace lib
