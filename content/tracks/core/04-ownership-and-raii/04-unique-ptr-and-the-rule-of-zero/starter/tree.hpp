#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

// A set of ints kept in a binary search tree. Every node is OWNED by exactly one std::unique_ptr (its parent's, or the
// tree's). So: no `new`, no `delete`, and NO destructor, copy constructor or move constructor to write. The compiler's own
// are exactly right (the "rule of zero"), because unique_ptr already frees what it owns and is move-only.
//
// Do not add a destructor or any other special member: declaring one suppresses the compiler's move operations.
// Do not keep a separate `size_` member either: ask the tree (see size()).
class Tree {
public:
    // TODO: insert `value` and return true; return false (changing nothing) if it is already in the tree.
    // Hint: keep a `std::unique_ptr<Node>*` that points at the pointer to fill in (start with &root_), walk it down
    // to the left or right child, and finish with `*link = std::make_unique<Node>(value);`.
    bool insert(int value) { (void)value; return false; }

    // TODO: is `value` in the tree?
    bool contains(int value) const { (void)value; return false; }

    // TODO: remove `value`; return false if it is not there. No child: the link becomes null. One child: the link takes
    // that child (`*link = std::move(n.right);`). Two children: copy the SMALLEST value of the right subtree into the node
    // and cut that smallest node out the same way.
    bool erase(int value) { (void)value; return false; }

    // TODO: how many values? Walk the tree (recursively); do not store a count in a member.
    std::size_t size() const { return 0; }

    // TODO: the number of levels: 0 for an empty tree, 1 for a single node, ...
    int height() const { return 0; }

    // TODO: all the values, ascending (an in-order walk: left subtree, the node, right subtree).
    std::vector<int> to_vector() const { return {}; }

    // TODO: an independent deep copy (every node copied). Tree itself can only be moved, so copying is explicit.
    Tree clone() const { return Tree(); }

private:
    struct Node {
        int value;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
        explicit Node(int v) : value(v) {}
    };

    std::unique_ptr<Node> root_;
};
