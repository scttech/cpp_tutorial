#pragma once
// READ-ONLY. The world, the items and their states, the Action interface, and the counters that the Fast tests read.
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace adv {

// Counts how often a Room / Item / World is COPIED (moves are free). It sits inside those types, so their normal
// copy constructors count automatically. Pass things by reference, move them into containers.
template <class Tag>
struct CopyCounter {
    static inline unsigned long long count = 0;
    CopyCounter() = default;
    CopyCounter(const CopyCounter&) { ++count; }
    CopyCounter(CopyCounter&&) noexcept = default;
    CopyCounter& operator=(const CopyCounter&) {
        ++count;
        return *this;
    }
    CopyCounter& operator=(CopyCounter&&) noexcept = default;
};
struct RoomTag {};
struct ItemTag {};
struct WorldTag {};
namespace stats {
inline unsigned long long rooms() { return CopyCounter<RoomTag>::count; }
inline unsigned long long items() { return CopyCounter<ItemTag>::count; }
inline unsigned long long worlds() { return CopyCounter<WorldTag>::count; }
inline void reset() { CopyCounter<RoomTag>::count = CopyCounter<ItemTag>::count = CopyCounter<WorldTag>::count = 0; }
}  // namespace stats

// ---- what an item can be ------------------------------------------------------------------------
struct Plain {};                       // nothing special: an anvil, a rope
struct Switch { bool on = false; };    // a lamp
enum class Lock { Locked, Closed, Open };
struct Door { Lock lock = Lock::Closed; std::string key; };  // key: the id of the item that locks and unlocks it ("" = none)
using State = std::variant<Plain, Switch, Door>;

struct Item {
    std::string id;     // lower-case: "lamp", "oak-door"
    std::string name;   // "a brass lamp"
    bool portable = false;
    State state;
    [[no_unique_address]] CopyCounter<ItemTag> copies_;
};

struct Exit {
    std::string dir;  // "north"
    std::string to;   // the id of the room it leads to
    std::string via;  // the id of a door item that must be open to pass ("" = free passage)
};

struct Room {
    std::string id;
    std::string description;
    bool dark = false;           // you need a light to see here
    bool win = false;            // entering this room wins the game
    std::vector<Exit> exits;     // in the order of the data file
    std::set<std::string> items; // the ids of the items lying here (sorted)
    [[no_unique_address]] CopyCounter<RoomTag> copies_;
};

struct World {
    std::string start;                           // the id of the first room
    std::map<std::string, Room> rooms;
    std::map<std::string, Item> items;           // every item, by id (where it is: in a Room::items or in `inventory`)
    std::set<std::string> inventory;             // the ids of the items the player carries (sorted)
    std::map<std::string, std::string> aliases;  // word -> what it stands for, e.g. "n" -> "go north"
    [[no_unique_address]] CopyCounter<WorldTag> copies_;
};

// ---- loading ---------------------------------------------------------------------------------------
enum class LoadError {
    None,
    UnknownDirective,  // the first word of a line is not start / alias / room / exit / item
    BadSyntax,         // the line is malformed
    DuplicateId,       // a room, an item, an alias or `start` defined twice (the line of the second one)
    ExitOutsideRoom,   // an exit before any room
    UnknownReference,  // a room or an item that is never defined (the line that mentions it)
    MissingStart,      // no `start` line (line 0)
};

inline const char* name(LoadError e) {
    switch (e) {
        case LoadError::None: return "None";
        case LoadError::UnknownDirective: return "UnknownDirective";
        case LoadError::BadSyntax: return "BadSyntax";
        case LoadError::DuplicateId: return "DuplicateId";
        case LoadError::ExitOutsideRoom: return "ExitOutsideRoom";
        case LoadError::UnknownReference: return "UnknownReference";
        case LoadError::MissingStart: return "MissingStart";
    }
    return "?";
}

struct LoadResult {
    World world;
    LoadError error = LoadError::None;
    std::size_t line = 0;   // 1-based line of the problem (0: no line)
    std::string message;    // never empty when there is an error
    bool ok() const { return error == LoadError::None; }
};

// where an item is when the player carries it
inline constexpr const char* kInventory = "inventory";

// ---- commands: one Action per verb --------------------------------------------------------------------
class Game;

class Action {
public:
    virtual ~Action() = default;
    // `args`: the words after the verb, lower case, the articles (the, a, an) already removed.
    // Returns the text to show the player. May change the game (move the player, move items, change a state).
    virtual std::string perform(Game& game, const std::vector<std::string>& args) const = 0;
};

using ActionTable = std::map<std::string, std::unique_ptr<Action>>;
inline ActionTable make_default_actions();  // defined in actions.hpp

}  // namespace adv
