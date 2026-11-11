#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <utility>
#include <vector>

#include "actions.hpp"
#include "game.hpp"
#include "loader.hpp"
#include "zth_test.hpp"

using namespace adv;

// shortens a text for a failure message (a local helper: zth::shorten is not available in every build)
inline std::string clip(std::string s) {
    for (char& c : s) {
        if (c == '\n') c = ' ';
    }
    return s.size() > 90 ? s.substr(0, 87) + "..." : s;
}

namespace {

const char* kSmall = R"(# The cellar adventure
start cellar
alias get take
alias grab take
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
item oak-door "a big oak door" in hall door locked key key
item anvil "a heavy anvil" in hall
item gold "a pile of gold" in vault portable
)";

const char* kCellarLit = "A damp cellar. Water drips from the ceiling.\nYou see: a coil of rope, a trapdoor.\nExits: north, up.";
const char* kHall = "A long hall. An oak door leads east.\nYou see: a heavy anvil, a big oak door.\nExits: south, east.";
const char* kKitchen = "A cosy kitchen.\nYou see: an iron key.\nExits: down.";

std::string show(const std::string& t) {
    std::string s;
    for (char c : t) s += c == '\n' ? std::string("\\n") : std::string(1, c);
    return clip(s);
}

World must_load(const std::string& text) {
    LoadResult r = load_world(text);
    if (!r.ok()) {
        ::zth::fail(std::string("load_world failed: ") + name(r.error) + " on line " + std::to_string(r.line) + " (" + r.message + ")");
        return World{};
    }
    return std::move(r.world);
}

struct Play {
    Game game;
    explicit Play(const std::string& text = kSmall) : game(must_load(text)) {}
    void step(const std::string& line, const std::string& want) {
        const std::string got = game.execute(line);
        if (got != want) ::zth::fail("after `" + show(line) + "` the reply was \"" + show(got) + "\", expected \"" + show(want) + "\"");
    }
};

void expect_load_error(const std::string& text, LoadError e, std::size_t line) {
    const LoadResult r = load_world(text);
    const std::string want = std::string(name(e)) + " on line " + std::to_string(line);
    const std::string got = r.ok() ? std::string("success") : std::string(name(r.error)) + " on line " + std::to_string(r.line);
    if (want != got) ::zth::fail("load_world(" + show(text) + "): expected " + want + ", got " + got);
    else if (r.message.empty()) ::zth::fail("load_world(" + show(text) + "): the error has no message");
}

const std::string kTwoRooms = "start a\nroom a \"A\"\nroom b \"B\"\n";

// an extra verb, written by the test: proves that the engine is open for extension
class Shout final : public Action {
public:
    std::string perform(Game&, const std::vector<std::string>& args) const override {
        std::string out;
        for (const std::string& a : args) {
            if (!out.empty()) out += ' ';
            for (char c : a) out += static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
        }
        return out + "!";
    }
};
class WhereAmI final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>&) const override { return "in the " + game.location(); }
};
class Teleport final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>& args) const override {
        if (args.size() != 1) return "Where?";
        game.set_location(args[0]);
        return "Poof.";
    }
};

