// MUTANT: SET copies the key and value out of the command
#pragma once
// Database: parse a line, run the command on the store, build the reply. std::visit picks the code for each command type.
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
        return std::visit([this](auto& command) { return run(command); }, parsed.command);
    }

    Store& store() { return store_; }

private:
    Store store_;

    Reply run(Set& c) {
        store_.set(c.key, c.value);
        return Reply::ok();
    }
    Reply run(Get& c) {
        const Text* value = store_.get(c.key);
        return value ? Reply::value(value->s) : Reply::nil();
    }
    Reply run(Del& c) { return Reply::integer(store_.del(c.key) ? 1 : 0); }
    Reply run(Count& c) { return Reply::integer(static_cast<long long>(store_.count(c.value))); }
    Reply run(Range& c) {
        std::vector<std::string> names;
        for (Text& key : store_.range(c.lo, c.hi)) names.push_back(std::move(key.s));
        return Reply::list(std::move(names));
    }
    Reply run(Incr& c) {
        long long n = 0;
        if (const Text* current = store_.get(c.key)) {
            const char* first = current->s.data();
            const char* last = first + current->s.size();
            const auto [end, ec] = std::from_chars(first, last, n);
            if (ec != std::errc() || end != last) return Reply::error("value is not an integer");
        }
        if (n == LLONG_MAX) return Reply::error("increment would overflow");
        ++n;
        store_.set(std::move(c.key), Text(std::to_string(n)));
        return Reply::integer(n);
    }
    Reply run(Begin&) {
        store_.begin();
        return Reply::ok();
    }
    Reply run(Rollback&) { return store_.rollback() ? Reply::ok() : Reply::error("NO TRANSACTION"); }
    Reply run(Commit&) { return store_.commit() ? Reply::ok() : Reply::error("NO TRANSACTION"); }
};

}  // namespace db
