// MUTANT: pre-increment.
#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// Lambdas and other callables: closures that capture state, std::function to store any of them, and templates to take them without a cost.

// ---- 1. A callback registry: std::function stores ANY callable with a given signature ---------------------------------------------------
// subscribe("topic", handler) returns an id (1, 2, 3, ... never reused). publish calls the handlers of that topic IN SUBSCRIPTION ORDER and
// returns how many it called. unsubscribe(id) removes one handler (false for an unknown id). The registry stores the handlers it is given:
// publish must not COPY them (a std::function copy copies the callable inside it).
class EventBus {
public:
    using Handler = std::function<void(const std::string&)>;

    int subscribe(const std::string& topic, Handler handler) {
        const int id = next_id_++;
        entries_.push_back(Entry{id, topic, std::move(handler)});
        return id;
    }

    bool unsubscribe(int id) {
        const auto it = std::find_if(entries_.begin(), entries_.end(), [id](const Entry& e) { return e.id == id; });
        if (it == entries_.end()) return false;
        entries_.erase(it);
        return true;
    }

    std::size_t publish(const std::string& topic, const std::string& payload) const {
        std::size_t called = 0;
        for (const Entry& e : entries_) {  // const& : no copy of the std::function
            if (e.topic == topic) {
                e.handler(payload);
                ++called;
            }
        }
        return called;
    }

    std::size_t subscriber_count(const std::string& topic) const {
        return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.topic == topic; }));
    }

private:
    struct Entry {
        int id;
        std::string topic;
        Handler handler;
    };
    std::vector<Entry> entries_;
    int next_id_ = 1;
};

// ---- 2. A closure with state: a `mutable` lambda -----------------------------------------------------------------------------------------
// make_counter(5) returns a function that gives 5, then 6, then 7... Each counter has its OWN state. A lambda's operator() is const unless
// the lambda is `mutable`; a copy of a closure copies its state.
inline std::function<int()> make_counter(int start) {
    return [next = start]() mutable { return ++next; };
}

// ---- 3. Capture by VALUE for closures that outlive the function ---------------------------------------------------------------------------
// The i-th function multiplies its argument by factors[i]:  make_multipliers({2, 10})[1](3) is 30.
// A closure that captures a loop variable BY REFERENCE ([&]) dangles as soon as the loop moves on: capture by value, or with an init-capture.
inline std::vector<std::function<int(int)>> make_multipliers(const std::vector<int>& factors) {
    std::vector<std::function<int(int)>> result;
    result.reserve(factors.size());
    for (int factor : factors) result.push_back([factor](int x) { return x * factor; });
    return result;
}

// ---- 4. A generic lambda: compose(f, g)(x) is g(f(x)) ----------------------------------------------------------------------------------------
// The result accepts whatever f accepts (`auto&&...` parameters, forwarded). The closure owns copies of f and g (init-captures).
template <class F, class G>
auto compose(F f, G g) {
    return [f = std::move(f), g = std::move(g)](auto&&... args) { return g(f(std::forward<decltype(args)>(args)...)); };
}

// ---- 5. Taking a callable: a template, not std::function ----------------------------------------------------------------------------------------
// Applies f to x, n times: apply_n(f, 3, x) is f(f(f(x))). The parameter is a forwarding reference: the callable is NOT copied (a std::function
// parameter, or a by-value one, would copy it, allocate, and call through an indirection the compiler cannot inline).
template <class F>
int apply_n(F&& f, int n, int x) {
    for (int i = 0; i < n; ++i) x = f(x);
    return x;
}

// ---- 6. std::function as a parameter AND a result ------------------------------------------------------------------------------------------------
// make_once(f) returns a function that calls f the FIRST time it is called and does nothing after that.
inline std::function<void()> make_once(std::function<void()> f) {
    return [f = std::move(f), called = false]() mutable {
        if (called) return;
        called = true;
        f();
    };
}
