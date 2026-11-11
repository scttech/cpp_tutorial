#pragma once
// YOUR ACTIONS: one class per verb, each derived from Action (see model.hpp), and make_default_actions() that builds the table.
// Verbs: look go take drop inventory open close unlock lock turn. (Synonyms such as get / i / x / n are aliases in the world data.)
// The exact messages are in the lesson.
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "game.hpp"
#include "model.hpp"

namespace adv {

// TODO: describe the current room: the description, "You see: ..." (items by id), "Exits: ..." (in file order);
// "It is pitch dark." in a dark room without a light.
class LookAction final : public Action {
public:
    std::string perform(Game& game, const std::vector<std::string>& args) const override {
        (void)args;
        return game.world().rooms.at(game.location()).description;
    }
};

inline ActionTable make_default_actions() {
    ActionTable table;
    table.emplace("look", std::make_unique<LookAction>());
    // TODO: go take drop inventory open close unlock lock turn
    return table;
}

}  // namespace adv
