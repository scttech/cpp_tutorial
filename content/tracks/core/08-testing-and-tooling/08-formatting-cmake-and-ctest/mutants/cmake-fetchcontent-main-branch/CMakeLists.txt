// MUTANT: googletest fetched from a moving branch.
cmake_minimum_required(VERSION 3.16)
project(greeter VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
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
include(FetchContent)
FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG main)
FetchContent_MakeAvailable(googletest)
add_executable(greeter_tests tests/greeter_test.cpp)
target_link_libraries(greeter_tests PRIVATE greeter GTest::gtest_main)
target_compile_options(greeter_tests PRIVATE -Wall -Wextra -Wpedantic)
include(GoogleTest)
gtest_discover_tests(greeter_tests)
