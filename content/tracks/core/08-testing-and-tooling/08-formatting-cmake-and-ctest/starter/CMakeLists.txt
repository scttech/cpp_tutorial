# TASK: the build of a small project with this layout (the files exist in your imagination; CMake is only the recipe):
#
#   include/greeter/greeter.hpp     the public header of the library
#   src/greeter.cpp                 the library "greeter"
#   app/main.cpp                    the program "app", which uses the library
#   tests/greeter_test.cpp          GoogleTest tests of the library
#
# Below is a CMakeLists.txt written in the old style of ten years ago. Rewrite it in the modern, TARGET-BASED style described in the
# lesson: every property belongs to a target. Run Tests checks the structure of your file (comments do not count).

cmake_minimum_required(VERSION 2.8)
project(greeter)

include_directories(include)
add_definitions(-Wall -Wextra -std=c++17)

file(GLOB SOURCES src/*.cpp)
add_library(greeter ${SOURCES})

add_executable(app app/main.cpp)
link_libraries(greeter)
