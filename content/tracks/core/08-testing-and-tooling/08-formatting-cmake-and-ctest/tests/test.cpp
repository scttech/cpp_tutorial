#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <string>

#include "zth_test.hpp"

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// The file without its `#` comments, in lower case (CMake command names are case-insensitive).
std::string without_comments(const char* file) {
    std::istringstream in(::zth::read_file(file));
    std::string out;
    for (std::string line; std::getline(in, line);) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        out += line + "\n";
    }
    return lower(out);
}

std::string cmake() { return without_comments("CMakeLists.txt"); }
std::string format() { return without_comments("format.yaml"); }

bool has(const std::string& text, const char* pattern) { return std::regex_search(text, std::regex(pattern)); }

// Every target_link_libraries(<target> <KEYWORD> ...) has PRIVATE, PUBLIC or INTERFACE right after the target.
bool all_links_have_a_keyword(const std::string& text) {
    const std::regex re(R"(target_link_libraries\s*\(\s*[\w:]+\s+(\w+))");
    bool any = false;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), re); it != std::sregex_iterator(); ++it) {
        any = true;
        const std::string word = (*it)[1];
        if (word != "private" && word != "public" && word != "interface") return false;
    }
    return any;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "CMakeLists.txt asks for a modern CMake: cmake_minimum_required(VERSION 3.x)") {
    ZTH_EXPECT_TRUE(has(cmake(), R"(cmake_minimum_required\s*\(\s*version\s+3\.\d+)"));
}
ZTH_TEST("works", "the project is called greeter and uses C++ only: project(greeter ... LANGUAGES CXX)") {
    const std::string c = cmake();
    ZTH_EXPECT_TRUE(has(c, R"(project\s*\(\s*greeter\b[^)]*languages\s+cxx)"));
}
ZTH_TEST("works", "the library target: add_library(greeter src/greeter.cpp)") {
    ZTH_EXPECT_TRUE(has(cmake(), R"(add_library\s*\(\s*greeter\b[^)]*src/greeter\.cpp)"));
}
ZTH_TEST("works", "the program target: add_executable(app app/main.cpp), linked with the library") {
    const std::string c = cmake();
    ZTH_EXPECT_TRUE(has(c, R"(add_executable\s*\(\s*app\b[^)]*app/main\.cpp)"));
    ZTH_EXPECT_TRUE(has(c, R"(target_link_libraries\s*\(\s*app\b[^)]*\bgreeter\b)"));
}
ZTH_TEST("works", "tests are enabled: enable_testing() and a greeter_tests executable built from tests/greeter_test.cpp") {
    const std::string c = cmake();
    ZTH_EXPECT_TRUE(has(c, R"(enable_testing\s*\(\s*\))"));
    ZTH_EXPECT_TRUE(has(c, R"(add_executable\s*\(\s*greeter_tests\b[^)]*tests/greeter_test\.cpp)"));
}
ZTH_TEST("works", "ctest knows the tests: gtest_discover_tests(greeter_tests) or add_test(...)") {
    const std::string c = cmake();
    ZTH_EXPECT_TRUE(has(c, R"(gtest_discover_tests\s*\(\s*greeter_tests)") || has(c, R"(add_test\s*\()"));
}
ZTH_TEST("works", "GoogleTest is found (find_package(GTest)) or fetched (FetchContent), and the tests link its main") {
    const std::string c = cmake();
    ZTH_EXPECT_TRUE(has(c, R"(find_package\s*\(\s*gtest)") || has(c, R"(fetchcontent_declare\s*\(\s*googletest)"));
    ZTH_EXPECT_TRUE(has(c, R"(target_link_libraries\s*\(\s*greeter_tests\b[^)]*gtest_main)"));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "the language standard is a CMake setting, not a compiler flag: CMAKE_CXX_STANDARD 17/20/23 and CMAKE_CXX_STANDARD_REQUIRED ON") {
    const std::string c = cmake();
    ZTH_EXPECT_TRUE(has(c, R"(set\s*\(\s*cmake_cxx_standard\s+(17|20|23))"));
    ZTH_EXPECT_TRUE(has(c, R"(set\s*\(\s*cmake_cxx_standard_required\s+on)"));
    ZTH_EXPECT_FALSE(has(c, R"(-std=)"));
}
ZTH_TEST("robust", "the include directory belongs to the library and is passed on to its users: target_include_directories(greeter PUBLIC include)") {
    ZTH_EXPECT_TRUE(has(cmake(), R"(target_include_directories\s*\(\s*greeter\s+public\s+include)"));
}
ZTH_TEST("robust", "every target_link_libraries says PRIVATE, PUBLIC or INTERFACE") {
    ZTH_EXPECT_TRUE(all_links_have_a_keyword(cmake()));
}
ZTH_TEST("robust", "warnings are options of the targets: target_compile_options(... -Wall -Wextra ...)") {
    const std::string c = cmake();
    ZTH_EXPECT_TRUE(has(c, R"(target_compile_options\s*\([^)]*-wall[^)]*-wextra)"));
    ZTH_EXPECT_FALSE(has(c, R"(add_definitions\s*\()"));
}
ZTH_TEST("robust", "none of the legacy global commands: include_directories, link_libraries, link_directories, add_definitions") {
    const std::string c = cmake();
    ZTH_EXPECT_FALSE(has(c, R"((^|[^_\w])include_directories\s*\()"));
    ZTH_EXPECT_FALSE(has(c, R"((^|[^_\w])link_libraries\s*\()"));
    ZTH_EXPECT_FALSE(has(c, R"(link_directories\s*\()"));
    ZTH_EXPECT_FALSE(has(c, R"(add_definitions\s*\()"));
}
ZTH_TEST("robust", "sources are listed by name, not collected with file(GLOB): a new file must show up in the diff") {
    ZTH_EXPECT_FALSE(has(cmake(), R"(file\s*\(\s*glob)"));
}
ZTH_TEST("robust", "CMake writes compile_commands.json for clang-tidy: CMAKE_EXPORT_COMPILE_COMMANDS ON") {
    ZTH_EXPECT_TRUE(has(cmake(), R"(set\s*\(\s*cmake_export_compile_commands\s+on)"));
}
ZTH_TEST("robust", "a fetched GoogleTest is pinned to a release, never to a moving branch") {
    const std::string c = cmake();
    if (has(c, R"(fetchcontent_declare)")) {
        ZTH_EXPECT_FALSE(has(c, R"(git_tag\s+(main|master|head)\b)"));
        ZTH_EXPECT_TRUE(has(c, R"(git_tag\s+\S+)") || has(c, R"(url\s+\S+)"));
    }
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "format.yaml builds on a base style: BasedOnStyle") {
    ZTH_EXPECT_TRUE(has(format(), R"(basedonstyle\s*:\s*\w+)"));
}
ZTH_TEST("clean", "the formatter is not disabled: no DisableFormat: true") {
    ZTH_EXPECT_FALSE(has(format(), R"(disableformat\s*:\s*true)"));
}
ZTH_TEST("clean", "lines are limited to a sensible length: ColumnLimit between 60 and 200 (0 would let clang-format leave any wrapping alone)") {
    std::smatch m;
    const std::string f = format();
    ZTH_EXPECT_TRUE(std::regex_search(f, m, std::regex(R"(columnlimit\s*:\s*(\d+))")));
    if (m.size() > 1) {
        const int limit = std::stoi(m[1]);
        ZTH_EXPECT_TRUE(limit >= 60 && limit <= 200);
    }
}
