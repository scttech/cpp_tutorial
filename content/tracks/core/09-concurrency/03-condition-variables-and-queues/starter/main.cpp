// A demo of the BoundedQueue (read-only). Press Run. ThreadSanitizer is on.
#include <iostream>
#include <thread>
#include <vector>

#include "queue.hpp"

int main() {
    BoundedQueue<int> queue(3);  // room for only three items: a fast producer has to wait for the consumer
    std::jthread producer([&queue] {
        for (int i = 1; i <= 10; ++i) queue.push(i);
        queue.close();  // "no more items"
    });
    long long sum = 0;
    while (auto item = queue.pop()) sum += *item;  // pop() waits for an item, and returns nothing only when the queue is closed and empty
    std::cout << "sum of 1..10 through the queue: " << sum << "\n";
}
