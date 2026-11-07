#define ZTH_TRACK_ALLOCATIONS
#include <string>

#include "zth_test.hpp"

namespace {
std::string run(const std::string& input) { return ZTH_RUN_USER_MAIN(input).out; }
std::string nl(std::initializer_list<const char*> lines) {
    std::string s;
    for (const char* l : lines) (s += l) += "\n";
    return s;
}
}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "add, then find") {
    ZTH_EXPECT_EQ(run("add widget 5 2.50\nfind widget\nquit\n"), nl({"added 5 widget (now 5)", "widget: 5 @ 2.50"}));
}
ZTH_TEST("works", "adding an existing item increases its quantity and updates the price") {
    ZTH_EXPECT_EQ(run("add widget 5 2.50\nadd widget 3 3\nfind widget\n"),
                  nl({"added 5 widget (now 5)", "added 3 widget (now 8)", "widget: 8 @ 3.00"}));
}
ZTH_TEST("works", "remove reduces the quantity") {
    ZTH_EXPECT_EQ(run("add widget 5 2.50\nremove widget 2\nfind widget\n"),
                  nl({"added 5 widget (now 5)", "removed 2 widget (now 3)", "widget: 3 @ 2.50"}));
}
ZTH_TEST("works", "removing everything deletes the item") {
    ZTH_EXPECT_EQ(run("add widget 2 1\nremove widget 2\nfind widget\n"),
                  nl({"added 2 widget (now 2)", "removed 2 widget (now 0)", "error: unknown item widget"}));
}
ZTH_TEST("works", "price changes the price of an existing item") {
    ZTH_EXPECT_EQ(run("add widget 2 1\nprice widget 4.5\nfind widget\n"),
                  nl({"added 2 widget (now 2)", "price of widget is now 4.50", "widget: 2 @ 4.50"}));
}
ZTH_TEST("works", "report lists the items sorted by name, with a total") {
    ZTH_EXPECT_EQ(run("add bolt 10 0.25\nadd apple 4 1.50\nreport\n"),
                  nl({"added 10 bolt (now 10)", "added 4 apple (now 4)", "apple x4 @ 1.50 = 6.00", "bolt x10 @ 0.25 = 2.50", "total: 8.50"}));
}
ZTH_TEST("works", "report of an empty inventory") {
    ZTH_EXPECT_EQ(run("report\n"), "inventory is empty\n");
}
ZTH_TEST("works", "an unknown command is reported") {
    ZTH_EXPECT_EQ(run("frobnicate now\n"), "error: unknown command frobnicate\n");
}
ZTH_TEST("works", "quit stops processing; so does the end of the input") {
    ZTH_EXPECT_EQ(run("add a 1 1\nquit\nadd b 1 1\n"), "added 1 a (now 1)\n");
    ZTH_EXPECT_EQ(run("add a 1 1\nadd b 1 1"), nl({"added 1 a (now 1)", "added 1 b (now 1)"}));
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "blank lines, extra spaces, tabs and Windows line endings are fine") {
    ZTH_EXPECT_EQ(run("\n   \n  add   widget \t 5   2.5  \r\nfind widget\r\n\n"),
                  nl({"added 5 widget (now 5)", "widget: 5 @ 2.50"}));
}
ZTH_TEST("robust", "wrong number of arguments gives a usage message (for every command)") {
    ZTH_EXPECT_EQ(run("add\nadd w 1\nadd w 1 1 extra\n"),
                  nl({"error: usage: add <name> <qty> <price>", "error: usage: add <name> <qty> <price>", "error: usage: add <name> <qty> <price>"}));
    ZTH_EXPECT_EQ(run("remove\nremove w\nremove w 1 2\n"),
                  nl({"error: usage: remove <name> <qty>", "error: usage: remove <name> <qty>", "error: usage: remove <name> <qty>"}));
    ZTH_EXPECT_EQ(run("price\nprice w\nprice w 1 2\n"),
                  nl({"error: usage: price <name> <price>", "error: usage: price <name> <price>", "error: usage: price <name> <price>"}));
    ZTH_EXPECT_EQ(run("find\nfind a b\n"), nl({"error: usage: find <name>", "error: usage: find <name>"}));
    ZTH_EXPECT_EQ(run("report now\n"), "error: usage: report\n");
    ZTH_EXPECT_EQ(run("quit now\nreport\n"), nl({"error: usage: quit", "inventory is empty"}));
}
ZTH_TEST("robust", "quantities must be whole numbers: junk, fractions and trailing characters are usage errors") {
    const char* usage = "error: usage: add <name> <qty> <price>";
    ZTH_EXPECT_EQ(run("add w 12x 1\nadd w 1.5 1\nadd w abc 1\nadd w 99999999999 1\n"), nl({usage, usage, usage, usage}));
}
ZTH_TEST("robust", "quantity must be positive") {
    ZTH_EXPECT_EQ(run("add w 0 1\nadd w -3 1\nadd w 5 1\nremove w 0\nremove w -1\n"),
                  nl({"error: quantity must be a positive integer", "error: quantity must be a positive integer", "added 5 w (now 5)",
                      "error: quantity must be a positive integer", "error: quantity must be a positive integer"}));
}
ZTH_TEST("robust", "prices: nan, inf, junk and out-of-range are usage errors; negative is its own error") {
    const char* usage = "error: usage: add <name> <qty> <price>";
    ZTH_EXPECT_EQ(run("add w 1 nan\nadd w 1 inf\nadd w 1 -inf\nadd w 1 2.5abc\nadd w 1 1e999\nadd w 1 abc\n"),
                  nl({usage, usage, usage, usage, usage, usage}));
    ZTH_EXPECT_EQ(run("add w 1 -0.01\nreport\n"), nl({"error: price must not be negative", "inventory is empty"}));
    ZTH_EXPECT_EQ(run("add w 1 1\nprice w nan\nprice w -2\nfind w\n"),
                  nl({"added 1 w (now 1)", "error: usage: price <name> <price>", "error: price must not be negative", "w: 1 @ 1.00"}));
}
ZTH_TEST("robust", "validation order: both numbers must parse, then quantity, then price") {
    ZTH_EXPECT_EQ(run("add w 0 abc\n"), "error: usage: add <name> <qty> <price>\n");
    ZTH_EXPECT_EQ(run("add w 0 -1\n"), "error: quantity must be a positive integer\n");
    ZTH_EXPECT_EQ(run("price nothere -1\n"), "error: price must not be negative\n");
}
ZTH_TEST("robust", "a price of zero is fine, and so is -0 (printed as 0.00, never -0.00)") {
    ZTH_EXPECT_EQ(run("add free 3 0\nadd also 2 -0\nreport\n"),
                  nl({"added 3 free (now 3)", "added 2 also (now 2)", "also x2 @ 0.00 = 0.00", "free x3 @ 0.00 = 0.00", "total: 0.00"}));
    ZTH_EXPECT_EQ(run("add w 1 5\nprice w -0\nfind w\n"), nl({"added 1 w (now 1)", "price of w is now 0.00", "w: 1 @ 0.00"}));
}
ZTH_TEST("robust", "quantity overflow is rejected and leaves the stock unchanged") {
    ZTH_EXPECT_EQ(run("add w 2147483647 1\nadd w 1 1\nfind w\n"),
                  nl({"added 2147483647 w (now 2147483647)", "error: quantity too large", "w: 2147483647 @ 1.00"}));
    ZTH_EXPECT_EQ(run("add w 2147483000 1\nadd w 647 2\nadd w 648 2\nfind w\n"),
                  nl({"added 2147483000 w (now 2147483000)", "added 647 w (now 2147483647)", "error: quantity too large", "w: 2147483647 @ 2.00"}));
}
ZTH_TEST("robust", "remove: unknown item, and not enough stock (nothing changes)") {
    ZTH_EXPECT_EQ(run("remove ghost 1\nadd w 3 1\nremove w 4\nfind w\n"),
                  nl({"error: unknown item ghost", "added 3 w (now 3)", "error: not enough w (have 3)", "w: 3 @ 1.00"}));
}
ZTH_TEST("robust", "price and find on an unknown item") {
    ZTH_EXPECT_EQ(run("price ghost 1\nfind ghost\n"), nl({"error: unknown item ghost", "error: unknown item ghost"}));
}
ZTH_TEST("robust", "names are case-sensitive and may contain punctuation; commands are case-sensitive too") {
    ZTH_EXPECT_EQ(run("add Widget 1 1\nadd widget 2 2\nadd m3-bolt 4 0.5\nfind Widget\nfind widget\nADD x 1 1\n"),
                  nl({"added 1 Widget (now 1)", "added 2 widget (now 2)", "added 4 m3-bolt (now 4)", "Widget: 1 @ 1.00", "widget: 2 @ 2.00", "error: unknown command ADD"}));
}
ZTH_TEST("robust", "money is rounded to two decimals; the total is the sum of the values") {
    ZTH_EXPECT_EQ(run("add a 3 0.1\nadd b 1 1234567.891\nreport\n"),
                  nl({"added 3 a (now 3)", "added 1 b (now 1)", "a x3 @ 0.10 = 0.30", "b x1 @ 1234567.89 = 1234567.89", "total: 1234568.19"}));
}
ZTH_TEST("robust", "an item can be removed and added again; the middle of the list can be removed") {
    ZTH_EXPECT_EQ(run("add a 1 1\nadd b 1 1\nadd c 1 1\nremove b 1\nreport\nadd b 5 2\nfind b\nfind a\nfind c\n"),
                  nl({"added 1 a (now 1)", "added 1 b (now 1)", "added 1 c (now 1)", "removed 1 b (now 0)", "a x1 @ 1.00 = 1.00",
                      "c x1 @ 1.00 = 1.00", "total: 2.00", "added 5 b (now 5)", "b: 5 @ 2.00", "a: 1 @ 1.00", "c: 1 @ 1.00"}));
}
ZTH_TEST("robust", "a thousand items: lookup, update and report stay correct") {
    std::string in;
    for (int i = 0; i < 1000; ++i) in += "add item" + std::to_string(i) + " " + std::to_string(i + 1) + " 1\n";
    for (int i = 0; i < 1000; i += 2) in += "remove item" + std::to_string(i) + " 1\n";
    in += "find item999\nfind item0\nreport\n";
    const std::string out = run(in);
    ZTH_EXPECT_CONTAINS(out, "item999: 1000 @ 1.00\n");
    ZTH_EXPECT_CONTAINS(out, "error: unknown item item0\n");  // item0 had 1 in stock; removing 1 deleted it
    ZTH_EXPECT_CONTAINS(out, "item2 x2 @ 1.00 = 2.00\n");      // item2 had 3; one was removed
}
ZTH_TEST("robust", "a very long name and a very long line are handled") {
    const std::string name(5000, 'n');
    ZTH_EXPECT_EQ(run("add " + name + " 1 1\nfind " + name + "\n"), "added 1 " + name + " (now 1)\n" + name + ": 1 @ 1.00\n");
}

// ---- Clean: no raw memory management ----------------------------------------------------------
ZTH_TEST("clean", "main.cpp does not use new, delete, malloc or free") {
    ZTH_EXPECT_NO_RAW_MEMORY("main.cpp");
}
ZTH_TEST("clean", "a typical session leaves no memory behind") {
    zth::mem::Scope m;
    {
        auto r = ZTH_RUN_USER_MAIN("add a 5 1.5\nadd b 2 3\nremove a 5\nprice b 9\nreport\nfind b\nbogus\nquit\n");
        (void)r;
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}
ZTH_TEST("clean", "a long session (many adds and removes) leaves no memory behind and frees what it erases") {
    std::string in;
    for (int i = 0; i < 300; ++i) in += "add item" + std::to_string(i) + " 1 1\n";
    for (int i = 0; i < 300; i += 3) in += "remove item" + std::to_string(i) + " 1\n";
    in += "report\n";
    zth::mem::Scope m;
    {
        auto r = ZTH_RUN_USER_MAIN(in);
        (void)r;
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}
