// MUTANT: contains gives up after the root.
#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

// A set of ints kept in a binary search tree. Every node is OWNED by exactly one unique_ptr (its parent's, or the tree's),
// so there is no `new`, no `delete`, and no destructor, copy constructor or move constructor to write: the RULE OF ZERO.
// unique_ptr already frees what it owns, and is move-only, so Tree is move-only too (copy it explicitly with clone()).
class Tree {
public:
    // false if the value is already there
    bool insert(int value) {
        std::unique_ptr<Node>* link = &root_;  // always points at the unique_ptr that holds (or will hold) the node
        while (*link) {
            if (value == (*link)->value) return false;
            link = value < (*link)->value ? &(*link)->left : &(*link)->right;
        }
        *link = std::make_unique<Node>(value);
        return true;
    }

    bool contains(int value) const {
        const Node* n = root_.get();
        if (n && n->value != value) return false;
        return n != nullptr;
    }

    // false if the value is not there
    bool erase(int value) {
        std::unique_ptr<Node>* link = &root_;
        while (*link && (*link)->value != value) link = value < (*link)->value ? &(*link)->left : &(*link)->right;
        if (!*link) return false;

        Node& n = **link;
        if (!n.left) {
            *link = std::move(n.right);  // the parent now owns n's only subtree; n itself is destroyed by this assignment
        } else if (!n.right) {
            *link = std::move(n.left);
        } else {
            // two children: n takes the smallest value of its right subtree, and that node is cut out
            std::unique_ptr<Node>* smallest = &n.right;
            while ((*smallest)->left) smallest = &(*smallest)->left;
            n.value = (*smallest)->value;
            *smallest = std::move((*smallest)->right);
        }
        return true;
    }

    // Derived by walking the tree, NOT cached in a member: a cached count would not be reset by the compiler's
    // move operations, and then a moved-from Tree would claim to hold values it no longer has.
    std::size_t size() const { return count(root_.get()); }

    int height() const { return height_of(root_.get()); }  // an empty tree has height 0, a single node 1

    std::vector<int> to_vector() const {  // ascending
        std::vector<int> out;
        collect(root_.get(), out);
        return out;
    }

    // An independent deep copy.
    Tree clone() const {
        Tree copy;
        copy.root_ = clone_node(root_.get());
        return copy;
    }

private:
    struct Node {
        int value;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
        explicit Node(int v) : value(v) {}
    };

    static std::size_t count(const Node* n) { return n ? 1 + count(n->left.get()) + count(n->right.get()) : 0; }
    static int height_of(const Node* n) { return n ? 1 + std::max(height_of(n->left.get()), height_of(n->right.get())) : 0; }
    static void collect(const Node* n, std::vector<int>& out) {
        if (!n) return;
        collect(n->left.get(), out);
        out.push_back(n->value);
        collect(n->right.get(), out);
    }
    static std::unique_ptr<Node> clone_node(const Node* n) {
        if (!n) return nullptr;
        auto copy = std::make_unique<Node>(n->value);
        copy->left = clone_node(n->left.get());
        copy->right = clone_node(n->right.get());
        return copy;
    }

    std::unique_ptr<Node> root_;
};
