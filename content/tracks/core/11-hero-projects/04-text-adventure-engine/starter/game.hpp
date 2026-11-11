#pragma once
// YOUR GAME: the state of one play-through (the world, where the player is, whether it is over) and the dispatch of commands.
// The game itself knows NO verbs: every verb is an Action in the table, so that new verbs can be added without touching this file.
//
//   Game(World)                    takes the world (moved, not copied); the player starts in world.start
//   start()                        the text for the beginning: the same as the command "look"
//   execute(line)                  one command line -> the reply (see the lesson for the steps and the exact messages)
//   add_action(verb, action)       register (or replace) the Action for a verb
//   world() / location() / set_location(id) / finished() / finish()
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "model.hpp"

namespace adv {

class Game {
public:
    explicit Game(World world) : world_(std::move(world)), location_(world_.start), actions_(make_default_actions()) {}

    std::string start() { return execute("look"); }

    std::string execute(std::string_view line) {
        (void)line;
        return "not implemented yet";  // TODO: finished? empty? alias? find the action? remove the articles? perform
    }

    void add_action(std::string verb, std::unique_ptr<Action> action) { actions_[std::move(verb)] = std::move(action); }

    World& world() { return world_; }
    const World& world() const { return world_; }
    const std::string& location() const { return location_; }
    void set_location(std::string room_id) { location_ = std::move(room_id); }
    bool finished() const { return finished_; }
    void finish() { finished_ = true; }

private:
    World world_;
    std::string location_;
    bool finished_ = false;
    ActionTable actions_;
};

}  // namespace adv

#include "actions.hpp"  // after the class: the actions need the complete Game, and the Game needs make_default_actions()
