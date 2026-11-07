#pragma once

// One link of a singly linked list. The list is just a pointer to its first node (the "head");
// an empty list is a null head. Each node is allocated on the heap with `new`.
struct Node {
    int value;
    Node* next;  // the following node, or nullptr at the end
};
