#pragma once
// YOUR DATABASE: the parser and the store, put together. execute(line) returns the Reply for one command line.
//   SET -> OK            GET -> the value, or nil        DEL -> (integer) 1 if the key existed, else 0
//   COUNT -> (integer)   RANGE -> the list of keys       BEGIN -> OK
//   ROLLBACK / COMMIT -> OK, or the error "NO TRANSACTION" when none is open
//   INCR key -> the new (integer) value; a missing key counts as 0. A value that is not a whole number
//      ("abc", "+5", " 5", "", too big for a long long) is the error "value is not an integer", the largest long long is
//      "increment would overflow"; in both cases nothing changes.
//   A line that does not parse is an error reply with the parser's message (and changes nothing).
#include <charconv>
#include <climits>
#include <string>
#include <string_view>
#include <variant>

#include "query.hpp"
#include "store.hpp"
#include "types.hpp"

namespace db {

class Database {
public:
    Reply execute(std::string_view line) {
        ParsedCommand parsed = parse_command(line);
        if (!parsed.ok()) return Reply::error(parsed.message);
        if (auto* set = std::get_if<Set>(&parsed.command)) {
            store_.set(std::move(set->key), std::move(set->value));
            return Reply::ok();
        }
        if (auto* get = std::get_if<Get>(&parsed.command)) {
            const Text* value = store_.get(get->key);
            return value ? Reply::value(value->s) : Reply::nil();
        }
        return Reply::error("not implemented yet");  // TODO: DEL COUNT RANGE INCR BEGIN ROLLBACK COMMIT
    }

    Store& store() { return store_; }

private:
    Store store_;
};

}  // namespace db
