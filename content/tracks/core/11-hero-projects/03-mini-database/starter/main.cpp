// An interactive session: type one command per line in the input box (a sample session if it is empty). Read-only. Press Run.
#include <iostream>
#include <string>

#include "database.hpp"

int main() {
    db::Database database;
    std::string line;
    int lines = 0;
    while (std::getline(std::cin, line)) {
        ++lines;
        std::cout << "> " << line << "\n" << db::describe(database.execute(line)) << "\n";
    }
    if (lines == 0) {
        for (const char* cmd : {"SET name \"Ada Lovelace\"", "SET lang C++", "GET name", "COUNT C++", "BEGIN", "SET lang Rust", "COUNT C++", "ROLLBACK", "GET lang",
                                "SET b 2", "SET a 1", "SET c 3", "RANGE a b", "INCR n", "INCR n", "FROB x", "SET only-one-argument"}) {
            std::cout << "> " << cmd << "\n" << db::describe(database.execute(cmd)) << "\n";
        }
    }
}
