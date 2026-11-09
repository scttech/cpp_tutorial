// MUTANT: -std=c++20 as a compile option instead of CMAKE_CXX_STANDARD.
cmake_minimum_required(VERSION 3.16)
project(greeter VERSION 1.0 LANGUAGES CXX)

add_compile_options(-std=c++20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# the library
add_library(greeter src/greeter.cpp)
target_include_directories(greeter PUBLIC include)
target_compile_options(greeter PRIVATE -Wall -Wextra -Wpedantic)

# the program
add_executable(app app/main.cpp)
target_link_libraries(app PRIVATE greeter)
target_compile_options(app PRIVATE -Wall -Wextra -Wpedantic)

# the tests
enable_testing()
find_package(GTest REQUIRED)
add_executable(greeter_tests tests/greeter_test.cpp)
target_link_libraries(greeter_tests PRIVATE greeter GTest::gtest_main)
target_compile_options(greeter_tests PRIVATE -Wall -Wextra -Wpedantic)
include(GoogleTest)
gtest_discover_tests(greeter_tests)
