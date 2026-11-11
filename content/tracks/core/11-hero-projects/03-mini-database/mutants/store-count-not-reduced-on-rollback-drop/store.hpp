// MUTANT: COUNT keeps the value of a key that ROLLBACK removed
#pragma once
// Store: an ordered map for the data, a hash index for COUNT, and an undo log per open transaction.
// BEGIN is free (an empty log); every change inside a transaction records how to undo itself; ROLLBACK replays the log backwards.
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace db {

class Store {
public:
    void set(Text key, Text value) {
        if (!txns_.empty()) {
            const auto it = data_.find(key);
            txns_.back().push_back(it == data_.end() ? Undo{key, std::nullopt} : Undo{key, it->second});
        }
        put(std::move(key), std::move(value));
    }

    const Text* get(const Text& key) const {
        const auto it = data_.find(key);
        return it == data_.end() ? nullptr : &it->second;
    }

    bool del(const Text& key) {
        const auto it = data_.find(key);
        if (it == data_.end()) return false;
        if (!txns_.empty()) txns_.back().push_back(Undo{it->first, it->second});
        unindex(it->second);
        data_.erase(it);
        return true;
    }

    std::size_t count(const Text& value) const {
        const auto it = counts_.find(value);
        return it == counts_.end() ? 0 : it->second;
    }

    std::vector<Text> range(const Text& lo, const Text& hi) const {
        std::vector<Text> keys;
        if (hi < lo) return keys;
        for (auto it = data_.lower_bound(lo); it != data_.end() && !(hi < it->first); ++it) keys.push_back(it->first);
        return keys;
    }

    void begin() { txns_.emplace_back(); }

    bool rollback() {
        if (txns_.empty()) return false;
        std::vector<Undo> log = std::move(txns_.back());
        txns_.pop_back();
        for (auto it = log.rbegin(); it != log.rend(); ++it) {
            if (it->old_value) put(std::move(it->key), std::move(*it->old_value));
            else drop(it->key);
        }
        return true;
    }

    bool commit() {
        if (txns_.empty()) return false;
        txns_.clear();
        return true;
    }

    std::size_t size() const { return data_.size(); }
    std::size_t depth() const { return txns_.size(); }

private:
    struct Undo {
        Text key;
        std::optional<Text> old_value;  // empty: the key did not exist before
    };

    std::map<Text, Text> data_;
    std::unordered_map<Text, std::size_t> counts_;  // value -> how many keys hold it
    std::vector<std::vector<Undo>> txns_;

    void index(const Text& value) { ++counts_[value]; }
    void unindex(const Text& value) {
        const auto it = counts_.find(value);
        if (it != counts_.end() && --it->second == 0) counts_.erase(it);
    }

    // store without logging
    void put(Text key, Text value) {
        const auto it = data_.lower_bound(key);
        if (it != data_.end() && !(key < it->first)) {
            unindex(it->second);
            index(value);
            it->second = std::move(value);
        } else {
            index(value);
            data_.emplace_hint(it, std::move(key), std::move(value));
        }
    }
    // erase without logging
    void drop(const Text& key) {
        const auto it = data_.find(key);
        if (it == data_.end()) return;
        data_.erase(it);
    }
};

}  // namespace db
