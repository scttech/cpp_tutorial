// MUTANT: an exit via an unknown item is accepted
#pragma once
// load_world(text): one pass over the lines, each split into words (a "quoted text" is one word); references to ids that
// appear later in the file are collected and checked at the end.
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "model.hpp"

namespace adv {
namespace detail {

struct Word {
    std::string text;
    bool quoted = false;
};

inline bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r'; }

inline bool valid_id(std::string_view s) {
    if (s.empty()) return false;
    for (const char c : s) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!ok) return false;
    }
    return true;
}

// false: an unterminated quote, or a quote glued to other text
inline bool split_words(std::string_view line, std::vector<Word>& out) {
    std::size_t i = 0;
    const std::size_t n = line.size();
    while (true) {
        while (i < n && is_space(line[i])) ++i;
        if (i >= n) return true;
        Word w;
        if (line[i] == '"') {
            const std::size_t close = line.find('"', i + 1);
            if (close == std::string_view::npos) return false;
            w.text = std::string(line.substr(i + 1, close - i - 1));
            w.quoted = true;
            i = close + 1;
            if (i < n && !is_space(line[i])) return false;
        } else {
            const std::size_t start = i;
            while (i < n && !is_space(line[i]) && line[i] != '"') ++i;
            if (i < n && line[i] == '"') return false;
            w.text = std::string(line.substr(start, i - start));
        }
        out.push_back(std::move(w));
    }
}

enum class RefKind { Room, Item, Place };
struct Reference {
    RefKind kind;
    std::string id;
    std::size_t line;
    const char* message;
};

}  // namespace detail

