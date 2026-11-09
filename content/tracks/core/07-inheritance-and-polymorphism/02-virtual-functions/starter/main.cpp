// A demo of your payroll (read-only). Press Run to see what it prints.
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "payroll.hpp"

int main() {
    std::cout << std::fixed << std::setprecision(2);
    {
        Staff staff;
        staff.push_back(std::make_unique<Salaried>("Ada", 72000));
        staff.push_back(std::make_unique<Hourly>("Bob", 20, 170));  // 10 hours of overtime
        auto boss = std::make_unique<Manager>("Cy", 120000);
        boss->add_report();
        boss->add_report();
        staff.push_back(std::move(boss));

        for (const auto& e : staff) std::cout << std::left << std::setw(16) << e->badge() << std::right << std::setw(10) << e->monthly_pay() << "\n";
        std::cout << "total: " << total_payroll(staff) << "\n";
        std::cout << "managers: " << count_managers(staff) << "\n";
        std::cout << "Ada and Cy the same kind? " << same_kind(*staff[0], *staff[2]) << "\n";

        try {
            must_be_manager(*staff[0]);
        } catch (const std::bad_cast&) {
            std::cout << "Ada is not a manager (std::bad_cast)\n";
        }
        std::cout << "-- the staff goes out of scope --\n";
    }
    std::cout << "the destructor trace:\n";
    for (const std::string& line : trace_log()) std::cout << "  " << line << "\n";
}
