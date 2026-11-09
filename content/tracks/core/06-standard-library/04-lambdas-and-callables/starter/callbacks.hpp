#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// Lambdas and other callables: closures that capture state, std::function to store any of them, and templates to take them without a cost.
// Every function below is a stub that compiles but is wrong: replace the bodies.

// ---- 1. A callback registry: std::function stores ANY callable with a given signature ---------------------------------------------------
// TODO: subscribe("topic", handler) returns an id (1, 2, 3, ... never reused). publish calls the handlers of that topic IN SUBSCRIPTION ORDER and
// returns how many it called. unsubscribe(id) removes one handler (false for an unknown id). The registry stores the handlers it is given:
// publish must not COPY them (a std::function copy copies the callable inside it): iterate with `const Entry&`.
class EventBus {
public:
    using Handler = std::function<void(const std::string&)>;

    // TODO: store {id, topic, handler} (std::move the handler into place) and return the new id.
    int subscribe(const std::string& topic, Handler handler) {
        (void)topic;
        (void)handler;
        return 0;
    }

    // TODO: remove the entry with that id; false if there is none.
    bool unsubscribe(int id) {
        (void)id;
        return false;
    }

    // TODO: call every handler subscribed to `topic`, in order, with the payload; return how many.
    std::size_t publish(const std::string& topic, const std::string& payload) const {
        (void)topic;
        (void)payload;
        return 0;
    }

    // TODO: how many handlers are subscribed to `topic`.
    std::size_t subscriber_count(const std::string& topic) const {
        (void)topic;
        return 0;
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
// TODO: make_counter(5) returns a function that gives 5, then 6, then 7... Each counter has its OWN state. A lambda's operator() is const unless
// the lambda is `mutable`; use an init-capture:  [next = start]() mutable { return next++; }
inline std::function<int()> make_counter(int start) {
    (void)start;
    return [] { return 0; };
}

// ---- 3. Capture by VALUE for closures that outlive the function ---------------------------------------------------------------------------
// TODO: the i-th function multiplies its argument by factors[i]:  make_multipliers({2, 10})[1](3) is 30.
// A closure that captures a loop variable BY REFERENCE ([&]) dangles as soon as the loop moves on: capture by value.
inline std::vector<std::function<int(int)>> make_multipliers(const std::vector<int>& factors) {
    (void)factors;
    return {};
}

// ---- 4. A generic lambda: compose(f, g)(x) is g(f(x)) ----------------------------------------------------------------------------------------
// TODO: return a lambda that accepts whatever f accepts (`auto&&... args`, forwarded) and returns g(f(args...)).
// The closure owns copies of f and g (init-captures: [f = std::move(f), g = std::move(g)]).
template <class F, class G>
auto compose(F f, G g) {
    (void)f;
    (void)g;
    return [](auto&&...) { return 0; };
}

// ---- 5. Taking a callable: a template, not std::function ----------------------------------------------------------------------------------------
// TODO: apply f to x, n times: apply_n(f, 3, x) is f(f(f(x))). The parameter is a forwarding reference (F&&): the callable is NOT copied
// (a std::function parameter, or a by-value one, would copy it, allocate, and call through an indirection the compiler cannot inline).
template <class F>
int apply_n(F&& f, int n, int x) {
    (void)f;
    (void)n;
    return x;
}

// ---- 6. std::function as a parameter AND a result ------------------------------------------------------------------------------------------------
// TODO: make_once(f) returns a function that calls f the FIRST time it is called and does nothing after that. (A mutable lambda with a bool.)
inline std::function<void()> make_once(std::function<void()> f) {
    (void)f;
    return [] {};
}
