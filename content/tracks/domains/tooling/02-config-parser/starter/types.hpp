#pragma once
// Read-only: the values a configuration file can hold, and the container the parser fills.
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace conf {

struct Value {
    enum class Type { String, Int, Float, Bool, Array };
    Type type = Type::String;
    std::string s;
    long long i = 0;
    double d = 0;
    bool b = false;
    std::vector<Value> a;

    static Value str(std::string v) {
        Value x;
        x.type = Type::String;
        x.s = std::move(v);
        return x;
    }
    static Value integer(long long v) {
        Value x;
        x.type = Type::Int;
        x.i = v;
        return x;
    }
    static Value real(double v) {
        Value x;
        x.type = Type::Float;
        x.d = v;
        return x;
    }
    static Value boolean(bool v) {
        Value x;
        x.type = Type::Bool;
        x.b = v;
        return x;
    }
    static Value array(std::vector<Value> v) {
        Value x;
        x.type = Type::Array;
        x.a = std::move(v);
        return x;
    }
};

inline bool operator==(const Value& x, const Value& y) {
    if (x.type != y.type) return false;
    switch (x.type) {
        case Value::Type::String: return x.s == y.s;
        case Value::Type::Int: return x.i == y.i;
        case Value::Type::Float: return x.d == y.d;
        case Value::Type::Bool: return x.b == y.b;
        case Value::Type::Array: return x.a == y.a;
    }
    return false;
}

// The parsed document: values by dotted path ("server.port"), in the order they appeared, plus the set of section names seen.
class Config {
public:
    struct Entry {
        std::string path;
        Value value;
    };
    // false (and nothing added) if the path is already taken
    bool add(const std::string& path, Value v) {
        if (index_.count(path)) return false;
        index_.emplace(path, entries_.size());
        entries_.push_back(Entry{path, std::move(v)});
        return true;
    }
    // false if a section of this name was already declared
    bool add_section(const std::string& name) { return sections_.emplace(name, sections_.size()).second; }
    bool has_section(const std::string& name) const { return sections_.count(name) != 0; }
    const Value* find(const std::string& path) const {
        const auto it = index_.find(path);
        return it == index_.end() ? nullptr : &entries_[it->second].value;
    }
    const std::vector<Entry>& entries() const { return entries_; }
    std::optional<std::string> get_string(const std::string& p) const { return get<std::string>(p, Value::Type::String, &Value::s); }
    std::optional<long long> get_int(const std::string& p) const { return get<long long>(p, Value::Type::Int, &Value::i); }
    std::optional<bool> get_bool(const std::string& p) const { return get<bool>(p, Value::Type::Bool, &Value::b); }
    // an integer is accepted where a float is wanted
    std::optional<double> get_double(const std::string& p) const {
        const Value* v = find(p);
        if (!v) return std::nullopt;
        if (v->type == Value::Type::Float) return v->d;
        if (v->type == Value::Type::Int) return static_cast<double>(v->i);
        return std::nullopt;
    }

private:
    template <class T>
    std::optional<T> get(const std::string& p, Value::Type t, T Value::* member) const {
        const Value* v = find(p);
        if (!v || v->type != t) return std::nullopt;
        return v->*member;
    }
    std::vector<Entry> entries_;
    std::unordered_map<std::string, std::size_t> index_;
    std::unordered_map<std::string, std::size_t> sections_;
};

}  // namespace conf
