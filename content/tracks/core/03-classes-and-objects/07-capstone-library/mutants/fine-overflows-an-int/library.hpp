// MUTANT: the fine is computed in int.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace lib {

enum class Status : std::uint8_t { Available, OnLoan, Lost };

// No `default:`: adding a Status later makes the compiler point at this switch.
inline std::string to_string(Status s) {
    switch (s) {
        case Status::Available: return "Available";
        case Status::OnLoan: return "OnLoan";
        case Status::Lost: return "Lost";
    }
    return "Unknown";
}

class Library;

// One book. Read-only to the outside world: only the Library (a friend) changes its status.
class Book {
public:
    Book(std::string title, std::string author, int id) : id_(id), title_(std::move(title)), author_(std::move(author)) {
        if (id_ <= 0) throw std::invalid_argument("Book: the id must be positive");
        if (title_.empty()) throw std::invalid_argument("Book: the title is empty");
        if (author_.empty()) throw std::invalid_argument("Book: the author is empty");
    }

    int id() const { return id_; }
    const std::string& title() const { return title_; }
    const std::string& author() const { return author_; }
    Status status() const { return status_; }
    int borrower() const { return borrower_; }  // the member who has it, or 0
    int due() const { return due_; }            // the day it is due back, or 0

private:
    friend class Library;

    int id_;
    std::string title_;
    std::string author_;
    Status status_ = Status::Available;
    int borrower_ = 0;
    int due_ = 0;
};

// #7 "Dune" by Frank Herbert [Available]      #7 "Dune" by Frank Herbert [OnLoan, due day 15]
inline std::ostream& operator<<(std::ostream& os, const Book& b) {
    os << '#' << b.id() << " \"" << b.title() << "\" by " << b.author() << " [" << to_string(b.status());
    if (b.status() == Status::OnLoan) os << ", due day " << b.due();
    return os << ']';
}

class Library {
public:
    static constexpr int max_loans = 3;             // books one member may have at a time
    static constexpr int loan_days = 14;            // a book is due this many days after checkout
    static constexpr long long fine_per_day = 25;   // cents, per day late
    static constexpr long long lost_fee = 2500;     // cents, charged when a borrowed book is lost

    // ---- adding things: ids are 1, 2, 3, ... in the order added -------------------------------------------
    int add_member(const std::string& name) {
        if (name.empty()) throw std::invalid_argument("Library: the member's name is empty");
        members_.push_back(Member{name, 0});
        return static_cast<int>(members_.size());
    }

    int add_book(const std::string& title, const std::string& author) {
        Book book(title, author, static_cast<int>(books_.size()) + 1);  // validates before anything is stored
        books_.push_back(std::move(book));
        return books_.back().id();
    }

    // ---- looking things up (all const) --------------------------------------------------------------------
    std::size_t member_count() const { return members_.size(); }
    std::size_t book_count() const { return books_.size(); }
    const Book& book(int id) const { return books_[book_index(id)]; }
    long long fines_owed(int member_id) const { return members_[member_index(member_id)].fines; }

    int loans(int member_id) const {
        (void)member_index(member_id);
        return static_cast<int>(std::count_if(books_.begin(), books_.end(), [&](const Book& b) {
            return b.status_ == Status::OnLoan && b.borrower_ == member_id;
        }));
    }

    std::vector<int> loans_of(int member_id) const {
        (void)member_index(member_id);
        std::vector<int> ids;
        for (const Book& b : books_)
            if (b.status_ == Status::OnLoan && b.borrower_ == member_id) ids.push_back(b.id_);
        return ids;  // books_ is in id order, so this is already ascending
    }

    // Books on loan that were due before `today`.
    std::vector<int> overdue(int today) const {
        std::vector<int> ids;
        for (const Book& b : books_)
            if (b.status_ == Status::OnLoan && b.due_ < today) ids.push_back(b.id_);
        return ids;
    }

    // ---- the operations -----------------------------------------------------------------------------------
    // false (nothing changes) if the member owes money, already has max_loans books, or the book is not Available.
    bool checkout(int member_id, int book_id, int today) {
        const std::size_t m = member_index(member_id);  // unknown member or book: std::out_of_range
        Book& b = books_[book_index(book_id)];
        if (members_[m].fines > 0) return false;
        if (loans(member_id) >= max_loans) return false;
        if (b.status_ != Status::Available) return false;
        b.status_ = Status::OnLoan;
        b.borrower_ = member_id;
        b.due_ = today + loan_days;
        return true;
    }

    // Returns the fine in cents (0 if on time) and adds it to what the borrower owes.
    long long return_book(int book_id, int today) {
        Book& b = books_[book_index(book_id)];
        if (b.status_ != Status::OnLoan) throw std::logic_error("Library: that book is not on loan");
        long long fine = 0;
        if (today > b.due_) fine = static_cast<long long>((today - b.due_) * static_cast<int>(fine_per_day));
        members_[member_index(b.borrower_)].fines += fine;
        b.status_ = Status::Available;
        b.borrower_ = 0;
        b.due_ = 0;
        return fine;
    }

    // A lost book is gone for good. If somebody had it, they pay the lost fee.
    void mark_lost(int book_id) {
        Book& b = books_[book_index(book_id)];
        if (b.status_ == Status::Lost) throw std::logic_error("Library: that book is already lost");
        if (b.status_ == Status::OnLoan) members_[member_index(b.borrower_)].fines += lost_fee;
        b.status_ = Status::Lost;
        b.borrower_ = 0;
        b.due_ = 0;
    }

    void pay_fine(int member_id, long long cents) {
        Member& m = members_[member_index(member_id)];  // unknown member first: std::out_of_range
        if (cents <= 0) throw std::invalid_argument("Library: the payment must be positive");
        if (cents > m.fines) throw std::invalid_argument("Library: the payment is more than the member owes");
        m.fines -= cents;
    }

private:
    struct Member {
        std::string name;
        long long fines;
    };

    std::size_t book_index(int id) const {
        if (id < 1 || static_cast<std::size_t>(id) > books_.size()) throw std::out_of_range("Library: unknown book");
        return static_cast<std::size_t>(id) - 1;
    }
    std::size_t member_index(int id) const {
        if (id < 1 || static_cast<std::size_t>(id) > members_.size()) throw std::out_of_range("Library: unknown member");
        return static_cast<std::size_t>(id) - 1;
    }

    std::vector<Book> books_;
    std::vector<Member> members_;
};

}  // namespace lib
