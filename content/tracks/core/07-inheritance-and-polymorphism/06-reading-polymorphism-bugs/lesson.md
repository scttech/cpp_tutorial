# Debug challenge: Reading Polymorphism Bugs

Inheritance bugs come in three flavors: the compiler **refuses** (an *error*), the compiler **warns** and builds a program that is subtly wrong (a *warning*, which in this area is almost always a real bug), or the compiler says **nothing at all** and only a test, or a user, notices. This challenge has one of each kind, and a few in between.

This lesson **does not compile**. `band.hpp` and `main.cpp` define a small band of instruments with **six mistakes**. Your job is to read what the compiler (and the tests) tell you, not to guess.

## How to read these messages

1. **Read the first error only**, fix it, and run again. Later errors are often consequences of it (a class that is abstract breaks everything that creates one).
2. **Read the notes under the error.** For polymorphism they carry the answer: *`unimplemented pure virtual method 'sound' in 'Piano'`* names the function you forgot; *`hidden overloaded virtual function 'Instrument::sound' declared here: different qualifiers ('const' vs unqualified)`* says exactly how your signature differs.
3. **Errors reported inside `/usr/include/...`** (for example *allocating an object of abstract class type* inside `std::make_unique`) are found there because the library is the one creating the object; the cause is in *your* class. Follow the `in instantiation of ... requested here` note to your own file.
4. **Take warnings seriously here.** `-Woverloaded-virtual` ("hides overloaded virtual function") and `-Wdelete-non-abstract-non-virtual-dtor` ("delete called on ... non-virtual destructor") do not stop the build, and each marks a bug that the tests will find.
5. **Some bugs have no message at all.** *Object slicing* compiles silently. When a program produces the wrong answer and the compiler is quiet, ask: *is a derived object being copied into a base-class variable or parameter?*

## The six mistakes (in the order of the file; do not read this before you try)

| | where | what | how it shows |
|---|---|---|---|
| 1 | `Guitar::sound` | `override` without `const`: not the same signature | **error**: *non-virtual member function marked 'override' hides virtual member function* |
| 2 | `Piano` | forgets to implement a pure virtual function, so it is still abstract | **error**: *allocating an object of abstract class type* (inside `make_unique`) |
| 3 | `Whistle` | `class Whistle : Instrument` inherits **privately** (the default for `class`) | **error**: *cannot cast 'Whistle' to its private base class* |
| 4 | `Instrument` | the destructor is not `virtual` | **warning**: *delete called on ... non-virtual destructor*; the drumhead leaks (a test counts) |
| 5 | `Drum::play` | the parameter is a `double`, but the base declares `play(int)`: it **hides** instead of overriding | **warning**: *hides overloaded virtual function*; a call through an `Instrument&` plays the wrong rhythm |
| 6 | `hear` | takes the `Drum` **by value**: the argument is sliced | **nothing**; `snare_demo()` gives `boom` instead of `crack` |

## Your task

Make the program compile **without warnings**, and every test pass. `main.cpp` contains `whistle_demo` and `snare_demo`, which the tests call, and a `main` that prints a small report: press **Run** to see it.

The tiers: **Works** (every instrument and the band behave, through base references) and **Robust** (the destruction counters: nothing leaks, copies are counted; `hear` really takes a reference; the traits of the classes).
