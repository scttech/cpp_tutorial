#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "trace.hpp"

// Virtual functions: payroll for a company with different kinds of employee.
// Every TODO is a numbered step. The tiers also read this file: "Clean" wants `override` on EVERY function that overrides one.

// ---- The base class ---------------------------------------------------------------------------------------------------------------------------
// TODO 1: make monthly_pay() and title() virtual (customization points), and the destructor too: deleting an Employee through an
//         Employee* (a unique_ptr<Employee>) must run the DERIVED destructor first. The destructor traces "~Employee(<name>)".
// TODO 2: badge() ("<name>, <title>") is computed here in the CONSTRUCTOR and stored. A virtual call inside a constructor reaches the BASE
//         version (the derived part does not exist yet), so a Manager's badge says "employee". Compute it on demand instead; drop badge_.
class Employee {
public:
    explicit Employee(std::string name) : name_(std::move(name)) { badge_ = name_ + ", " + title(); }
    ~Employee() { trace("~Employee(" + name_ + ")"); }

    const std::string& name() const { return name_; }
    double monthly_pay() const { return 0.0; }
    std::string title() const { return "employee"; }
    std::string badge() const { return badge_; }

private:
    std::string name_;
    std::string badge_;
};

// ---- The derived classes ---------------------------------------------------------------------------------------------------------------------
// TODO 3: Salaried(name, annual): monthly pay is annual / 12, title "salaried", a negative (or NaN) salary throws std::invalid_argument.
//         The destructor traces "~Salaried(<name>)". Mark every overriding function `override`.
class Salaried : public Employee {
public:
    Salaried(std::string name, double annual) : Employee(std::move(name)) { (void)annual; }
    ~Salaried() { trace("~Salaried(" + name() + ")"); }

    double monthly_pay() const { return 0; }
    std::string title() const { return "salaried"; }
};

// TODO 4: Hourly(name, rate, hours), a leaf class (final): the first 160 hours are paid at the rate, every hour beyond that at 1.5 times
//         the rate; title "hourly"; a negative rate or hours throws std::invalid_argument. Destructor traces "~Hourly(<name>)".
//         (Look closely at the signature of monthly_pay below: it does not override anything. `override` would have told you.)
class Hourly : public Employee {
public:
    Hourly(std::string name, double rate, double hours) : Employee(std::move(name)) {
        (void)rate;
        (void)hours;
    }
    ~Hourly() { trace("~Hourly(" + name() + ")"); }

    double monthly_pay() { return 0; }
    std::string title() const { return "hourly"; }
};

// TODO 5: Manager(name, annual) is a Salaried (final): its pay is the salaried pay PLUS 250 per report (call the base version:
//         `Salaried::monthly_pay()`); add_report() / reports(); title "manager"; destructor traces "~Manager(<name>)".
class Manager : public Salaried {
public:
    Manager(std::string name, double annual) : Salaried(std::move(name), annual) {}
    ~Manager() { trace("~Manager(" + name() + ")"); }

    void add_report() {}
    int reports() const { return 0; }
};

// ---- Functions that work on any Employee ---------------------------------------------------------------------------------------------------
using Staff = std::vector<std::unique_ptr<Employee>>;

// TODO 6: the sum of the monthly pay of the staff; a null pointer is skipped.
inline double total_payroll(const Staff& staff) {
    (void)staff;
    return 0;
}

// TODO 7: the badge of every employee, in order (a null pointer is skipped).
inline std::vector<std::string> badges(const Staff& staff) {
    (void)staff;
    return {};
}

// ---- RTTI -------------------------------------------------------------------------------------------------------------------------------------
// TODO 8: as_manager: dynamic_cast to a POINTER: the Manager, or nullptr when `e` is not a Manager. (dynamic_cast needs a polymorphic
//         base, one with a virtual function: that is why TODO 1 comes first.)
inline const Manager* as_manager(const Employee& e) {
    (void)e;
    return nullptr;
}

// TODO 9: must_be_manager: dynamic_cast to a REFERENCE: the same object as a Manager&; a failed reference cast throws std::bad_cast.
inline Manager& must_be_manager(Employee& e) {
    static Manager unused("?", 0);
    (void)e;
    return unused;
}

// TODO 10: how many managers are in the staff (null pointers are skipped)?
inline std::size_t count_managers(const Staff& staff) {
    (void)staff;
    return 0;
}

// TODO 11: same_kind: do the two objects have EXACTLY the same dynamic type? (typeid(a) == typeid(b)). A Manager IS-A Salaried, but
//          they are not the same kind.
inline bool same_kind(const Employee& a, const Employee& b) {
    (void)a;
    (void)b;
    return false;
}