inline LoadResult load_world(std::string_view text) {
    using detail::RefKind;
    using detail::Word;

    LoadResult r;
    World& w = r.world;
    std::vector<detail::Reference> refs;
    std::vector<std::pair<std::string, std::string>> places;  // (item id, room id or inventory)
    std::string current_room;
    std::size_t start_line = 0;

    auto fail = [&r](LoadError e, std::size_t line, const char* message) -> LoadResult& {
        r.error = e;
        r.line = line;
        r.message = message;
        return r;
    };

    std::vector<Word> words;  // reused for every line
    std::size_t line_no = 0;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        const std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        ++line_no;

        const std::size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string_view::npos || line[first] == '#') continue;
        words.clear();
        if (!detail::split_words(line, words)) return fail(LoadError::BadSyntax, line_no, "a quote is not closed, or is glued to other text");
        const std::string& directive = words[0].text;

        if (directive == "start") {
            if (words.size() != 2 || words[1].quoted || !detail::valid_id(words[1].text)) return fail(LoadError::BadSyntax, line_no, "start needs one room id");
            if (!w.start.empty()) return fail(LoadError::DuplicateId, line_no, "there is already a start line");
            w.start = words[1].text;
            start_line = line_no;
            refs.push_back({RefKind::Room, w.start, line_no, "the start room is never defined"});
        } else if (directive == "alias") {
            if (words.size() < 3) return fail(LoadError::BadSyntax, line_no, "alias needs a word and what it stands for");
            std::string expansion;
            for (std::size_t i = 1; i < words.size(); ++i) {
                if (words[i].quoted || !detail::valid_id(words[i].text)) return fail(LoadError::BadSyntax, line_no, "an alias is made of lower-case words");
                if (i > 2) expansion += ' ';
                if (i > 1) expansion += words[i].text;
            }
            if (!w.aliases.emplace(words[1].text, std::move(expansion)).second) return fail(LoadError::DuplicateId, line_no, "this alias is already defined");
        } else if (directive == "room") {
            if (words.size() < 3 || words[1].quoted || !detail::valid_id(words[1].text) || !words[2].quoted) {
                return fail(LoadError::BadSyntax, line_no, "room needs an id and a \"description\"");
            }
            if (words[1].text == kInventory) return fail(LoadError::BadSyntax, line_no, "\"inventory\" is not a room");
            Room room;
            room.id = words[1].text;
            room.description = words[2].text;
            bool seen_dark = false, seen_win = false;
            for (std::size_t i = 3; i < words.size(); ++i) {
                if (!words[i].quoted && words[i].text == "dark" && !seen_dark) seen_dark = room.dark = true;
                else if (!words[i].quoted && words[i].text == "win" && !seen_win) seen_win = room.win = true;
                else return fail(LoadError::BadSyntax, line_no, "after the description only the flags dark and win (once each) are allowed");
            }
            if (w.rooms.count(room.id)) return fail(LoadError::DuplicateId, line_no, "this room is already defined");
            current_room = room.id;
            w.rooms.emplace(room.id, std::move(room));
        } else if (directive == "exit") {
            if (current_room.empty()) return fail(LoadError::ExitOutsideRoom, line_no, "an exit must come after a room");
            const bool via = words.size() == 5;
            if ((words.size() != 3 && !via) || (via && words[3].text != "via")) return fail(LoadError::BadSyntax, line_no, "exit <direction> <room> [via <item>]");
            for (const Word& word : words) {
                if (word.quoted) return fail(LoadError::BadSyntax, line_no, "no quotes in an exit");
            }
            if (!detail::valid_id(words[1].text) || !detail::valid_id(words[2].text) || (via && !detail::valid_id(words[4].text))) {
                return fail(LoadError::BadSyntax, line_no, "ids are lower-case words");
            }
            Room& room = w.rooms.at(current_room);
            for (const Exit& e : room.exits) {
                if (e.dir == words[1].text) return fail(LoadError::BadSyntax, line_no, "this room already has an exit in that direction");
            }
            Exit e;
            e.dir = words[1].text;
            e.to = words[2].text;
            if (via) e.via = words[4].text;
            refs.push_back({RefKind::Room, e.to, line_no, "this exit leads to a room that is never defined"});
            room.exits.push_back(std::move(e));
        } else if (directive == "item") {
            if (words.size() < 5 || words[1].quoted || !detail::valid_id(words[1].text) || !words[2].quoted || words[3].quoted || words[3].text != "in" ||
                words[4].quoted || !detail::valid_id(words[4].text)) {
                return fail(LoadError::BadSyntax, line_no, "item <id> \"<name>\" in <room or inventory> [flags]");
            }
            Item item;
            item.id = words[1].text;
            item.name = words[2].text;
            const std::string place = words[4].text;
            bool seen_portable = false, seen_switch = false, seen_door = false;
            for (std::size_t i = 5; i < words.size(); ++i) {
                if (words[i].quoted) return fail(LoadError::BadSyntax, line_no, "no quotes after the place");
                const std::string& flag = words[i].text;
                if (flag == "portable" && !seen_portable) {
                    seen_portable = item.portable = true;
                } else if (flag == "switch" && !seen_switch && !seen_door) {
                    if (i + 1 >= words.size() || (words[i + 1].text != "on" && words[i + 1].text != "off") || words[i + 1].quoted) {
                        return fail(LoadError::BadSyntax, line_no, "switch needs on or off");
                    }
                    item.state = Switch{words[i + 1].text == "on"};
                    seen_switch = true;
                    ++i;
                } else if (flag == "door" && !seen_door && !seen_switch) {
                    if (i + 1 >= words.size() || words[i + 1].quoted) return fail(LoadError::BadSyntax, line_no, "door needs locked, closed or open");
                    const std::string& lock = words[i + 1].text;
                    Door door;
                    if (lock == "locked") door.lock = Lock::Locked;
                    else if (lock == "closed") door.lock = Lock::Closed;
                    else if (lock == "open") door.lock = Lock::Open;
                    else return fail(LoadError::BadSyntax, line_no, "door needs locked, closed or open");
                    i += 1;
                    if (i + 1 < words.size() && !words[i + 1].quoted && words[i + 1].text == "key") {
                        if (i + 2 >= words.size() || words[i + 2].quoted || !detail::valid_id(words[i + 2].text)) return fail(LoadError::BadSyntax, line_no, "key needs an item id");
                        door.key = words[i + 2].text;
                        refs.push_back({RefKind::Item, door.key, line_no, "this door needs a key item that is never defined"});
                        i += 2;
                    }
                    item.state = door;
                    seen_door = true;
                } else {
                    return fail(LoadError::BadSyntax, line_no, "unknown or repeated flag (portable, switch on|off, door locked|closed|open [key <item>])");
                }
            }
            if (w.items.count(item.id)) return fail(LoadError::DuplicateId, line_no, "this item is already defined");
            refs.push_back({RefKind::Place, place, line_no, "this item is in a room that is never defined"});
            places.emplace_back(item.id, place);  // where it lies is filled in after all rooms are known (a room may be defined later)
            w.items.emplace(item.id, std::move(item));
        } else {
            return fail(LoadError::UnknownDirective, line_no, "unknown directive (start, alias, room, exit, item)");
        }
    }

    // references to ids that are defined later in the file: report the one on the smallest line
    const detail::Reference* worst = nullptr;
    for (const detail::Reference& ref : refs) {
        const bool known = ref.kind == RefKind::Item ? w.items.count(ref.id) > 0
                           : ref.kind == RefKind::Room ? w.rooms.count(ref.id) > 0
                                                       : (ref.id == kInventory || w.rooms.count(ref.id) > 0);
        if (!known && (!worst || ref.line < worst->line)) worst = &ref;
    }
    if (worst) return fail(LoadError::UnknownReference, worst->line, worst->message);
    if (w.start.empty()) return fail(LoadError::MissingStart, 0, "there is no start line");
    (void)start_line;

    for (const auto& [item_id, place] : places) {
        if (place == kInventory) w.inventory.insert(item_id);
        else w.rooms.at(place).items.insert(item_id);
    }
    return r;
}

}  // namespace adv
