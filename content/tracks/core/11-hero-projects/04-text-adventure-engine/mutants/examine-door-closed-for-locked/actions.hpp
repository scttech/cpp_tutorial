// MUTANT: a locked door is described as closed
#pragma once
// One class per verb, each an Action. They share a few helpers: is it light, can the player reach this item, what does the room look like.
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "game.hpp"
#include "model.hpp"

namespace adv {
namespace detail {

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

inline bool gives_light(const World& w, const std::string& item_id) {
    const auto* sw = std::get_if<Switch>(&w.items.at(item_id).state);
    return sw && sw->on;
}

// A dark room is lit by a lamp that is on, carried or lying in the room.
inline bool is_lit(const Game& game) {
    const World& w = game.world();
    const Room& room = w.rooms.at(game.location());
    if (!room.dark) return true;
    for (const std::string& id : w.inventory) {
        if (gives_light(w, id)) return true;
    }
    for (const std::string& id : room.items) {
        if (gives_light(w, id)) return true;
    }
    return false;
}

inline std::string describe_room(const Game& game) {
    if (!is_lit(game)) return "It is pitch dark.";
    const World& w = game.world();
    const Room& room = w.rooms.at(game.location());
    std::string out = room.description;
    if (!room.items.empty()) {
        out += "\nYou see: ";
        bool first = true;
        for (const std::string& id : room.items) {
            if (!first) out += ", ";
            first = false;
            out += w.items.at(id).name;
        }
        out += ".";
    }
    if (!room.exits.empty()) {
        out += "\nExits: ";
        bool first = true;
        for (const Exit& e : room.exits) {
            if (!first) out += ", ";
            first = false;
            out += e.dir;
        }
        out += ".";
    }
    return out;
}

// The item the player means, if the player can reach it: carried, or lying here while there is light.
// Otherwise nullptr and `problem` says why.
inline Item* locate(Game& game, const std::string& noun, std::string& problem) {
    World& w = game.world();
    const auto it = w.items.find(noun);
    const bool lit = is_lit(game);
    const bool reachable = it != w.items.end() && (w.inventory.count(noun) > 0 || (lit && w.rooms.at(game.location()).items.count(noun) > 0));
    if (!reachable) {
        problem = lit ? "You don't see that here." : "You can't see anything.";
        return nullptr;
    }
    return &it->second;
}

inline std::string state_sentence(const State& state) {
    return std::visit(Overloaded{[](const Plain&) { return std::string(); },
                                 [](const Switch& s) { return std::string(s.on ? " It is on." : " It is off."); },
                                 [](const Door& d) {
                                     switch (d.lock) {
                                         case Lock::Locked: return std::string(" It is closed.");
                                         case Lock::Closed: return std::string(" It is closed.");
                                         case Lock::Open: return std::string(" It is open.");
                                     }
                                     return std::string();
                                 }},
                      state);
}

// the one noun of commands such as "take lamp": a message when there is none or more than one
inline bool one_noun(const std::vector<std::string>& args, const char* question, std::string& reply) {
    if (args.empty()) {
        reply = question;
        return false;
    }
    if (args.size() > 1) {
        reply = "I don't understand.";
        return false;
    }
    return true;
}

}  // namespace detail

class LookAction final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>& args) const override {
        if (args.empty()) return detail::describe_room(game);
        const std::size_t first = args[0] == "at" ? 1 : 0;
        if (args.size() == first) return "What do you want to look at?";
        if (args.size() > first + 1) return "I don't understand.";
        std::string problem;
        const Item* item = detail::locate(game, args[first], problem);
        if (!item) return problem;
        return "This is " + item->name + "." + detail::state_sentence(item->state);
    }
};

class GoAction final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>& args) const override {
        if (args.empty()) return "Where do you want to go?";
        if (args.size() > 1) return "I don't understand.";
        World& w = game.world();
        const Room& here = w.rooms.at(game.location());
        const Exit* exit = nullptr;
        for (const Exit& e : here.exits) {
            if (e.dir == args[0]) exit = &e;
        }
        if (!exit) return "You can't go that way.";
        if (!exit->via.empty()) {
            const Item& blocker = w.items.at(exit->via);
            const auto* door = std::get_if<Door>(&blocker.state);
            if (door && door->lock != Lock::Open) return "The way is blocked by " + blocker.name + ".";
        }
        game.set_location(exit->to);
        std::string text = detail::describe_room(game);
        if (w.rooms.at(exit->to).win) {
            text += "\n*** You have won! ***";
            game.finish();
        }
        return text;
    }
};

class TakeAction final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>& args) const override {
        std::string reply;
        if (!detail::one_noun(args, "What do you want to take?", reply)) return reply;
        Item* item = detail::locate(game, args[0], reply);
        if (!item) return reply;
        World& w = game.world();
        if (w.inventory.count(item->id)) return "You already have that.";
        if (!item->portable) return "You can't take that.";
        w.rooms.at(game.location()).items.erase(item->id);
        w.inventory.insert(item->id);
        return "Taken.";
    }
};

