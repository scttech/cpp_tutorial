#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// A node in a tree of named things (folders, departments, UI widgets). Parent and children point at each other.
// As written, BOTH directions are shared_ptr: the parent keeps the child alive and the child keeps the parent alive, so
// once you link two nodes they can never be freed: an ownership CYCLE. Press Run and read the leak report.
//
// The fix is a decision about who owns whom: a parent OWNS its children (shared_ptr), a child only OBSERVES its parent
// (weak_ptr: it does not keep the parent alive, and `lock()` tells you whether the parent still exists).
class Node {
public:
    explicit Node(std::string name) : name_(std::move(name)) {}

    static std::shared_ptr<Node> make(std::string name) { return std::make_shared<Node>(std::move(name)); }

    const std::string& name() const { return name_; }
    const std::vector<std::shared_ptr<Node>>& children() const { return children_; }

    // TODO: the parent, or null for a root. (With a weak_ptr member: `return parent_.lock();`.)
    std::shared_ptr<Node> parent() const { return parent_; }

    // TODO: make `child` the LAST child of `parent`.
    //   - a null node: std::invalid_argument
    //   - a link that would put a node under itself (child is `parent` or one of its ancestors): std::invalid_argument
    //   - a node that already has a parent is MOVED: it leaves its old parent's children first (use detach)
    // The parent owns the child (children_), the child only observes the parent (the weak pointer).
    // Careful: the arguments can be references INTO a children list, e.g. add_child(a, b->children()[0]); detaching erases
    // from such a list, and a reference to an erased element dangles. Copy the two shared_ptrs into locals first.
    static void add_child(const std::shared_ptr<Node>& parent, const std::shared_ptr<Node>& child) {
        child->parent_ = parent;
        parent->children_.push_back(child);
    }

    // TODO: remove `node` from its parent's children and make it a root. A root, or a null pointer, is left alone.
    // (It takes the shared_ptr, and not `this`, so the node stays alive while it is being removed.)
    static void detach(const std::shared_ptr<Node>& node) { (void)node; }

    // TODO: how many ancestors: 0 for a root.
    std::size_t depth() const { return 0; }

    // TODO: the names from the root down to this node, joined with '/': "root/docs/notes".
    std::string path() const { return name_; }

    // TODO: this node and everything below it.
    std::size_t subtree_size() const { return 1; }

    // TODO: the first node named `name`: `root` itself, then depth-first through the children. Null if there is none.
    static std::shared_ptr<Node> find(const std::shared_ptr<Node>& root, const std::string& name) {
        (void)root;
        (void)name;
        return nullptr;
    }

private:
    std::string name_;
    std::shared_ptr<Node> parent_;  // TODO: this must not own its parent: std::weak_ptr<Node>
    std::vector<std::shared_ptr<Node>> children_;
};
