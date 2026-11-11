# Fast path: what's different in C++

- **`std::priority_queue` is a max-heap.** To get the smallest `f` first, give it a comparator that says "a is worse than b when a.f > b.f", or use `std::greater`. Python's `heapq` and Java's `PriorityQueue` are min-heaps; this trips up everyone once.
- **There is no decrease-key**, so A* pushes a new entry and skips the stale ones when they surface (*lazy deletion*). It is simpler than an indexed heap and usually as fast, because the queue stays small.
- **Flat arrays beat node containers.** `std::vector<long long> best(w*h)` is one allocation and perfectly cache-friendly; `std::map<Point, int>` is one allocation per cell and a pointer chase per lookup. This is the Module 10 lesson applied.
- **A lambda as the comparator** has a unique type, so the queue's template argument is `decltype(later)` and the lambda is passed to the constructor.
- **`std::abs(int)`** is in `<cstdlib>`; `std::abs(double)` is in `<cmath>`. Mixing them up is a classic silent bug.
- **Structured bindings** (`auto [dist, cell] = q.top();`) make the test's reference Dijkstra read like the pseudocode.
