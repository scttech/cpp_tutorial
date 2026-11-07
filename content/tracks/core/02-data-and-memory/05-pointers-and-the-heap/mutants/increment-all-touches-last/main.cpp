// MUTANT: also increments the element one past the end.
#include <cstddef>
#include <iostream>

#include "list.hpp"

Node* push_front(Node* head, int value) {
    return new Node{value, head};  // the new node points at the old head, and becomes the head
}

Node* push_back(Node* head, int value) {
    Node* node = new Node{value, nullptr};
    if (head == nullptr) return node;  // empty list: the new node is the whole list
    Node* last = head;
    while (last->next != nullptr) last = last->next;  // walk to the final node
    last->next = node;
    return head;
}

std::size_t list_length(const Node* head) {
    std::size_t n = 0;
    for (const Node* p = head; p != nullptr; p = p->next) ++n;
    return n;
}

Node* list_find(Node* head, int value) {
    for (Node* p = head; p != nullptr; p = p->next) {
        if (p->value == value) return p;
    }
    return nullptr;
}

Node* remove_first(Node* head, int value) {
    Node* prev = nullptr;
    Node* cur = head;
    while (cur != nullptr && cur->value != value) {
        prev = cur;
        cur = cur->next;
    }
    if (cur == nullptr) return head;  // not found: nothing to do

    Node* rest = cur->next;
    if (prev != nullptr) prev->next = rest; else head = rest;  // unlink first...
    delete cur;                                                 // ...then delete
    return head;
}

Node* list_reverse(Node* head) {
    Node* prev = nullptr;
    while (head != nullptr) {
        Node* following = head->next;  // remember the rest before overwriting the link
        head->next = prev;
        prev = head;
        head = following;
    }
    return prev;
}

void free_list(Node* head) {
    while (head != nullptr) {
        Node* following = head->next;  // read the link BEFORE deleting the node that holds it
        delete head;
        head = following;
    }
}

int* find_max(int* first, int* last) {
    if (first == last) return last;
    int* best = first;
    for (int* p = first + 1; p != last; ++p) {
        if (*p > *best) best = p;
    }
    return best;
}

void increment_all(int* first, int* last) {
    for (int* p = first; p <= last; ++p) ++*p;
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
