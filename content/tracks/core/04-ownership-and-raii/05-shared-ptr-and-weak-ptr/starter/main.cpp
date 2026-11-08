// A demo of your Node tree (read-only). Press Run to see what it prints.
#include <iostream>
#include <memory>

#include "node.hpp"

int main() {
    auto root = Node::make("root");
    auto docs = Node::make("docs");
    auto notes = Node::make("notes");
    Node::add_child(root, docs);
    Node::add_child(docs, notes);
    Node::add_child(root, Node::make("music"));

    std::cout << notes->path() << " has depth " << notes->depth() << "\n";
    std::cout << "root's tree has " << root->subtree_size() << " nodes\n";
    std::cout << "root is owned by " << root.use_count() << " pointer(s), docs by " << docs.use_count() << "\n";

    std::cout << "found: " << Node::find(root, "music")->path() << "\n";

    root.reset();  // the last pointer to the root goes away: the whole tree should be freed (except what we still hold)
    std::cout << "docs still exists; its parent is " << (docs->parent() ? "alive" : "gone") << "\n";
}
