#include <string>

#include "zth_test.hpp"

static std::string run(const std::string& input) {
    return ZTH_RUN_USER_MAIN(input).out;
}

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "length: 1 km is 1000 m") {
    ZTH_EXPECT_EQ(run("convert 1 km m\n"), "1 km = 1000.0000 m\n");
}
ZTH_TEST("works", "length: 100 km in miles") {
    ZTH_EXPECT_EQ(run("convert 100 km mi\n"), "100 km = 62.1371 mi\n");
}
ZTH_TEST("works", "length: feet to meters and miles to feet") {
    ZTH_EXPECT_EQ(run("convert 3 ft m\nconvert 1 mi ft\n"), "3 ft = 0.9144 m\n1 mi = 5280.0000 ft\n");
}
ZTH_TEST("works", "mass: pounds to kilograms and kilograms to grams") {
    ZTH_EXPECT_EQ(run("convert 10 lb kg\nconvert 2.5 kg g\n"), "10 lb = 4.5359 kg\n2.5 kg = 2500.0000 g\n");
}
ZTH_TEST("works", "temperature: Celsius to Fahrenheit and Kelvin") {
    ZTH_EXPECT_EQ(run("convert 100 C F\nconvert 0 C K\n"), "100 C = 212.0000 F\n0 C = 273.1500 K\n");
}
ZTH_TEST("works", "temperature: Fahrenheit to Celsius and Kelvin to Celsius") {
    ZTH_EXPECT_EQ(run("convert 32 F C\nconvert 300 K C\n"), "32 F = 0.0000 C\n300 K = 26.8500 C\n");
}
ZTH_TEST("works", "negative values: -40 is the same in C and F") {
    ZTH_EXPECT_EQ(run("convert -40 C F\nconvert -40 F C\n"), "-40 C = -40.0000 F\n-40 F = -40.0000 C\n");
}
ZTH_TEST("works", "the value is echoed exactly as typed") {
    ZTH_EXPECT_EQ(run("convert 1.50 km m\n"), "1.50 km = 1500.0000 m\n");
}
ZTH_TEST("works", "converting a unit to itself works") {
    ZTH_EXPECT_EQ(run("convert 7 m m\n"), "7 m = 7.0000 m\n");
}
ZTH_TEST("works", "quit stops processing") {
    ZTH_EXPECT_EQ(run("convert 1 km m\nquit\nconvert 2 km m\n"), "1 km = 1000.0000 m\n");
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "unknown unit (either side), and units are case-sensitive") {
    ZTH_EXPECT_EQ(run("convert 5 km parsec\n"), "error: unknown unit 'parsec'\n");
    ZTH_EXPECT_EQ(run("convert 5 furlong m\n"), "error: unknown unit 'furlong'\n");
    ZTH_EXPECT_EQ(run("convert 1 KM m\n"), "error: unknown unit 'KM'\n");
}
ZTH_TEST("robust", "units of different dimensions cannot be converted") {
    ZTH_EXPECT_EQ(run("convert 5 km kg\n"), "error: cannot convert km to kg\n");
    ZTH_EXPECT_EQ(run("convert 5 C m\n"), "error: cannot convert C to m\n");
}
ZTH_TEST("robust", "invalid numbers: letters, trailing junk, nan, inf, empty exponent") {
    ZTH_EXPECT_EQ(run("convert abc km m\n"), "error: invalid number 'abc'\n");
    ZTH_EXPECT_EQ(run("convert 5x km m\n"), "error: invalid number '5x'\n");
    ZTH_EXPECT_EQ(run("convert nan km m\n"), "error: invalid number 'nan'\n");
    ZTH_EXPECT_EQ(run("convert inf km m\n"), "error: invalid number 'inf'\n");
    ZTH_EXPECT_EQ(run("convert -inf km m\n"), "error: invalid number '-inf'\n");
    ZTH_EXPECT_EQ(run("convert 1,5 km m\n"), "error: invalid number '1,5'\n");
}
ZTH_TEST("robust", "the number is checked before the units") {
    ZTH_EXPECT_EQ(run("convert abc parsec furlong\n"), "error: invalid number 'abc'\n");
}
ZTH_TEST("robust", "wrong argument count prints the usage line") {
    ZTH_EXPECT_EQ(run("convert\n"), "error: usage: convert <value> <from> <to>\n");
    ZTH_EXPECT_EQ(run("convert 5 km\n"), "error: usage: convert <value> <from> <to>\n");
    ZTH_EXPECT_EQ(run("convert 5 km m extra\n"), "error: usage: convert <value> <from> <to>\n");
}
ZTH_TEST("robust", "unknown commands are reported") {
    ZTH_EXPECT_EQ(run("frobnicate 1 2\n"), "error: unknown command 'frobnicate'\n");
    ZTH_EXPECT_EQ(run("Convert 1 km m\n"), "error: unknown command 'Convert'\n");
}
ZTH_TEST("robust", "an error does not stop later commands from running") {
    ZTH_EXPECT_EQ(run("convert 5 km kg\nconvert 1 km m\nbogus\nconvert 2 km m\n"),
                  "error: cannot convert km to kg\n1 km = 1000.0000 m\nerror: unknown command 'bogus'\n2 km = 2000.0000 m\n");
}
ZTH_TEST("robust", "blank lines and extra whitespace are tolerated") {
    ZTH_EXPECT_EQ(run("\n   \n   convert    5   km   m   \n\t\n"), "5 km = 5000.0000 m\n");
}
ZTH_TEST("robust", "the last command is processed without a trailing newline") {
    ZTH_EXPECT_EQ(run("convert 1 km m"), "1 km = 1000.0000 m\n");
}
ZTH_TEST("robust", "empty input produces no output") {
    ZTH_EXPECT_EQ(run(""), "");
}
ZTH_TEST("robust", "below absolute zero is an error, but exactly absolute zero is fine") {
    ZTH_EXPECT_EQ(run("convert -300 C K\n"), "error: below absolute zero\n");
    ZTH_EXPECT_EQ(run("convert -1 K C\n"), "error: below absolute zero\n");
    ZTH_EXPECT_EQ(run("convert -273.15 C K\n"), "-273.15 C = 0.0000 K\n");
    ZTH_EXPECT_EQ(run("convert 0 K C\n"), "0 K = -273.1500 C\n");
}
ZTH_TEST("robust", "a result that rounds to zero prints 0.0000, never -0.0000") {
    ZTH_EXPECT_EQ(run("convert -0.000000001 m km\n"), "-0.000000001 m = 0.0000 km\n");
}
ZTH_TEST("robust", "negative lengths are allowed (only temperatures have an absolute zero)") {
    ZTH_EXPECT_EQ(run("convert -5 km m\n"), "-5 km = -5000.0000 m\n");
}
ZTH_TEST("robust", "very small and very large values convert without crashing") {
    ZTH_EXPECT_EQ(run("convert 0.001 km m\n"), "0.001 km = 1.0000 m\n");
    ZTH_EXPECT_EQ(run("convert 1000000 km m\n"), "1000000 km = 1000000000.0000 m\n");
}
ZTH_TEST("robust", "scientific notation is accepted as a number") {
    ZTH_EXPECT_EQ(run("convert 1e3 m m\n"), "1e3 m = 1000.0000 m\n");
}
