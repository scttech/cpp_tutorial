// A little game. Type commands in the input box (one per line), or press Run to watch a sample walkthrough. Read-only.
#include <iostream>
#include <string>

#include "game.hpp"
#include "loader.hpp"

static const char* kWorld = R"(# The cellar adventure
start cellar
alias get take
alias i inventory
alias x look
alias examine look
alias n go north
alias s go south
alias u go up
alias d go down

room cellar "A damp cellar. Water drips from the ceiling." dark
  exit north hall
  exit up kitchen via trapdoor
room hall "A long hall. An oak door leads east."
  exit south cellar
  exit east vault via oak-door
room kitchen "A cosy kitchen."
  exit down cellar via trapdoor
room vault "The vault glitters with gold. You win!" win
  exit west hall via oak-door

item lamp "a brass lamp" in inventory portable switch off
item rope "a coil of rope" in cellar portable
item trapdoor "a trapdoor" in cellar door open
item key "an iron key" in kitchen portable
item oak-door "an oak door" in hall door locked key key
item anvil "a heavy anvil" in hall
item gold "a pile of gold" in vault portable
)";

int main() {
    adv::LoadResult loaded = adv::load_world(kWorld);
    if (!loaded.ok()) {
        std::cout << "cannot load the world: " << adv::name(loaded.error) << " on line " << loaded.line << ": " << loaded.message << "\n";
        return 1;
    }
    adv::Game game(std::move(loaded.world));
    std::cout << game.start() << "\n";

    std::string line;
    int lines = 0;
    while (std::getline(std::cin, line)) {
        ++lines;
        std::cout << "\n> " << line << "\n" << game.execute(line) << "\n";
    }
    if (lines == 0) {
        for (const char* cmd : {"look", "turn on lamp", "look", "take rope", "inventory", "u", "take key", "d", "n", "go east", "unlock oak-door with key",
                                "open oak-door", "go east", "take gold", "look"}) {
            std::cout << "\n> " << cmd << "\n" << game.execute(cmd) << "\n";
        }
    }
}
