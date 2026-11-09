// MUTANT: add_definitions(-Wall) is global and for preprocessor definitions.
cmake_minimum_required(VERSION 3.16)
project(greeter VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# the library
add_library(greeter src/greeter.cpp)
target_include_directories(greeter PUBLIC include)
add_definitions(-Wall -Wextra)

# the program
add_executable(app app/main.cpp)
target_link_libraries(app PRIVATE greeter)

# the tests
enable_testing()
find_package(GTest REQUIRED)
add_executable(greeter_tests tests/greeter_test.cpp)
target_link_libraries(greeter_tests PRIVATE greeter GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(greeter_tests)