std::string big_world_text(int n) {
    std::string t = "start r0\n";
    for (int i = 0; i < n; ++i) {
        const std::string id = "r" + std::to_string(i);
        t += "room " + id + " \"Room number " + std::to_string(i) + " of the big world\"\n";
        if (i + 1 < n) t += "  exit east r" + std::to_string(i + 1) + "\n";
        if (i > 0) t += "  exit west r" + std::to_string(i - 1) + "\n";
        t += "  item thing" + std::to_string(i) + " \"thing number " + std::to_string(i) + "\" in " + id + " portable\n";
    }
    return t;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "load: rooms, items, start, aliases and the flags") {
    const World w = must_load(kSmall);
    ZTH_EXPECT_EQ(w.start, std::string("cellar"));
    ZTH_EXPECT_EQ(w.rooms.size(), std::size_t{4});
    ZTH_EXPECT_EQ(w.items.size(), std::size_t{7});
    ZTH_EXPECT_EQ(w.aliases.size(), std::size_t{9});
    ZTH_EXPECT_EQ(w.aliases.at("n"), std::string("go north"));
    ZTH_EXPECT_EQ(w.aliases.at("get"), std::string("take"));
    ZTH_EXPECT_TRUE(w.rooms.at("cellar").dark);
    ZTH_EXPECT_FALSE(w.rooms.at("hall").dark);
    ZTH_EXPECT_TRUE(w.rooms.at("vault").win);
    ZTH_EXPECT_FALSE(w.rooms.at("cellar").win);
    ZTH_EXPECT_EQ(w.rooms.at("cellar").description, std::string("A damp cellar. Water drips from the ceiling."));
}
ZTH_TEST("works", "load: exits (in file order, with their doors) and where the items are") {
    const World w = must_load(kSmall);
    const Room& cellar = w.rooms.at("cellar");
    ZTH_EXPECT_EQ(cellar.exits.size(), std::size_t{2});
    ZTH_EXPECT_EQ(cellar.exits[0].dir, std::string("north"));
    ZTH_EXPECT_EQ(cellar.exits[0].to, std::string("hall"));
    ZTH_EXPECT_EQ(cellar.exits[0].via, std::string());
    ZTH_EXPECT_EQ(cellar.exits[1].dir, std::string("up"));
    ZTH_EXPECT_EQ(cellar.exits[1].to, std::string("kitchen"));
    ZTH_EXPECT_EQ(cellar.exits[1].via, std::string("trapdoor"));
    ZTH_EXPECT_EQ(w.rooms.at("hall").exits[1].via, std::string("oak-door"));
    ZTH_EXPECT_EQ(w.rooms.at("hall").exits[1].dir, std::string("east"));
    ZTH_EXPECT_TRUE((cellar.items == std::set<std::string>{"rope", "trapdoor"}));
    ZTH_EXPECT_TRUE((w.rooms.at("hall").items == std::set<std::string>{"anvil", "oak-door"}));
    ZTH_EXPECT_TRUE((w.inventory == std::set<std::string>{"lamp"}));
    ZTH_EXPECT_TRUE(w.rooms.at("kitchen").items.count("key") == 1);
}
ZTH_TEST("works", "load: item states are a std::variant (plain, switch, door)") {
    const World w = must_load(kSmall);
    const Item& lamp = w.items.at("lamp");
    ZTH_EXPECT_EQ(lamp.name, std::string("a brass lamp"));
    ZTH_EXPECT_TRUE(lamp.portable);
    ZTH_EXPECT_TRUE(std::holds_alternative<Switch>(lamp.state));
    ZTH_EXPECT_FALSE(std::get<Switch>(lamp.state).on);
    const Item& oak = w.items.at("oak-door");
    ZTH_EXPECT_FALSE(oak.portable);
    ZTH_EXPECT_TRUE(std::holds_alternative<Door>(oak.state));
    ZTH_EXPECT_TRUE(std::get<Door>(oak.state).lock == Lock::Locked);
    ZTH_EXPECT_EQ(std::get<Door>(oak.state).key, std::string("key"));
    const Item& trap = w.items.at("trapdoor");
    ZTH_EXPECT_TRUE(std::get<Door>(trap.state).lock == Lock::Open);
    ZTH_EXPECT_EQ(std::get<Door>(trap.state).key, std::string());
    ZTH_EXPECT_TRUE(std::holds_alternative<Plain>(w.items.at("anvil").state));
    ZTH_EXPECT_TRUE(w.items.at("rope").portable);
    const World lit = must_load("start a\nroom a \"A\"\nitem l \"a lamp\" in a switch on\nitem c \"a chest\" in a door closed\n");
    ZTH_EXPECT_TRUE(std::get<Switch>(lit.items.at("l").state).on);
    ZTH_EXPECT_TRUE(std::get<Door>(lit.items.at("c").state).lock == Lock::Closed);
}
ZTH_TEST("works", "a dark room, a lamp, and look") {
    Play p;
    p.step("look", "It is pitch dark.");
    p.step("turn on lamp", "You turn it on.");
    p.step("look", kCellarLit);
    p.step("turn off lamp", "You turn it off.");
    p.step("look", "It is pitch dark.");
}
ZTH_TEST("works", "start() describes the first room like look does") {
    Play p;
    ZTH_EXPECT_EQ(p.game.start(), std::string("It is pitch dark."));
    Play q(kTwoRooms);
    ZTH_EXPECT_EQ(q.game.start(), std::string("A"));
    ZTH_EXPECT_EQ(q.game.location(), std::string("a"));
}
ZTH_TEST("works", "go: through exits, with the aliases u d n s") {
    Play p;
    p.step("turn on lamp", "You turn it on.");
    p.step("go up", kKitchen);
    p.step("go down", kCellarLit);
    p.step("n", kHall);
    p.step("s", kCellarLit);
    p.step("u", kKitchen);
    p.step("d", kCellarLit);
    p.step("go sideways", "You can't go that way.");
    p.step("go south", "You can't go that way.");
    ZTH_EXPECT_EQ(p.game.location(), std::string("cellar"));
}
ZTH_TEST("works", "take, drop and inventory (items are listed by id)") {
    Play p;
    p.step("turn on lamp", "You turn it on.");
    p.step("inventory", "You are carrying: a brass lamp.");
    p.step("take rope", "Taken.");
    p.step("inventory", "You are carrying: a brass lamp, a coil of rope.");
    p.step("look", "A damp cellar. Water drips from the ceiling.\nYou see: a trapdoor.\nExits: north, up.");
    p.step("drop rope", "Dropped.");
    p.step("look", kCellarLit);
    p.step("drop lamp", "Dropped.");
    p.step("inventory", "You are carrying nothing.");
    p.step("take lamp", "Taken.");
}
ZTH_TEST("works", "take: messages for what cannot be taken") {
    Play p;
    p.step("turn on lamp", "You turn it on.");
    p.step("take rope", "Taken.");
    p.step("take rope", "You already have that.");
    p.step("take trapdoor", "You can't take that.");
    p.step("take unicorn", "You don't see that here.");
    p.step("take key", "You don't see that here.");
    p.step("drop trapdoor", "You aren't carrying that.");
    p.step("drop key", "You aren't carrying that.");
}
ZTH_TEST("works", "words: aliases (get, grab, i), articles (the, a, an) and capital letters") {
    Play p;
    p.step("TURN ON the LAMP", "You turn it on.");
    p.step("get the rope", "Taken.");
    p.step("TAKE   the   Rope", "You already have that.");
    p.step("I", "You are carrying: a brass lamp, a coil of rope.");
    p.step("grab a trapdoor", "You can't take that.");
    p.step("\tinventory\r\n", "You are carrying: a brass lamp, a coil of rope.");
    p.step("drop an rope", "Dropped.");
}
ZTH_TEST("works", "look at an item (x and examine are aliases)") {
    Play p;
    p.step("x lamp", "This is a brass lamp. It is off.");
    p.step("turn on lamp", "You turn it on.");
    p.step("x lamp", "This is a brass lamp. It is on.");
    p.step("look at rope", "This is a coil of rope.");
    p.step("examine trapdoor", "This is a trapdoor. It is open.");
    p.step("look at the rope", "This is a coil of rope.");
    p.step("x key", "You don't see that here.");
    p.step("look at", "What do you want to look at?");
    p.step("look at lamp rope", "I don't understand.");
}
ZTH_TEST("works", "doors: open, close, and a door that blocks an exit") {
    Play p;
    p.step("turn on lamp", "You turn it on.");
    p.step("close trapdoor", "You close it.");
    p.step("go up", "The way is blocked by a trapdoor.");
    p.step("close trapdoor", "It is already closed.");
    p.step("x trapdoor", "This is a trapdoor. It is closed.");
    p.step("open trapdoor", "You open it.");
    p.step("open trapdoor", "It is already open.");
    p.step("go up", kKitchen);
    p.step("open rope", "You don't see that here.");
    p.step("d", kCellarLit);
    p.step("open rope", "You can't open that.");
    p.step("close rope", "You can't close that.");
}
ZTH_TEST("works", "locks: unlock and lock with the right key") {
    Play p;
    p.step("turn on lamp", "You turn it on.");
    p.step("take rope", "Taken.");
    p.step("u", kKitchen);
    p.step("take key", "Taken.");
    p.step("d", "A damp cellar. Water drips from the ceiling.\nYou see: a trapdoor.\nExits: north, up.");
    p.step("n", kHall);
    p.step("go east", "The way is blocked by a big oak door.");
    p.step("close oak-door", "It is already closed.");
    p.step("x oak-door", "This is a big oak door. It is locked.");
    p.step("open oak-door", "It is locked.");
    p.step("unlock oak-door with key", "You unlock it.");
    p.step("unlock oak-door with key", "It isn't locked.");
    p.step("x oak-door", "This is a big oak door. It is closed.");
    p.step("go east", "The way is blocked by a big oak door.");
    p.step("open oak-door", "You open it.");
    p.step("lock oak-door with key", "Close it first.");
    p.step("close oak-door", "You close it.");
    p.step("lock oak-door with rope", "That doesn't fit.");
    p.step("lock oak-door with key", "You lock it.");
    p.step("lock oak-door with key", "It is already locked.");
    p.step("x oak-door", "This is a big oak door. It is locked.");
}
ZTH_TEST("works", "unlock and lock: the wrong key, no key, wrong things") {
    Play p;
    p.step("turn on lamp", "You turn it on.");
    p.step("take rope", "Taken.");
    p.step("n", kHall);
    p.step("unlock oak-door with key", "You aren't carrying that.");
    p.step("unlock oak-door with rope", "That doesn't fit.");
    p.step("unlock anvil with rope", "You can't unlock that.");
    p.step("lock anvil with rope", "You can't lock that.");
    p.step("unlock oak-door", "Unlock it with what?");
    p.step("lock oak-door", "Lock it with what?");
    p.step("unlock", "What do you want to unlock?");
    p.step("lock", "What do you want to lock?");
    p.step("unlock oak-door rope", "I don't understand.");
    p.step("unlock oak-door with", "I don't understand.");
    p.step("unlock unicorn with rope", "You don't see that here.");
}
ZTH_TEST("works", "turn: on and off, in either word order, and the messages") {
    Play p;
    p.step("turn on lamp", "You turn it on.");
    p.step("turn on lamp", "It is already on.");
    p.step("turn lamp off", "You turn it off.");
    p.step("turn lamp off", "It is already off.");
    p.step("turn lamp on", "You turn it on.");
    p.step("turn off lamp", "You turn it off.");
    p.step("turn", "Turn what on or off?");
    p.step("turn on", "Turn what on or off?");
    p.step("turn lamp", "Turn it on or off?");
    p.step("turn lamp sideways", "I don't understand.");
    p.step("turn on lamp now", "I don't understand.");
    p.step("turn on lamp", "You turn it on.");
    p.step("turn on rope", "You can't switch that.");
    p.step("turn on unicorn", "You don't see that here.");
}
ZTH_TEST("works", "the whole game: walk to the vault and win") {
    Play p;
    p.step("turn on lamp", "You turn it on.");
    p.step("u", kKitchen);
    p.step("take key", "Taken.");
    p.step("d", kCellarLit);
    p.step("n", kHall);
    p.step("unlock oak-door with key", "You unlock it.");
    p.step("open oak-door", "You open it.");
    ZTH_EXPECT_FALSE(p.game.finished());
    p.step("go east", "The vault glitters with gold. You win!\nYou see: a pile of gold.\nExits: west.\n*** You have won! ***");
    ZTH_EXPECT_TRUE(p.game.finished());
    p.step("look", "The game is over.");
    p.step("take gold", "The game is over.");
    p.step("", "The game is over.");
}
ZTH_TEST("works", "unknown words, empty lines, missing nouns") {
    Play p;
    p.step("", "Say something.");
    p.step("   \t ", "Say something.");
    p.step("dance", "I don't understand.");
    p.step("the lamp", "I don't understand.");
    p.step("take", "What do you want to take?");
    p.step("drop", "What do you want to drop?");
    p.step("open", "What do you want to open?");
    p.step("close", "What do you want to close?");
    p.step("go", "Where do you want to go?");
    p.step("take lamp rope", "I don't understand.");
    p.step("go north now", "I don't understand.");
}
ZTH_TEST("works", "extension: a new verb is a new Action; aliases in the data can point to it") {
    Play p("start a\nalias shh shout\nroom a \"A quiet room.\"\n");
    p.step("shout hello", "I don't understand.");
    p.game.add_action("shout", std::make_unique<Shout>());
    p.game.add_action("where", std::make_unique<WhereAmI>());
    p.step("shout hello there", "HELLO THERE!");
    p.step("SHOUT Hi", "HI!");
    p.step("shh be quiet", "BE QUIET!");
    p.step("where", "in the a");
    p.step("shout the end", "END!");
}
ZTH_TEST("works", "extension: an Action can replace a built-in verb and can change the game") {
    Play p;
    p.game.add_action("teleport", std::make_unique<Teleport>());
    p.step("teleport hall", "Poof.");
    ZTH_EXPECT_EQ(p.game.location(), std::string("hall"));
    p.step("look", kHall);
    p.game.add_action("look", std::make_unique<Shout>());
    p.step("look at me", "AT ME!");
    p.step("x lamp", "LAMP!");
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "load errors while reading: unknown directive, syntax (BadSyntax, with the line)") {
    expect_load_error("bogus line", LoadError::UnknownDirective, 1);
    expect_load_error("start a\n\n\nroomy a \"x\"", LoadError::UnknownDirective, 4);
    expect_load_error("START a", LoadError::UnknownDirective, 1);
    expect_load_error("start", LoadError::BadSyntax, 1);
    expect_load_error("start a b", LoadError::BadSyntax, 1);
    expect_load_error("start A", LoadError::BadSyntax, 1);
    expect_load_error("room a", LoadError::BadSyntax, 1);
    expect_load_error("room a desc", LoadError::BadSyntax, 1);
    expect_load_error("room a \"unclosed", LoadError::BadSyntax, 1);
    expect_load_error("room a \"x\"y", LoadError::BadSyntax, 1);
    expect_load_error("room a \"A\"dark", LoadError::BadSyntax, 1);
    expect_load_error("room a x\"A\"", LoadError::BadSyntax, 1);
    expect_load_error("room A \"x\"", LoadError::BadSyntax, 1);
    expect_load_error("room \"a\" \"x\"", LoadError::BadSyntax, 1);
    expect_load_error("room inventory \"x\"", LoadError::BadSyntax, 1);
    expect_load_error("room a \"x\" dark dark", LoadError::BadSyntax, 1);
    expect_load_error("room a \"x\" spooky", LoadError::BadSyntax, 1);
    expect_load_error("room a \"x\" \"dark\"", LoadError::BadSyntax, 1);
    expect_load_error("alias", LoadError::BadSyntax, 1);
    expect_load_error("alias n", LoadError::BadSyntax, 1);
    expect_load_error("alias N go north", LoadError::BadSyntax, 1);
    expect_load_error("alias n go \"north\"", LoadError::BadSyntax, 1);
}
ZTH_TEST("robust", "load errors: exits") {
    expect_load_error("exit north b", LoadError::ExitOutsideRoom, 1);
    expect_load_error("# first\n\nexit north b\nroom a \"A\"", LoadError::ExitOutsideRoom, 3);
    expect_load_error("room a \"A\"\nexit north", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nexit north a via", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nexit north a by door", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nexit north a via door extra", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nexit North a", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nexit north a\nexit north a", LoadError::BadSyntax, 3);
    expect_load_error("room a \"A\"\nexit \"north\" a", LoadError::BadSyntax, 2);
}
ZTH_TEST("robust", "load errors: items") {
    expect_load_error("item i \"n\" at a", LoadError::BadSyntax, 1);
    expect_load_error("item i \"n\" in", LoadError::BadSyntax, 1);
    expect_load_error("item i n in a", LoadError::BadSyntax, 1);
    expect_load_error("item I \"n\" in a", LoadError::BadSyntax, 1);
    expect_load_error("room a \"A\"\nitem i \"n\" in a portable portable", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nitem i \"n\" in a switch", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nitem i \"n\" in a switch maybe", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nitem i \"n\" in a door ajar", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nitem i \"n\" in a door", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nitem i \"n\" in a switch on door open", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nitem i \"n\" in a door open switch on", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nitem i \"n\" in a key k", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nitem i \"n\" in a door locked key", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nitem i \"n\" in a shiny", LoadError::BadSyntax, 2);
    expect_load_error("room a \"A\"\nitem i \"n\" in a portable switch on switch off", LoadError::BadSyntax, 2);
}
ZTH_TEST("robust", "load errors: something defined twice (DuplicateId, on the second line)") {
    expect_load_error("room a \"A\"\nroom a \"B\"", LoadError::DuplicateId, 2);
    expect_load_error("start a\nstart b", LoadError::DuplicateId, 2);
    expect_load_error("room a \"A\"\nitem i \"n\" in a\n\nitem i \"m\" in a", LoadError::DuplicateId, 4);
    expect_load_error("alias n go north\nalias n go south", LoadError::DuplicateId, 2);
}
ZTH_TEST("robust", "load errors: ids that are never defined (UnknownReference, on the line that mentions them)") {
    expect_load_error("start nowhere\nroom a \"A\"", LoadError::UnknownReference, 1);
    expect_load_error("start a\nroom a \"A\"\n  exit north b", LoadError::UnknownReference, 3);
    expect_load_error("start a\nroom a \"A\"\n  exit north a via door", LoadError::UnknownReference, 3);
    expect_load_error("start a\nroom a \"A\"\nitem i \"n\" in b", LoadError::UnknownReference, 3);
    expect_load_error("start a\nroom a \"A\"\nitem d \"a door\" in a door locked key k", LoadError::UnknownReference, 3);
    expect_load_error("start a\nroom a \"A\"\nitem i \"n\" in nothing portable", LoadError::UnknownReference, 3);
}
ZTH_TEST("robust", "forward references are fine; with several problems the smallest line is reported") {
    const World w = must_load("start b\nroom a \"A\"\n  exit east b via d\nroom b \"B\"\nitem d \"a door\" in a door closed key k\nitem k \"a key\" in b portable\n");
    ZTH_EXPECT_EQ(w.rooms.size(), std::size_t{2});
    ZTH_EXPECT_EQ(w.rooms.at("a").exits[0].to, std::string("b"));
    ZTH_EXPECT_TRUE(w.rooms.at("b").items.count("k") == 1);
    ZTH_EXPECT_TRUE(w.rooms.at("a").items.count("d") == 1);
    expect_load_error("start zzz\nroom a \"A\"\n exit n q\n", LoadError::UnknownReference, 1);
    expect_load_error("start a\nroom a \"A\"\n exit n q\n exit s r\n", LoadError::UnknownReference, 3);
    expect_load_error("start a\nroom a \"A\"\nitem i \"n\" in q\n exit s a via nothing", LoadError::UnknownReference, 3);
}
ZTH_TEST("robust", "a syntax error stops loading before references are checked (even if the reference is on an earlier line)") {
    expect_load_error("start a\nroom a \"A\"\n exit n zzz\nbogus", LoadError::UnknownDirective, 4);
    expect_load_error("start nowhere\nroom a \"A\" spooky", LoadError::BadSyntax, 2);
}
ZTH_TEST("robust", "a world without a start line is MissingStart (line 0)") {
    expect_load_error("room a \"A\"", LoadError::MissingStart, 0);
    expect_load_error("", LoadError::MissingStart, 0);
    expect_load_error("# only a comment\n\n", LoadError::MissingStart, 0);
}
ZTH_TEST("robust", "loading accepts comments, blank lines, indentation, tabs, CRLF, trailing blanks, # inside quotes, an empty description") {
    const World w = must_load(
        "# comment\r\n"
        "\r\n"
        "   start   hall   \r\n"
        "\talias   look2   look   \r\n"
        "room hall \"A hall # not a comment\"  dark   win  \r\n"
        "\t\texit\tnorth\t\thall\t\r\n"
        "   # indented comment\r\n"
        "room attic \"\"\r\n"
        "item thing-1 \"a thing, with: punctuation!\" in attic  portable  \r\n"
        "item   pocket_lint \"lint\" in inventory\r\n");
    ZTH_EXPECT_EQ(w.start, std::string("hall"));
    ZTH_EXPECT_EQ(w.rooms.at("hall").description, std::string("A hall # not a comment"));
    ZTH_EXPECT_TRUE(w.rooms.at("hall").dark);
    ZTH_EXPECT_TRUE(w.rooms.at("hall").win);
    ZTH_EXPECT_EQ(w.rooms.at("hall").exits[0].to, std::string("hall"));
    ZTH_EXPECT_EQ(w.rooms.at("attic").description, std::string());
    ZTH_EXPECT_EQ(w.items.at("thing-1").name, std::string("a thing, with: punctuation!"));
    ZTH_EXPECT_TRUE(w.items.at("thing-1").portable);
    ZTH_EXPECT_TRUE(w.inventory.count("pocket_lint") == 1);
    ZTH_EXPECT_EQ(w.aliases.at("look2"), std::string("look"));
}
ZTH_TEST("robust", "line numbers count blank lines, comments and CRLF lines") {
    expect_load_error("# c\r\n\r\n\r\n# c\r\nroom a \"A\" spooky\r\n", LoadError::BadSyntax, 5);
    expect_load_error("\n\n\n\nbogus", LoadError::UnknownDirective, 5);
}
ZTH_TEST("robust", "darkness: nothing can be seen or taken without light; carried things still work") {
    Play p;
    p.step("take rope", "You can't see anything.");
    p.step("look at rope", "You can't see anything.");
    p.step("open trapdoor", "You can't see anything.");
    p.step("x lamp", "This is a brass lamp. It is off.");
    p.step("take unicorn", "You can't see anything.");
    p.step("go up", "A cosy kitchen.\nYou see: an iron key.\nExits: down.");  // moving in the dark is allowed, the kitchen is lit
    p.step("d", "It is pitch dark.");
    p.step("inventory", "You are carrying: a brass lamp.");
    p.step("turn on lamp", "You turn it on.");
    p.step("take rope", "Taken.");
}
ZTH_TEST("robust", "light: a lamp lying on the floor of a dark room still lights it; a lamp in another room does not") {
    Play p;
    p.step("turn on lamp", "You turn it on.");
    p.step("drop lamp", "Dropped.");
    p.step("look", "A damp cellar. Water drips from the ceiling.\nYou see: a brass lamp, a coil of rope, a trapdoor.\nExits: north, up.");
    p.step("take rope", "Taken.");
    p.step("u", kKitchen);
    p.step("d", "A damp cellar. Water drips from the ceiling.\nYou see: a brass lamp, a trapdoor.\nExits: north, up.");
    p.step("turn off lamp", "You turn it off.");
    p.step("look", "It is pitch dark.");
    p.step("drop rope", "Dropped.");
    p.step("take lamp", "You can't see anything.");
    p.step("turn on lamp", "You can't see anything.");
    p.step("drop rope", "You aren't carrying that.");
}
ZTH_TEST("robust", "light: the lamp carried into another dark room works there; leaving it behind makes the next dark room dark") {
    Play p("start a\nroom a \"A\" dark\n  exit east b\nroom b \"B\" dark\n  exit west a\nitem lamp \"a lamp\" in a portable switch on\nitem coin \"a coin\" in b portable\n");
    p.step("take lamp", "Taken.");
    p.step("go east", "B\nYou see: a coin.\nExits: west.");
    p.step("drop lamp", "Dropped.");
    p.step("west", "I don't understand.");
    p.step("go west", "It is pitch dark.");
    p.step("go east", "B\nYou see: a coin, a lamp.\nExits: west.");
}
ZTH_TEST("robust", "an exit through a door that is not a Door (a plain item) is free; a closed door in another room still blocks") {
    Play p("start a\nroom a \"A\"\n  exit north b via statue\n  exit east b via gate\nroom b \"B\"\nitem statue \"a statue\" in a\nitem gate \"a gate\" in a door closed\n");
    p.step("go east", "The way is blocked by a gate.");
    p.step("go north", "B");
}
ZTH_TEST("robust", "aliases: expand once, keep the rest of the line, may name an unknown verb or another alias") {
    Play p("start a\nalias n go north\nalias w1 go\nalias loop1 loop2\nalias loop2 look\nalias zz zzz\nroom a \"A\"\n  exit north b\nroom b \"B\"\n  exit south a\n");
    p.step("n", "B\nExits: south.");
    p.step("s", "I don't understand.");
    p.step("go south", "A\nExits: north.");
    p.step("w1 north", "B\nExits: south.");
    p.step("W1 SOUTH", "A\nExits: north.");
    p.step("w1", "Where do you want to go?");
    p.step("a inventory", "I don't understand.");
    p.step("the look", "I don't understand.");
    p.step("loop1", "I don't understand.");
    p.step("zz", "I don't understand.");
    p.step("n now", "I don't understand.");
}
ZTH_TEST("robust", "an alias wins over a verb of the same name") {
    Play p("start a\nalias inventory look\nalias take go\nroom a \"A\"\n  exit east b\nroom b \"B\"\n  exit west a\nitem coin \"a coin\" in a portable\n");
    p.step("inventory", "A\nYou see: a coin.\nExits: east.");
    p.step("take east", "B\nExits: west.");
}
ZTH_TEST("robust", "take: an item that is carried but not portable is already had (the order of the checks)") {
    Play p("start a\nroom a \"A\"\nitem rock \"a rock\" in inventory\nitem boulder \"a boulder\" in a\n");
    p.step("take rock", "You already have that.");
    p.step("take boulder", "You can't take that.");
    p.step("drop rock", "Dropped.");
    p.step("take rock", "You can't take that.");
}
ZTH_TEST("robust", "after the game is won every command (also an empty one) says the game is over") {
    Play p("start a\nalias z look\nroom a \"A\"\n  exit east b\nroom b \"B\" win\n");
    p.step("go east", "B\n*** You have won! ***");
    for (const char* cmd : {"look", "z", "", "go west", "dance", "inventory"}) p.step(cmd, "The game is over.");
}
ZTH_TEST("robust", "a room can be both dark and win; entering it wins, light or not") {
    Play p("start a\nroom a \"A\"\n  exit east b\nroom b \"B\" dark win\n");
    p.step("go east", "It is pitch dark.\n*** You have won! ***");
    ZTH_EXPECT_TRUE(p.game.finished());
}
ZTH_TEST("robust", "the world seen through game.world() is the live state (items move, states change)") {
    Play p;
    p.step("turn on lamp", "You turn it on.");
    p.step("take rope", "Taken.");
    ZTH_EXPECT_TRUE(p.game.world().inventory.count("rope") == 1);
    ZTH_EXPECT_TRUE(p.game.world().rooms.at("cellar").items.count("rope") == 0);
    ZTH_EXPECT_TRUE(std::get<Switch>(p.game.world().items.at("lamp").state).on);
    p.step("close trapdoor", "You close it.");
    ZTH_EXPECT_TRUE(std::get<Door>(p.game.world().items.at("trapdoor").state).lock == Lock::Closed);
    p.game.world().rooms.at("cellar").description = "Changed.";
    p.step("look", "Changed.\nYou see: a trapdoor.\nExits: north, up.");
    p.game.set_location("hall");
    p.step("look", kHall);
}
ZTH_TEST_TIMEOUT("robust", "20000 random commands never break the world (items are somewhere exactly once, the player is in a room)", 20) {
    ::zth::Lcg rng(31337);
    Play p;
    static const char* verbs[] = {"look", "go", "take", "drop", "inventory", "open", "close", "unlock", "lock", "turn", "x", "get", "n", "s", "u", "d", "dance", ""};
    static const char* nouns[] = {"lamp", "rope", "trapdoor", "key", "oak-door", "anvil", "gold", "north", "south", "east", "west", "up", "down", "on", "off", "with", "the", "unicorn"};
    for (int i = 0; i < 20000 && !p.game.finished(); ++i) {
        std::string line = verbs[rng.below(18)];
        const int words = rng.below(4);
        for (int k = 0; k < words; ++k) line += std::string(" ") + nouns[rng.below(18)];
        (void)p.game.execute(line);
        const World& w = p.game.world();
        if (w.rooms.count(p.game.location()) == 0) {
            ::zth::fail("the player is in a room that does not exist after `" + line + "`");
            return;
        }
        for (const auto& [id, item] : w.items) {
            int places = w.inventory.count(id) ? 1 : 0;
            for (const auto& [rid, room] : w.rooms) places += room.items.count(id) ? 1 : 0;
            if (places != 1) {
                ::zth::fail("item " + id + " is in " + std::to_string(places) + " places after `" + line + "`");
                return;
            }
        }
    }
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "loading does not copy rooms or items (5000 rooms, 5000 items: at most 20 copies)", 10) {
    const std::string text = big_world_text(5000);
    stats::reset();
    LoadResult r = load_world(text);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(r.world.rooms.size(), std::size_t{5000});
    ZTH_EXPECT_TRUE(stats::rooms() + stats::items() <= 20);
}
ZTH_TEST_TIMEOUT("fast", "a world is moved into the Game and into the result, never copied", 10) {
    stats::reset();
    LoadResult r = load_world(big_world_text(2000));
    ZTH_EXPECT_TRUE(r.ok());
    Game game(std::move(r.world));
    ZTH_EXPECT_EQ(stats::worlds(), 0ULL);
    ZTH_EXPECT_TRUE(stats::rooms() + stats::items() <= 20);
    ZTH_EXPECT_FALSE(game.start().empty());
}
ZTH_TEST_TIMEOUT("fast", "commands do not copy the world, a room or an item (3000 commands on a 2000-room world: at most 20 copies)", 10) {
    LoadResult r = load_world(big_world_text(2000));
    ZTH_EXPECT_TRUE(r.ok());
    Game game(std::move(r.world));
    stats::reset();
    ::zth::Lcg rng(5);
    for (int i = 0; i < 3000; ++i) {
        switch (rng.below(8)) {
            case 0: game.execute("go west"); break;
            case 1: case 2: game.execute("go east"); break;
            case 3: game.execute("look"); break;
            case 4: game.execute("take thing" + std::to_string(rng.below(2000))); break;
            case 5: game.execute("drop thing" + std::to_string(rng.below(2000))); break;
            case 6: game.execute("inventory"); break;
            default: game.execute("look at thing" + std::to_string(rng.below(2000))); break;
        }
    }
    for (int i = 0; i < 300; ++i) {
        game.set_location("r0");
        game.execute("look at thing0");
        game.execute("take thing0");
        game.execute("x thing0");
        game.execute("drop thing0");
    }
    ZTH_EXPECT_EQ(stats::worlds(), 0ULL);
    ZTH_EXPECT_TRUE(stats::rooms() + stats::items() <= 20);
}
ZTH_TEST_TIMEOUT("fast", "loading: at most 10 allocations per room (with its exits and its item) and the cost grows linearly", 10) {
    const std::string small = big_world_text(3000), large = big_world_text(6000);
    std::size_t a = 0, b = 0;
    {
        zth::mem::Scope scope;
        LoadResult r = load_world(small);
        ZTH_EXPECT_TRUE(r.ok());
        a = scope.allocs();
    }
    {
        zth::mem::Scope scope;
        LoadResult r = load_world(large);
        ZTH_EXPECT_TRUE(r.ok());
        b = scope.allocs();
    }
    ZTH_EXPECT_TRUE(a <= 3000 * 10);
    ZTH_EXPECT_TRUE(b <= a * 5 / 2);
}
ZTH_TEST_TIMEOUT("fast", "loading 100000 lines is done in time (no quadratic duplicate checks or copying)", 10) {
    const std::string text = big_world_text(25000);
    LoadResult r = load_world(text);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(r.world.items.size(), std::size_t{25000});
}
ZTH_TEST_TIMEOUT("fast", "a command costs the same in a world of 20000 rooms as in a world of 100 (allocations per command)", 10) {
    auto per_command = [](int rooms) -> double {
        LoadResult r = load_world(big_world_text(rooms));
        Game game(std::move(r.world));
        game.set_location("r" + std::to_string(rooms / 2));
        ::zth::Lcg rng(99);
        zth::mem::Scope scope;
        for (int i = 0; i < 2000; ++i) {
            switch (rng.below(5)) {
                case 0: game.execute("go east"); break;
                case 1: game.execute("go west"); break;
                case 2: game.execute("look"); break;
                case 3: game.execute("inventory"); break;
                default: game.execute("take thing" + std::to_string(rng.below(rooms))); break;
            }
        }
        return static_cast<double>(scope.allocs()) / 2000.0;
    };
    const double small = per_command(100);
    const double large = per_command(20000);
    ZTH_EXPECT_TRUE(small > 0);
    ZTH_EXPECT_TRUE(large <= small * 1.3 + 1);
}
ZTH_TEST_TIMEOUT("fast", "a command needs at most 8 allocations on average (splitting the line, building the reply)", 10) {
    Play p;
    p.game.execute("turn on lamp");
    zth::mem::Scope scope;
    for (int i = 0; i < 1000; ++i) {
        p.game.execute("take rope");
        p.game.execute("drop rope");
        p.game.execute("look");
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 3000 * 8);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw new/delete/malloc/free in any of the three files (the table owns its Actions through std::unique_ptr)") {
    ZTH_EXPECT_NO_RAW_MEMORY("loader.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("actions.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("game.hpp");
}
ZTH_TEST("clean", "no throw/try/catch/goto, no dynamic_cast/typeid, no output streams") {
    for (const char* file : {"loader.hpp", "actions.hpp", "game.hpp"}) {
        const std::string code = ::zth::strip_comments_and_literals(::zth::read_file(file));
        for (const char* word : {"throw", "try", "catch", "goto", "dynamic_cast", "typeid", "cout", "cerr", "printf", "stoi", "stol", "sscanf", "stringstream", "istringstream"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`");
                return;
            }
        }
    }
}
ZTH_TEST("clean", "the verbs are Actions: at least eight classes derived from Action in actions.hpp, each overriding perform") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("actions.hpp"));
    std::size_t derived = 0, overrides = 0;
    for (std::size_t p = 0; (p = code.find("public Action", p)) != std::string::npos; p += 13) ++derived;
    for (std::size_t p = 0; (p = code.find("override", p)) != std::string::npos; p += 8) ++overrides;
    ZTH_EXPECT_TRUE(derived >= 8);
    ZTH_EXPECT_TRUE(overrides >= derived);
}
ZTH_TEST("clean", "game.hpp knows no verbs: the dispatch is a table lookup (no \"take\", \"look\", \"go\", ... in game.hpp)") {
    const std::string text = ::zth::read_file("game.hpp");
    for (const char* verb : {"\"look\"", "\"go\"", "\"take\"", "\"drop\"", "\"inventory\"", "\"open\"", "\"close\"", "\"unlock\"", "\"lock\"", "\"turn\""}) {
        // the only place where game.hpp may name a verb is start(), which asks for "look"
        std::size_t count = 0;
        for (std::size_t p = 0; (p = text.find(verb, p)) != std::string::npos; p += 2) ++count;
        const std::size_t allowed = std::string(verb) == "\"look\"" ? 1 : 0;
        if (count > allowed) {
            ::zth::fail(std::string("game.hpp names the verb ") + verb + ": register an Action for it instead");
            return;
        }
    }
}
ZTH_TEST("clean", "item states are handled through the variant (std::get_if, std::holds_alternative or std::visit) in actions.hpp") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("actions.hpp"));
    ZTH_EXPECT_TRUE(!::zth::find_word(code, "get_if").empty() || !::zth::find_word(code, "visit").empty() || !::zth::find_word(code, "holds_alternative").empty());
}
