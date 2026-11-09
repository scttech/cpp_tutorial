// MUTANT: no enable_testing / test target.
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

