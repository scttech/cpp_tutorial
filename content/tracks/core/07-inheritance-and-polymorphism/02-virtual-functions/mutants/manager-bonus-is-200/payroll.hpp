// MUTANT: wrong amount.
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

// ---- The base class: virtual functions are the customization points --------------------------------------------------------------------------
class Employee {
public:
    explicit Employee(std::string name) : name_(std::move(name)) {}
    virtual ~Employee() { trace("~Employee(" + name_ + ")"); }  // VIRTUAL: deleting through an Employee* must run the derived destructor

    const std::string& name() const { return name_; }
    virtual double monthly_pay() const { return 0.0; }
    virtual std::string title() const { return "employee"; }

    // Not virtual, and it must not be cached in the constructor: a virtual call in a constructor reaches the BASE version.
    std::string badge() const { return name_ + ", " + title(); }

private:
    std::string name_;
};

class Salaried : public Employee {
public:
    Salaried(std::string name, double annual) : Employee(std::move(name)), annual_(annual) {
        if (!(annual >= 0)) throw std::invalid_argument("a salary must not be negative");
    }
    ~Salaried() override { trace("~Salaried(" + name() + ")"); }

    double monthly_pay() const override { return annual_ / 12; }
    std::string title() const override { return "salaried"; }

private:
    double annual_;
};

class Hourly final : public Employee {
public:
    static constexpr double kRegularHours = 160;  // per month; every hour beyond that is paid at 1.5 times the rate

    Hourly(std::string name, double rate, double hours) : Employee(std::move(name)), rate_(rate), hours_(hours) {
        if (!(rate >= 0) || !(hours >= 0)) throw std::invalid_argument("a rate and hours must not be negative");
    }
    ~Hourly() override { trace("~Hourly(" + name() + ")"); }

    double monthly_pay() const override {
        const double regular = std::min(hours_, kRegularHours);
        const double overtime = std::max(0.0, hours_ - kRegularHours);
        return rate_ * regular + 1.5 * rate_ * overtime;
    }
    std::string title() const override { return "hourly"; }

private:
    double rate_;
    double hours_;
};

class Manager final : public Salaried {
public:
    Manager(std::string name, double annual) : Salaried(std::move(name), annual) {}
    ~Manager() override { trace("~Manager(" + name() + ")"); }

    void add_report() { ++reports_; }
    int reports() const { return reports_; }
    double monthly_pay() const override { return Salaried::monthly_pay() + 200.0 * reports_; }  // extends the base behavior
    std::string title() const override { return "manager"; }

private:
    int reports_ = 0;
};

// ---- Functions that work on any Employee ---------------------------------------------------------------------------------------------------
using Staff = std::vector<std::unique_ptr<Employee>>;

inline double total_payroll(const Staff& staff) {
    double sum = 0;
    for (const auto& e : staff)
        if (e) sum += e->monthly_pay();
    return sum;
}

inline std::vector<std::string> badges(const Staff& staff) {
    std::vector<std::string> out;
    for (const auto& e : staff)
        if (e) out.push_back(e->badge());
    return out;
}

// ---- RTTI: asking what an object REALLY is ---------------------------------------------------------------------------------------------------
// dynamic_cast on a POINTER: a null pointer when the object is not (derived from) the target. It needs a polymorphic base (a virtual function).
inline const Manager* as_manager(const Employee& e) { return dynamic_cast<const Manager*>(&e); }

// dynamic_cast on a REFERENCE cannot return "nothing": it throws std::bad_cast.
inline Manager& must_be_manager(Employee& e) { return dynamic_cast<Manager&>(e); }

inline std::size_t count_managers(const Staff& staff) {
    std::size_t n = 0;
    for (const auto& e : staff)
        if (e && as_manager(*e)) ++n;
    return n;
}

// typeid compares the EXACT dynamic types: a Manager is-a Salaried, but they are not the same kind.
inline bool same_kind(const Employee& a, const Employee& b) { return typeid(a) == typeid(b); }
