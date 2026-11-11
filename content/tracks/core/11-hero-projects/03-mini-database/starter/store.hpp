#pragma once
// YOUR STORE. Keys and values are Text (see types.hpp). Keep the keys ordered; keep COUNT fast; make transactions cheap.
//
//   set(key, value)   store or overwrite                  get(key)       pointer to the value, or nullptr
//   del(key)          true if the key existed             count(value)   how many keys hold exactly this value
//   range(lo, hi)     all keys with lo <= key <= hi, ascending (empty when lo > hi)
//   begin()           open a transaction (they nest)
//   rollback()        undo the changes of the innermost transaction; false if none is open
//   commit()          make the changes of ALL open transactions permanent; false if none is open
//   size(), depth()   number of keys, number of open transactions
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace db {

class Store {
public:
    void set(Text key, Text value) { data_[std::move(key)] = std::move(value); }  // TODO: also the COUNT index and the undo log

    const Text* get(const Text& key) const {
        const auto it = data_.find(key);
        return it == data_.end() ? nullptr : &it->second;
    }

    bool del(const Text& key) { return data_.erase(key) > 0; }  // TODO

    std::size_t count(const Text& value) const {  // TODO: this is a slow scan
        std::size_t n = 0;
        for (const auto& kv : data_) n += kv.second == value ? 1 : 0;
        return n;
    }

    std::vector<Text> range(const Text& lo, const Text& hi) const {  // TODO: this is a slow scan
        std::vector<Text> keys;
        for (const auto& kv : data_) {
            if (!(kv.first < lo) && !(hi < kv.first)) keys.push_back(kv.first);
        }
        return keys;
    }

    void begin() {}                      // TODO
    bool rollback() { return false; }    // TODO
    bool commit() { return false; }      // TODO

    std::size_t size() const { return data_.size(); }
    std::size_t depth() const { return 0; }  // TODO

private:
    std::map<Text, Text> data_;
};

}  // namespace db
