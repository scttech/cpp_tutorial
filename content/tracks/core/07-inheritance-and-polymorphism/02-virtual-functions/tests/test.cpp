#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include "payroll.hpp"
#include "zth_test.hpp"

namespace {

using Log = std::vector<std::string>;
using Names = std::vector<std::string>;

template <class F>
bool throws_invalid(F f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

Staff sample_staff() {
    Staff s;
    s.push_back(std::make_unique<Salaried>("Ada", 72000));  // 6000
    s.push_back(std::make_unique<Hourly>("Bob", 20, 100));  // 2000
    auto m = std::make_unique<Manager>("Cy", 120000);       // 10000 + 2 * 250
    m->add_report();
    m->add_report();
    s.push_back(std::move(m));
    return s;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "Salaried: a twelfth of the annual salary, title salaried") {
    Salaried s("Ada", 72000);
    ZTH_EXPECT_NEAR(s.monthly_pay(), 6000, 1e-9);
    ZTH_EXPECT_EQ(s.title(), std::string("salaried"));
    ZTH_EXPECT_EQ(s.name(), std::string("Ada"));
}
ZTH_TEST("works", "Hourly: rate times hours, title hourly") {
    Hourly h("Bob", 20, 100);
    ZTH_EXPECT_NEAR(h.monthly_pay(), 2000, 1e-9);
    ZTH_EXPECT_EQ(h.title(), std::string("hourly"));
}
ZTH_TEST("works", "Hourly: overtime beyond 160 hours is paid at 1.5 times") {
    Hourly h("Bob", 20, 170);
    ZTH_EXPECT_NEAR(h.monthly_pay(), 20 * 160 + 30 * 10, 1e-9);
}
ZTH_TEST("works", "Manager: the salaried pay plus 250 per report, title manager") {
    Manager m("Cy", 120000);
    ZTH_EXPECT_NEAR(m.monthly_pay(), 10000, 1e-9);
    m.add_report();
    m.add_report();
    m.add_report();
    ZTH_EXPECT_EQ(m.reports(), 3);
    ZTH_EXPECT_NEAR(m.monthly_pay(), 10750, 1e-9);
    ZTH_EXPECT_EQ(m.title(), std::string("manager"));
}
ZTH_TEST("works", "calls through an Employee reference reach the real class (virtual dispatch)") {
    Hourly h("Bob", 20, 100);
    Salaried s("Ada", 72000);
    Manager m("Cy", 120000);
    const Employee& eh = h;
    const Employee& es = s;
    const Employee& em = m;
    ZTH_EXPECT_NEAR(eh.monthly_pay(), 2000, 1e-9);
    ZTH_EXPECT_NEAR(es.monthly_pay(), 6000, 1e-9);
    ZTH_EXPECT_NEAR(em.monthly_pay(), 10000, 1e-9);
    ZTH_EXPECT_EQ(eh.title(), std::string("hourly"));
    ZTH_EXPECT_EQ(es.title(), std::string("salaried"));
    ZTH_EXPECT_EQ(em.title(), std::string("manager"));
}
ZTH_TEST("works", "a plain Employee: no pay, title employee") {
    Employee e("Zed");
    ZTH_EXPECT_NEAR(e.monthly_pay(), 0, 1e-12);
    ZTH_EXPECT_EQ(e.title(), std::string("employee"));
    ZTH_EXPECT_EQ(e.badge(), std::string("Zed, employee"));
}
ZTH_TEST("works", "badge: name and title of the REAL class") {
    const Staff s = sample_staff();
    ZTH_EXPECT_EQ(s[0]->badge(), std::string("Ada, salaried"));
    ZTH_EXPECT_EQ(s[1]->badge(), std::string("Bob, hourly"));
    ZTH_EXPECT_EQ(s[2]->badge(), std::string("Cy, manager"));
}
ZTH_TEST("works", "total_payroll adds up the staff") {
    ZTH_EXPECT_NEAR(total_payroll(sample_staff()), 6000 + 2000 + 10500, 1e-9);
}
ZTH_TEST("works", "badges lists every badge in order") {
    ZTH_EXPECT_TRUE((badges(sample_staff()) == Names{"Ada, salaried", "Bob, hourly", "Cy, manager"}));
}
ZTH_TEST("works", "as_manager finds a Manager and nothing else") {
    Manager m("Cy", 1);
    Salaried s("Ada", 1);
    Hourly h("Bob", 1, 1);
    const Employee& em = m;
    ZTH_EXPECT_TRUE(as_manager(em) == &m);
    ZTH_EXPECT_TRUE(as_manager(s) == nullptr);
    ZTH_EXPECT_TRUE(as_manager(h) == nullptr);
}
ZTH_TEST("works", "count_managers") {
    Staff s = sample_staff();
    s.push_back(std::make_unique<Manager>("Di", 1000));
    ZTH_EXPECT_EQ(count_managers(s), std::size_t{2});
}
ZTH_TEST("works", "must_be_manager returns the very same object, or throws std::bad_cast") {
    Manager m("Cy", 1000);
    Employee& e = m;
    ZTH_EXPECT_TRUE(&must_be_manager(e) == &m);
    Salaried s("Ada", 1000);
    ZTH_EXPECT_THROWS(std::bad_cast, must_be_manager(s));
}
ZTH_TEST("works", "same_kind compares the exact dynamic types") {
    Salaried s1("A", 1), s2("B", 1);
    Manager m("C", 1);
    Hourly h("D", 1, 1);
    ZTH_EXPECT_TRUE(same_kind(s1, s2));
    ZTH_EXPECT_FALSE(same_kind(s1, m));  // a Manager is-a Salaried, but not the same kind
    ZTH_EXPECT_FALSE(same_kind(s1, h));
    ZTH_EXPECT_TRUE(same_kind(m, m));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "the Employee destructor is virtual") {
    ZTH_EXPECT_TRUE(std::has_virtual_destructor_v<Employee>);
    ZTH_EXPECT_TRUE(std::is_polymorphic_v<Employee>);
}
ZTH_TEST("robust", "deleting a Manager through an Employee pointer runs every destructor, derived first") {
    trace_log().clear();
    {
        std::unique_ptr<Employee> e = std::make_unique<Manager>("Ann", 1000);
    }
    ZTH_EXPECT_TRUE((trace_log() == Log{"~Manager(Ann)", "~Salaried(Ann)", "~Employee(Ann)"}));
}
ZTH_TEST("robust", "deleting an Hourly (through an Employee pointer) runs ~Hourly then ~Employee") {
    trace_log().clear();
    {
        std::unique_ptr<Employee> e = std::make_unique<Hourly>("Bo", 10, 10);
    }
    ZTH_EXPECT_TRUE((trace_log() == Log{"~Hourly(Bo)", "~Employee(Bo)"}));
}
ZTH_TEST("robust", "a vector of employees destroys each one completely, in order") {
    trace_log().clear();
    {
        Staff s;
        s.push_back(std::make_unique<Salaried>("A", 1));
        s.push_back(std::make_unique<Hourly>("B", 1, 1));
    }
    ZTH_EXPECT_TRUE((trace_log() == Log{"~Salaried(A)", "~Employee(A)", "~Hourly(B)", "~Employee(B)"}));
}
ZTH_TEST("robust", "a Manager's badge is computed when asked, not when it was constructed") {
    Manager m("Cy", 1000);
    const Employee& e = m;
    ZTH_EXPECT_EQ(e.badge(), std::string("Cy, manager"));
}
ZTH_TEST("robust", "Manager and Hourly are final; Salaried is not") {
    ZTH_EXPECT_TRUE(std::is_final_v<Manager>);
    ZTH_EXPECT_TRUE(std::is_final_v<Hourly>);
    ZTH_EXPECT_FALSE(std::is_final_v<Salaried>);
}
ZTH_TEST("robust", "Hourly: exactly 160 hours has no overtime; 161 has one hour at 1.5") {
    ZTH_EXPECT_NEAR(Hourly("a", 10, 160).monthly_pay(), 1600, 1e-9);
    ZTH_EXPECT_NEAR(Hourly("a", 10, 161).monthly_pay(), 1600 + 15, 1e-9);
    ZTH_EXPECT_NEAR(Hourly("a", 10, 0).monthly_pay(), 0, 1e-9);
    ZTH_EXPECT_NEAR(Hourly("a", 10, 159.5).monthly_pay(), 1595, 1e-9);
}
ZTH_TEST("robust", "Manager: pay grows with each report, through a base reference") {
    Manager m("Cy", 12000);
    Employee& e = m;
    ZTH_EXPECT_NEAR(e.monthly_pay(), 1000, 1e-9);
    m.add_report();
    ZTH_EXPECT_NEAR(e.monthly_pay(), 1250, 1e-9);
    ZTH_EXPECT_EQ(m.reports(), 1);
}
ZTH_TEST("robust", "negative or NaN salaries, rates and hours are rejected") {
    ZTH_EXPECT_TRUE(throws_invalid([] { Salaried s("A", -1); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Salaried s("A", std::numeric_limits<double>::quiet_NaN()); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Manager m("A", -1); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Hourly h("A", -1, 10); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Hourly h("A", 10, -1); }));
    ZTH_EXPECT_NEAR(Salaried("A", 0).monthly_pay(), 0, 1e-12);
}
ZTH_TEST("robust", "total_payroll, badges and count_managers of an empty staff, and with null entries") {
    const Staff none;
    ZTH_EXPECT_NEAR(total_payroll(none), 0, 1e-12);
    ZTH_EXPECT_TRUE(badges(none).empty());
    ZTH_EXPECT_EQ(count_managers(none), std::size_t{0});
    Staff s;
    s.push_back(nullptr);
    s.push_back(std::make_unique<Salaried>("A", 1200));
    s.push_back(nullptr);
    ZTH_EXPECT_NEAR(total_payroll(s), 100, 1e-9);
    ZTH_EXPECT_TRUE((badges(s) == Names{"A, salaried"}));
    ZTH_EXPECT_EQ(count_managers(s), std::size_t{0});
}
ZTH_TEST("robust", "same_kind of two Hourly employees with different data is true") {
    Hourly a("A", 1, 1), b("B", 99, 99);
    ZTH_EXPECT_TRUE(same_kind(a, b));
}

// ================================================ Clean =======================================================
// The tests below read the SOURCE: `override` is not optional style, it is a safety net (a misspelled or const-less "override" compiles
// silently without it).
ZTH_TEST("clean", "every overriding function and destructor is marked override: payroll.hpp uses it at least 9 times") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("payroll.hpp"));
    const std::size_t n = ::zth::find_word(code, "override").size();
    if (n < 9) {
        ::zth::fail("payroll.hpp uses `override` " + std::to_string(n) + " time(s), but Salaried, Hourly and Manager each override a destructor, monthly_pay and title (9)");
        return;
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("clean", "`virtual` appears only in the base class (3 times: the destructor, monthly_pay and title)") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("payroll.hpp"));
    const std::size_t n = ::zth::find_word(code, "virtual").size();
    if (n != 3) {
        ::zth::fail("payroll.hpp uses `virtual` " + std::to_string(n) + " time(s); the base class declares three virtual functions, and an override needs only `override`");
        return;
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("clean", "payroll.hpp has no `using namespace`") {
    ZTH_EXPECT_FALSE(::zth::source_layout_of("payroll.hpp").using_directive);
}
