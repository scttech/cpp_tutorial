#include <cstddef>
#include <iostream>

#include "list.hpp"

// ---- the list: every function takes and returns the HEAD, because the head can change -----------

// Add a new node holding `value` at the front. Returns the new head.
Node* push_front(Node* head, int value) {
    // TODO: allocate a node with `new`
    (void)value;
    return head;
}

// Add a new node at the END. Returns the head (a new node if the list was empty).
Node* push_back(Node* head, int value) {
    // TODO
    (void)value;
    return head;
}

// How many nodes? (An empty list has 0.)
std::size_t list_length(const Node* head) {
    // TODO
    (void)head;
    return 0;
}

// The first node holding `value`, or nullptr. (Return the node itself, not a copy.)
Node* list_find(Node* head, int value) {
    // TODO
    (void)head;
    (void)value;
    return nullptr;
}

// Remove the FIRST node holding `value`: unlink it and delete it. Returns the (possibly new) head.
// If there is no such node, nothing changes.
Node* remove_first(Node* head, int value) {
    // TODO
    (void)value;
    return head;
}

// Reverse the list by relinking the existing nodes (no new nodes!). Returns the new head.
Node* list_reverse(Node* head) {
    // TODO
    return head;
}

// Delete every node. (After this the caller must not use the old head: it dangles.)
void free_list(Node* head) {
    // TODO
    (void)head;
}

// ---- pointers into an array: the range [first, last) -------------------------------------------

// A pointer to the largest element in [first, last) (the first one on a tie), or `last` if the range is empty.
int* find_max(int* first, int* last) {
    // TODO
    (void)first;
    return last;
}

// Add 1 to every element in [first, last). `last` itself is one past the end: do not touch it.
void increment_all(int* first, int* last) {
    // TODO
    (void)first;
    (void)last;
}

int main() {
    int on_stack = 1;                // lives in this function's stack frame, vanishes when main returns
    int* on_heap = new int(2);       // lives until you delete it
    std::cout << "stack value " << on_stack << " at " << &on_stack << "\n";
    std::cout << "heap value  " << *on_heap << " at " << on_heap << "\n";
    delete on_heap;                  // every new needs exactly one delete
    on_heap = nullptr;               // and the old pointer must not be used again

    Node* list = nullptr;
    for (int v : {3, 2, 1}) list = push_front(list, v);
    for (const Node* n = list; n != nullptr; n = n->next) std::cout << n->value << " ";
    std::cout << "\n";
    free_list(list);
}
