// MUTANT: ^ matches everywhere
#pragma once
// The matcher: a Pike-style simulation. A "thread" is an NFA state together with the offset where its match started. For every input
// byte the set of threads is advanced once, so the work is proportional to (text length) x (NFA size) whatever the pattern is.
// Leftmost-longest: threads are kept in order of their start; a state reached from two starts keeps the earlier one; when a Match is
// reached the start is fixed, later starts are dropped and the search goes on only to find the longest end for that start.
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "types.hpp"

namespace rx {
namespace detail {

struct Thread {
    std::size_t state;
    std::size_t start;
};

// the states that are currently possible (consuming states and Match only), each at most once
struct ThreadList {
    std::vector<Thread> threads;
    std::vector<std::size_t> seen;  // seen[state] == stamp: already in this list for this position
    explicit ThreadList(std::size_t n) : seen(n, kNone) {}
};

class Simulation {
public:
    Simulation(const Nfa& nfa, std::string_view text) : nfa_(nfa), text_(text) {}

    // add the state and everything reachable from it without consuming input (the epsilon closure)
    void close(ThreadList& list, std::size_t first, std::size_t start, std::size_t pos) {
        stack_.push_back(first);
        while (!stack_.empty()) {
            const std::size_t s = stack_.back();
            stack_.pop_back();
            if (list.seen[s] == pos) continue;
            list.seen[s] = pos;
            const State& st = nfa_.at(s);
            switch (st.op) {
                case Op::Split:
                    stack_.push_back(st.out1);
                    stack_.push_back(st.out);
                    break;
                case Op::Jump: stack_.push_back(st.out); break;
                case Op::Begin: stack_.push_back(st.out); break;
                case Op::End:
                    if (pos == text_.size()) stack_.push_back(st.out);
                    break;
                default: list.threads.push_back({s, start});
            }
        }
    }

    // consume text_[pos] with every thread of `cur`; the survivors go to `next` (positioned at pos + 1)
    // `limit`: threads that started after this offset are not worth following (a match with an earlier start exists)
    void step(const ThreadList& cur, ThreadList& next, std::size_t pos, std::size_t limit) {
        const unsigned char c = static_cast<unsigned char>(text_[pos]);
        for (const Thread& t : cur.threads) {
            if (t.start > limit) break;
            const State& st = nfa_.at(t.state);
            bool takes = false;
            switch (st.op) {
                case Op::Char: takes = st.ch == c; break;
                case Op::Any: takes = c != '\n'; break;
                case Op::Set: takes = nfa_.set(st.set).test(c); break;
                default: break;
            }
            if (takes) close(next, st.out, t.start, pos + 1);
        }
    }

    const Nfa& nfa() const { return nfa_; }

private:
    const Nfa& nfa_;
    std::string_view text_;
    std::vector<std::size_t> stack_;
};

}  // namespace detail

inline bool nfa_full_match(const Nfa& nfa, std::string_view text) {
    detail::Simulation sim(nfa, text);
    detail::ThreadList cur(nfa.size()), next(nfa.size());
    sim.close(cur, nfa.start(), 0, 0);
    for (std::size_t pos = 0; pos < text.size(); ++pos) {
        next.threads.clear();
        sim.step(cur, next, pos, kNone);
        std::swap(cur, next);
        if (cur.threads.empty()) return false;
    }
    for (const detail::Thread& t : cur.threads) {
        if (nfa.at(t.state).op == Op::Match) return true;
    }
    return false;
}

inline std::optional<Match> nfa_search(const Nfa& nfa, std::string_view text, std::size_t from) {
    if (from > text.size()) return std::nullopt;
    detail::Simulation sim(nfa, text);
    detail::ThreadList cur(nfa.size()), next(nfa.size());
    std::optional<Match> best;
    for (std::size_t pos = from;; ++pos) {
        if (!best) sim.close(cur, nfa.start(), pos, pos);  // a new thread that starts here (after all the older ones)
        for (const detail::Thread& t : cur.threads) {
            if (best && t.start > best->begin) break;
            if (nfa.at(t.state).op != Op::Match) continue;
            if (!best || t.start < best->begin || (t.start == best->begin && pos > best->end)) best = Match{t.start, pos};
        }
        if (pos >= text.size()) break;
        next.threads.clear();
        sim.step(cur, next, pos, best ? best->begin : kNone);
        std::swap(cur, next);
        if (cur.threads.empty() && best) break;
    }
    return best;
}

}  // namespace rx
