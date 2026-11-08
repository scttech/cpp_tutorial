#define ZTH_TRACK_ALLOCATIONS
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "node.hpp"
#include "zth_test.hpp"

namespace {

using NodeP = std::shared_ptr<Node>;

template <class Ex, class F>
bool throws(F&& f) {
    try {
        f();
    } catch (const Ex&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

NodeP mk(const char* name) { return Node::make(name); }

std::string names(const NodeP& n) {
    std::string out;
    for (const auto& c : n->children()) out += (out.empty() ? "" : ",") + c->name();
    return out;
}

//  root
//   |- docs
//   |   `- notes
//   `- music
struct Sample {
    NodeP root = mk("root"), docs = mk("docs"), notes = mk("notes"), music = mk("music");
    Sample() {
        Node::add_child(root, docs);
        Node::add_child(docs, notes);
        Node::add_child(root, music);
    }
};

template <class T>
constexpr bool const_queries = requires(const T& n) {
    n.name();
    n.children();
    n.parent();
    n.depth();
    n.path();
    n.subtree_size();
};

template <class T>
void const_queries_test() {
    if constexpr (!const_queries<T>) {
        ::zth::fail("name, children, parent, depth, path and subtree_size must be const member functions");
    } else {
        Sample s;
        const T& c = *s.notes;
        ZTH_EXPECT_EQ(c.name(), std::string("notes"));
        ZTH_EXPECT_EQ(c.depth(), std::size_t{2});
        ZTH_EXPECT_EQ(c.path(), std::string("root/docs/notes"));
        ZTH_EXPECT_EQ(c.subtree_size(), std::size_t{1});
        ZTH_EXPECT_EQ(c.children().size(), std::size_t{0});
        ZTH_EXPECT_TRUE(c.parent() == s.docs);
    }
}

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "a new node has a name, no parent and no children") {
    NodeP n = mk("solo");
    ZTH_EXPECT_EQ(n->name(), std::string("solo"));
    ZTH_EXPECT_TRUE(n->parent() == nullptr);
    ZTH_EXPECT_TRUE(n->children().empty());
    ZTH_EXPECT_EQ(n->depth(), std::size_t{0});
    ZTH_EXPECT_EQ(n->path(), std::string("solo"));
    ZTH_EXPECT_EQ(n->subtree_size(), std::size_t{1});
}
ZTH_TEST("works", "add_child links both ways: the parent lists the child, the child knows its parent") {
    NodeP a = mk("a"), b = mk("b");
    Node::add_child(a, b);
    ZTH_EXPECT_EQ(a->children().size(), std::size_t{1});
    ZTH_EXPECT_TRUE(a->children()[0] == b);
    ZTH_EXPECT_TRUE(b->parent() == a);
    ZTH_EXPECT_TRUE(a->parent() == nullptr);
}
ZTH_TEST("works", "children are kept in the order they were added") {
    NodeP r = mk("r");
    for (const char* n : {"x", "y", "z"}) Node::add_child(r, mk(n));
    ZTH_EXPECT_EQ(names(r), std::string("x,y,z"));
}
ZTH_TEST("works", "depth and path walk up through the parents") {
    Sample s;
    ZTH_EXPECT_EQ(s.root->depth(), std::size_t{0});
    ZTH_EXPECT_EQ(s.docs->depth(), std::size_t{1});
    ZTH_EXPECT_EQ(s.notes->depth(), std::size_t{2});
    ZTH_EXPECT_EQ(s.notes->path(), std::string("root/docs/notes"));
    ZTH_EXPECT_EQ(s.music->path(), std::string("root/music"));
    ZTH_EXPECT_EQ(s.root->path(), std::string("root"));
}
ZTH_TEST("works", "subtree_size counts the node and everything below it") {
    Sample s;
    ZTH_EXPECT_EQ(s.root->subtree_size(), std::size_t{4});
    ZTH_EXPECT_EQ(s.docs->subtree_size(), std::size_t{2});
    ZTH_EXPECT_EQ(s.music->subtree_size(), std::size_t{1});
}
ZTH_TEST("works", "find returns the first node with that name (the root itself, or a descendant), or null") {
    Sample s;
    ZTH_EXPECT_TRUE(Node::find(s.root, "root") == s.root);
    ZTH_EXPECT_TRUE(Node::find(s.root, "notes") == s.notes);
    ZTH_EXPECT_TRUE(Node::find(s.root, "music") == s.music);
    ZTH_EXPECT_TRUE(Node::find(s.docs, "music") == nullptr);
    ZTH_EXPECT_TRUE(Node::find(s.root, "nothing") == nullptr);
    ZTH_EXPECT_TRUE(Node::find(nullptr, "x") == nullptr);
}
ZTH_TEST("works", "detach takes a node out of its parent and makes it a root") {
    Sample s;
    Node::detach(s.docs);
    ZTH_EXPECT_EQ(names(s.root), std::string("music"));
    ZTH_EXPECT_TRUE(s.docs->parent() == nullptr);
    ZTH_EXPECT_EQ(s.docs->depth(), std::size_t{0});
    ZTH_EXPECT_EQ(s.notes->path(), std::string("docs/notes"));  // the subtree travels with it
    ZTH_EXPECT_EQ(s.root->subtree_size(), std::size_t{2});
}
ZTH_TEST("works", "dropping the root frees the whole tree: no leaks, nothing freed twice") {
    zth::mem::Scope scope;
    {
        Sample s;
        Node::add_child(s.music, mk("track 1"));
        Node::add_child(s.music, mk("track 2"));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "the parent owns its child (use count 2) but the child only observes its parent (use count stays 1)") {
    NodeP a = mk("a"), b = mk("b");
    Node::add_child(a, b);
    ZTH_EXPECT_EQ(b.use_count(), 2);  // the variable b, and a's list of children
    ZTH_EXPECT_EQ(a.use_count(), 1);  // b does NOT keep a alive
}
ZTH_TEST("robust", "no ownership cycle: a parent and child that are both dropped are both freed") {
    zth::mem::Scope scope;
    {
        NodeP a = mk("a"), b = mk("b");
        Node::add_child(a, b);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a child can outlive its parent: parent() is then null, never dangling") {
    zth::mem::Scope scope;
    NodeP kid;
    {
        NodeP boss = mk("boss");
        kid = mk("kid");
        Node::add_child(boss, kid);
        ZTH_EXPECT_TRUE(kid->parent() == boss);
    }  // boss is destroyed here
    ZTH_EXPECT_TRUE(kid->parent() == nullptr);
    ZTH_EXPECT_EQ(kid->depth(), std::size_t{0});
    ZTH_EXPECT_EQ(kid->path(), std::string("kid"));
    kid.reset();
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a grandchild we still hold keeps its own subtree alive but not its ancestors") {
    zth::mem::Scope scope;
    NodeP keep;
    {
        Sample s;
        keep = s.docs;
    }
    ZTH_EXPECT_TRUE(keep->parent() == nullptr);
    ZTH_EXPECT_EQ(keep->subtree_size(), std::size_t{2});
    ZTH_EXPECT_EQ(keep->children()[0]->name(), std::string("notes"));
    ZTH_EXPECT_EQ(keep->children()[0]->path(), std::string("docs/notes"));
    keep.reset();
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "null nodes are std::invalid_argument") {
    NodeP a = mk("a");
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([&] { Node::add_child(nullptr, a); }));
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([&] { Node::add_child(a, nullptr); }));
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([&] { Node::add_child(nullptr, nullptr); }));
    ZTH_EXPECT_TRUE(a->children().empty());
    Node::detach(nullptr);  // harmless
}
ZTH_TEST("robust", "a node cannot be placed under itself or under one of its own descendants (std::invalid_argument)") {
    Sample s;
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([&] { Node::add_child(s.root, s.root); }));
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([&] { Node::add_child(s.notes, s.root); }));
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([&] { Node::add_child(s.docs, s.docs); }));
    ZTH_EXPECT_TRUE(throws<std::invalid_argument>([&] { Node::add_child(s.notes, s.docs); }));
    // nothing changed
    ZTH_EXPECT_EQ(names(s.root), std::string("docs,music"));
    ZTH_EXPECT_TRUE(s.root->parent() == nullptr);
    ZTH_EXPECT_EQ(s.root->subtree_size(), std::size_t{4});
    ZTH_EXPECT_EQ(s.notes->path(), std::string("root/docs/notes"));
}
ZTH_TEST("robust", "adding a node that already has a parent MOVES it: it leaves the old parent") {
    Sample s;
    Node::add_child(s.music, s.notes);  // notes: from docs to music
    ZTH_EXPECT_EQ(names(s.docs), std::string(""));
    ZTH_EXPECT_EQ(names(s.music), std::string("notes"));
    ZTH_EXPECT_TRUE(s.notes->parent() == s.music);
    ZTH_EXPECT_EQ(s.notes->path(), std::string("root/music/notes"));
    ZTH_EXPECT_EQ(s.root->subtree_size(), std::size_t{4});
    ZTH_EXPECT_EQ(s.notes.use_count(), 2);  // still exactly one owner besides our variable
}
ZTH_TEST("robust", "re-adding a child to its own parent moves it to the end") {
    NodeP r = mk("r"), a = mk("a"), b = mk("b"), c = mk("c");
    Node::add_child(r, a);
    Node::add_child(r, b);
    Node::add_child(r, c);
    Node::add_child(r, a);
    ZTH_EXPECT_EQ(names(r), std::string("b,c,a"));
    ZTH_EXPECT_EQ(r->subtree_size(), std::size_t{4});
}
ZTH_TEST("robust", "detach is harmless on a root and when repeated, and the node can be added somewhere else afterwards") {
    Sample s;
    Node::detach(s.root);  // a root: nothing to do
    ZTH_EXPECT_EQ(s.root->subtree_size(), std::size_t{4});
    Node::detach(s.music);
    Node::detach(s.music);
    ZTH_EXPECT_EQ(names(s.root), std::string("docs"));
    Node::add_child(s.notes, s.music);
    ZTH_EXPECT_EQ(s.music->path(), std::string("root/docs/notes/music"));
    ZTH_EXPECT_EQ(s.music->depth(), std::size_t{3});
}
ZTH_TEST("robust", "detach works when the argument is a reference into the parent's own list (the element being erased)") {
    zth::mem::Scope scope;
    {
        NodeP r = mk("r");
        Node::add_child(r, mk("only owned by r"));
        Node::add_child(r, mk("second"));
        Node::detach(r->children()[0]);  // the reference IS the element that detach erases from the list
        ZTH_EXPECT_EQ(names(r), std::string("second"));
        ZTH_EXPECT_TRUE(r->children()[0]->parent() == r);  // the node that slid into that slot is untouched
        Node::detach(r->children()[0]);                    // and now the list becomes empty
        ZTH_EXPECT_TRUE(r->children().empty());
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "add_child works when the child is a reference into the old parent's list (moving it between parents)") {
    zth::mem::Scope scope;
    {
        Sample s;
        Node::add_child(s.music, s.docs->children()[0]);  // notes, straight out of docs' list
        ZTH_EXPECT_EQ(names(s.docs), std::string(""));
        ZTH_EXPECT_EQ(names(s.music), std::string("notes"));
        ZTH_EXPECT_EQ(s.music->children()[0]->path(), std::string("root/music/notes"));
        NodeP a = mk("a"), b = mk("b"), c = mk("c");
        Node::add_child(a, b);
        Node::add_child(a, c);
        Node::add_child(a->children()[1], a->children()[0]);  // b under c, both straight out of a's list
        ZTH_EXPECT_EQ(names(a), std::string("c"));
        ZTH_EXPECT_EQ(b->path(), std::string("a/c/b"));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "find shares ownership with the tree: the returned pointer keeps the node alive") {
    zth::mem::Scope scope;
    NodeP found;
    {
        Sample s;
        found = Node::find(s.root, "music");
        ZTH_EXPECT_EQ(found.use_count(), 3);  // s.music, the root's list, and `found`
    }
    ZTH_EXPECT_EQ(found->name(), std::string("music"));
    ZTH_EXPECT_EQ(found.use_count(), 1);
    found.reset();
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "find returns the first match in depth-first order") {
    NodeP r = mk("r"), a = mk("a"), b = mk("b"), dup1 = mk("dup"), dup2 = mk("dup");
    Node::add_child(r, a);
    Node::add_child(r, b);
    Node::add_child(a, dup1);
    Node::add_child(b, dup2);
    ZTH_EXPECT_TRUE(Node::find(r, "dup") == dup1);
}
ZTH_TEST("robust", "a long chain and a very wide node are built and freed without trouble") {
    zth::mem::Scope scope;
    {
        NodeP head = mk("n0");
        NodeP tail = head;
        for (int i = 1; i < 800; ++i) {
            NodeP next = mk(("n" + std::to_string(i)).c_str());
            Node::add_child(tail, next);
            tail = next;
        }
        ZTH_EXPECT_EQ(tail->depth(), std::size_t{799});
        ZTH_EXPECT_EQ(head->subtree_size(), std::size_t{800});
        NodeP wide = mk("wide");
        for (int i = 0; i < 500; ++i) Node::add_child(wide, mk("leaf"));
        ZTH_EXPECT_EQ(wide->subtree_size(), std::size_t{501});
        tail.reset();
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "everything that only looks works on a const Node") { const_queries_test<Node>(); }

// ---- Clean ---------------------------------------------------------------------------------------
ZTH_TEST("clean", "node.hpp does not use new, delete, malloc or free: make_shared does it") { ZTH_EXPECT_NO_RAW_MEMORY("node.hpp"); }
