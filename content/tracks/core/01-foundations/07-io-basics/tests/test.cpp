#include <string>

#include "zth_test.hpp"

// The learner's whole program is exercised through its stdin and stdout.
static std::string run(const std::string& input) {
    return ZTH_RUN_USER_MAIN(input).out;
}

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "a single valid line") {
    ZTH_EXPECT_EQ(run("Ada 36\n"), "Ada is 36 years old\nValid: 1, Invalid: 0\n");
}
ZTH_TEST("works", "several valid lines are processed in order") {
    ZTH_EXPECT_EQ(run("Ada 36\nBob 4\nCy 99\n"),
                  "Ada is 36 years old\nBob is 4 years old\nCy is 99 years old\nValid: 3, Invalid: 0\n");
}
ZTH_TEST("works", "names may contain spaces (the age is the last token)") {
    ZTH_EXPECT_EQ(run("Ada Lovelace 36\n"), "Ada Lovelace is 36 years old\nValid: 1, Invalid: 0\n");
}
ZTH_TEST("works", "a line with no age is invalid and echoed back") {
    ZTH_EXPECT_EQ(run("Bob\n"), "Invalid line: Bob\nValid: 0, Invalid: 1\n");
}
ZTH_TEST("works", "a non-numeric age is invalid") {
    ZTH_EXPECT_EQ(run("Carol x\n"), "Invalid line: Carol x\nValid: 0, Invalid: 1\n");
}
ZTH_TEST("works", "no input at all prints just the summary") {
    ZTH_EXPECT_EQ(run(""), "Valid: 0, Invalid: 0\n");
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "the last line is processed even without a trailing newline") {
    ZTH_EXPECT_EQ(run("Ada 36\nBob 4"), "Ada is 36 years old\nBob is 4 years old\nValid: 2, Invalid: 0\n");
}
ZTH_TEST("robust", "blank and whitespace-only lines are ignored (not counted)") {
    ZTH_EXPECT_EQ(run("\n   \nAda 36\n\t\n"), "Ada is 36 years old\nValid: 1, Invalid: 0\n");
}
ZTH_TEST("robust", "an age with trailing junk like 30x is invalid") {
    ZTH_EXPECT_EQ(run("Dan 30x\n"), "Invalid line: Dan 30x\nValid: 0, Invalid: 1\n");
}
ZTH_TEST("robust", "a decimal or negative age is invalid") {
    ZTH_EXPECT_EQ(run("Eve 3.5\nFay -2\n"),
                  "Invalid line: Eve 3.5\nInvalid line: Fay -2\nValid: 0, Invalid: 2\n");
}
ZTH_TEST("robust", "ages outside 0..150 are invalid, the limits are valid") {
    ZTH_EXPECT_EQ(run("Old 151\nBorn 0\nMax 150\n"),
                  "Invalid line: Old 151\nBorn is 0 years old\nMax is 150 years old\nValid: 2, Invalid: 1\n");
}
ZTH_TEST("robust", "an absurdly large number does not crash and is invalid") {
    ZTH_EXPECT_EQ(run("Big 99999999999999999999\n"), "Invalid line: Big 99999999999999999999\nValid: 0, Invalid: 1\n");
}
ZTH_TEST("robust", "a line with only a number has no name and is invalid") {
    ZTH_EXPECT_EQ(run("42\n"), "Invalid line: 42\nValid: 0, Invalid: 1\n");
}
ZTH_TEST("robust", "extra spaces around the name are trimmed from the output") {
    ZTH_EXPECT_EQ(run("   Ada   36  \n"), "Ada is 36 years old\nValid: 1, Invalid: 0\n");
}
ZTH_TEST("robust", "an invalid line does not stop later lines being read") {
    ZTH_EXPECT_EQ(run("oops\nAda 36\n"), "Invalid line: oops\nAda is 36 years old\nValid: 1, Invalid: 1\n");
}
