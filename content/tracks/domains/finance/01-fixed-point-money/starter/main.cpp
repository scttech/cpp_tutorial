// Money without floating point: a bill, a tax, a split among friends. Read-only. Press Run.
#include <cstdio>

#include "money.hpp"

int main() {
    using fin::Money;
    using fin::Rounding;
    // the classic: 0.1 + 0.2 in binary floating point is 0.30000000000000004
    std::printf("0.1 + 0.2 as double: %.17g\n", 0.1 + 0.2);
    const Money a = *Money::parse("0.10"), b = *Money::parse("0.20");
    std::printf("0.10 + 0.20 as Money: %s\n", fin::checked_add(a, b)->to_string().c_str());

    const Money bill = *Money::parse("$86.40");
    const Money tax = *bill.percent(825, Rounding::HalfUp);  // 8.25 %
    std::printf("bill %s, tax %s, total %s\n", bill.to_string().c_str(), tax.to_string().c_str(), fin::checked_add(bill, tax)->to_string().c_str());
    std::printf("split %s three ways:", bill.to_string().c_str());
    for (const Money& share : bill.allocate(3)) std::printf(" %s", share.to_string().c_str());
    std::printf("\nsplit $100.00 three ways:");
    for (const Money& share : Money::from_cents(10000).allocate(3)) std::printf(" %s", share.to_string().c_str());
    std::printf("\n2.5 cents rounds to %s (half up) and %s (half even)\n", Money::from_cents(5).divided(2, Rounding::HalfUp)->to_string().c_str(),
                Money::from_cents(5).divided(2, Rounding::HalfEven)->to_string().c_str());
    std::printf("overflow is noticed: %s\n", fin::checked_add(Money::from_cents(INT64_MAX), Money::from_cents(1)) ? "no (BUG)" : "yes");
}