class DropAction final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>& args) const override {
        std::string reply;
        if (!detail::one_noun(args, "What do you want to drop?", reply)) return reply;
        World& w = game.world();
        const auto it = w.inventory.find(args[0]);
        if (it == w.inventory.end()) return "You aren't carrying that.";
        w.rooms.at(game.location()).items.insert(*it);
        w.inventory.erase(it);
        return "Dropped.";
    }
};

class InventoryAction final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>&) const override {
        const World& w = game.world();
        if (w.inventory.empty()) return "You are carrying nothing.";
        std::string out = "You are carrying: ";
        bool first = true;
        for (const std::string& id : w.inventory) {
            if (!first) out += ", ";
            first = false;
            out += w.items.at(id).name;
        }
        return out + ".";
    }
};

class OpenAction final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>& args) const override {
        std::string reply;
        if (!detail::one_noun(args, "What do you want to open?", reply)) return reply;
        Item* item = detail::locate(game, args[0], reply);
        if (!item) return reply;
        auto* door = std::get_if<Door>(&item->state);
        if (!door) return "You can't open that.";
        if (door->lock == Lock::Open) return "It is already open.";
        if (door->lock == Lock::Locked) return "It is locked.";
        door->lock = Lock::Open;
        return "You open it.";
    }
};

class CloseAction final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>& args) const override {
        std::string reply;
        if (!detail::one_noun(args, "What do you want to close?", reply)) return reply;
        Item* item = detail::locate(game, args[0], reply);
        if (!item) return reply;
        auto* door = std::get_if<Door>(&item->state);
        if (!door) return "You can't close that.";
        if (door->lock != Lock::Open) return "It is already closed.";
        door->lock = Lock::Closed;
        return "You close it.";
    }
};

// "unlock door with key" and "lock door with key": the parts that are the same
namespace detail {
struct DoorAndKey {
    Item* door = nullptr;
    const Item* key = nullptr;
    std::string reply;  // set when something is wrong
};
inline DoorAndKey door_and_key(Game& game, const std::vector<std::string>& args, const char* question, const char* missing_key) {
    DoorAndKey result;
    if (args.empty()) {
        result.reply = question;
    } else if (args.size() == 1) {
        result.reply = missing_key;
    } else if (args.size() != 3 || args[1] != "with") {
        result.reply = "I don't understand.";
    } else {
        result.door = locate(game, args[0], result.reply);
        if (result.door) {
            const World& w = game.world();
            if (w.inventory.count(args[2]) == 0) {
                result.reply = "You aren't carrying that.";
                result.door = nullptr;
            } else {
                result.key = &w.items.at(args[2]);
            }
        }
    }
    return result;
}
}  // namespace detail

class UnlockAction final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>& args) const override {
        detail::DoorAndKey dk = detail::door_and_key(game, args, "What do you want to unlock?", "Unlock it with what?");
        if (!dk.door) return dk.reply;
        auto* door = std::get_if<Door>(&dk.door->state);
        if (!door) return "You can't unlock that.";
        if (door->lock != Lock::Locked) return "It isn't locked.";
        if (door->key != dk.key->id) return "That doesn't fit.";
        door->lock = Lock::Closed;
        return "You unlock it.";
    }
};

class LockAction final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>& args) const override {
        detail::DoorAndKey dk = detail::door_and_key(game, args, "What do you want to lock?", "Lock it with what?");
        if (!dk.door) return dk.reply;
        auto* door = std::get_if<Door>(&dk.door->state);
        if (!door) return "You can't lock that.";
        if (door->lock == Lock::Open) return "Close it first.";
        if (door->lock == Lock::Locked) return "It is already locked.";
        if (door->key != dk.key->id) return "That doesn't fit.";
        door->lock = Lock::Locked;
        return "You lock it.";
    }
};

class TurnAction final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>& args) const override {
        if (args.empty()) return "Turn what on or off?";
        if (args.size() == 1) return (args[0] == "on" || args[0] == "off") ? "Turn what on or off?" : "Turn it on or off?";
        if (args.size() > 2) return "I don't understand.";
        const bool on_first = args[0] == "on" || args[0] == "off";
        const std::string& mode = on_first ? args[0] : args[1];
        const std::string& noun = on_first ? args[1] : args[0];
        if (mode != "on" && mode != "off") return "I don't understand.";
        std::string reply;
        Item* item = detail::locate(game, noun, reply);
        if (!item) return reply;
        auto* sw = std::get_if<Switch>(&item->state);
        if (!sw) return "You can't switch that.";
        const bool want = mode == "on";
        if (sw->on == want) return want ? "It is already on." : "It is already off.";
        sw->on = want;
        return want ? "You turn it on." : "You turn it off.";
    }
};

inline ActionTable make_default_actions() {
    ActionTable table;
    table.emplace("look", std::make_unique<LookAction>());
    table.emplace("go", std::make_unique<GoAction>());
    table.emplace("take", std::make_unique<TakeAction>());
    table.emplace("drop", std::make_unique<DropAction>());
    table.emplace("inventory", std::make_unique<InventoryAction>());
    table.emplace("open", std::make_unique<OpenAction>());
    table.emplace("close", std::make_unique<CloseAction>());
    table.emplace("unlock", std::make_unique<UnlockAction>());
    table.emplace("lock", std::make_unique<LockAction>());
    table.emplace("turn", std::make_unique<TurnAction>());
    return table;
}

}  // namespace adv
