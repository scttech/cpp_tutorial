// MUTANT: add_action does not replace an existing verb
#pragma once
// Game: the state of one play-through, and the dispatch of a command line to the Action registered for its verb.
// This file contains no verbs: new verbs are new Actions in the table.
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "model.hpp"

namespace adv {
namespace detail {

// lower-case words, separated by blanks
inline std::vector<std::string> split_lower(std::string_view line) {
    std::vector<std::string> words;
    std::string current;
    for (const char c : line) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!current.empty()) {
                words.push_back(std::move(current));
                current.clear();
            }
        } else {
            current.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
        }
    }
    if (!current.empty()) words.push_back(std::move(current));
    return words;
}

inline bool is_article(const std::string& w) { return w == "the" || w == "a" || w == "an"; }

}  // namespace detail

class Game {
public:
    explicit Game(World world) : world_(std::move(world)), location_(world_.start), actions_(make_default_actions()) {}

    std::string start() { return execute("look"); }

    std::string execute(std::string_view line) {
        if (finished_) return "The game is over.";
        std::vector<std::string> words = detail::split_lower(line);
        if (words.empty()) return "Say something.";
        const auto alias = world_.aliases.find(words[0]);
        if (alias != world_.aliases.end()) {  // the alias is replaced by what it stands for, once
            std::vector<std::string> expanded = detail::split_lower(alias->second);
            expanded.insert(expanded.end(), std::make_move_iterator(words.begin() + 1), std::make_move_iterator(words.end()));
            words = std::move(expanded);
        }
        const auto action = actions_.find(words[0]);
        if (action == actions_.end()) return "I don't understand.";
        std::vector<std::string> args;
        for (std::size_t i = 1; i < words.size(); ++i) {
            if (!detail::is_article(words[i])) args.push_back(std::move(words[i]));
        }
        return action->second->perform(*this, args);
    }

    void add_action(std::string verb, std::unique_ptr<Action> action) { actions_.emplace(std::move(verb), std::move(action)); }

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
