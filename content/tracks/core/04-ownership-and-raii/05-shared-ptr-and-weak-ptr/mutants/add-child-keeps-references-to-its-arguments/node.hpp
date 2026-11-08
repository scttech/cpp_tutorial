// MUTANT: the arguments are used by reference: a reference into the old parent's list dangles after the erase.
#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// A node in a tree of named things (folders, departments, UI widgets). Parent and children point at each other, and that
// is the classic way to build a LEAK with shared_ptr: if both directions own, the two keep each other alive forever.
// So the ownership goes ONE way: a parent OWNS its children (shared_ptr), a child only OBSERVES its parent (weak_ptr).
class Node {
public:
    explicit Node(std::string name) : name_(std::move(name)) {}

    static std::shared_ptr<Node> make(std::string name) { return std::make_shared<Node>(std::move(name)); }

    const std::string& name() const { return name_; }
    const std::vector<std::shared_ptr<Node>>& children() const { return children_; }

    // The parent, or null for a root. lock() turns the weak_ptr into a shared_ptr if the parent is still alive, and
    // gives null if it is gone: a weak_ptr can never dangle.
    std::shared_ptr<Node> parent() const { return parent_.lock(); }

    // Makes `child` the LAST child of `parent`. A node that already has a parent is moved (it leaves its old parent).
    // std::invalid_argument for a null node, and for a link that would create a cycle (a node under itself).
    static void add_child(const std::shared_ptr<Node>& parent_ref, const std::shared_ptr<Node>& child_ref) {
        // Take OWN copies first. The arguments may be references INTO a children list (add_child(a, b->children()[0])),
        // and detaching erases from such a list: a reference to the erased element would then dangle.
        const std::shared_ptr<Node>& parent = parent_ref;
        const std::shared_ptr<Node>& child = child_ref;
        if (!parent || !child) throw std::invalid_argument("Node::add_child: null node");
        for (std::shared_ptr<Node> p = parent; p; p = p->parent())
            if (p == child) throw std::invalid_argument("Node::add_child: a node cannot be placed under itself");
        detach(child);
        child->parent_ = parent;  // weak: the child does not own its parent
        parent->children_.push_back(child);  // strong: the parent owns its children
    }

    // Removes `node` from its parent's children; it becomes a root. (Static, and it takes a shared_ptr, so the node
    // is kept alive while it is being removed: the parent's list may have been its last owner.)
    static void detach(const std::shared_ptr<Node>& node_ref) {
        const std::shared_ptr<Node> node = node_ref;  // own copy: node_ref may be the very element we are about to erase
        if (!node) return;
        std::shared_ptr<Node> p = node->parent_.lock();
        if (!p) return;
        auto& siblings = p->children_;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), node), siblings.end());
        node->parent_.reset();
    }

    std::size_t depth() const {  // a root has depth 0
        std::size_t d = 0;
        for (std::shared_ptr<Node> p = parent(); p; p = p->parent()) ++d;
        return d;
    }

    std::string path() const {  // "root/docs/notes"
        std::vector<const Node*> chain;
        std::vector<std::shared_ptr<Node>> keep_alive;  // the ancestors stay alive while we walk up
        chain.push_back(this);
        for (std::shared_ptr<Node> p = parent(); p; p = p->parent()) {
            keep_alive.push_back(p);
            chain.push_back(p.get());
        }
        std::string out;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            if (!out.empty()) out += '/';
            out += (*it)->name_;
        }
        return out;
    }

    std::size_t subtree_size() const {  // this node and everything below it
        std::size_t n = 1;
        for (const auto& c : children_) n += c->subtree_size();
        return n;
    }

    // The first node named `name` in `root`'s tree (root first, then depth-first), or null.
    static std::shared_ptr<Node> find(const std::shared_ptr<Node>& root, const std::string& name) {
        if (!root) return nullptr;
        if (root->name_ == name) return root;
        for (const auto& c : root->children_)
            if (auto hit = find(c, name)) return hit;
        return nullptr;
    }

private:
    std::string name_;
    std::weak_ptr<Node> parent_;
    std::vector<std::shared_ptr<Node>> children_;
};
