#define ZTH_TRACK_ALLOCATIONS
#include <initializer_list>
#include <vector>

#include "list.hpp"
#include "zth_test.hpp"

Node* push_front(Node*, int);
Node* push_back(Node*, int);
std::size_t list_length(const Node*);
Node* list_find(Node*, int);
Node* remove_first(Node*, int);
Node* list_reverse(Node*);
void free_list(Node*);
int* find_max(int*, int*);
void increment_all(int*, int*);

using V = std::vector<int>;

namespace {
// The tests build and tear down lists with their OWN helpers, so that one bug in the learner's code does
// not make unrelated tests fail.
Node* build(std::initializer_list<int> values) {
    Node* head = nullptr;
    Node** tail = &head;
    for (int v : values) {
        *tail = new Node{v, nullptr};
        tail = &(*tail)->next;
    }
    return head;
}
V to_vec(const Node* head) {
    V out;
    for (const Node* p = head; p != nullptr && out.size() < 3000000; p = p->next) out.push_back(p->value);
    return out;
}
void destroy(Node* head) {
    while (head != nullptr) {
        Node* next = head->next;
        delete head;
        head = next;
    }
}
// An array with a sentinel right after it: a function that reads or writes one past the end is caught.
struct Guarded {
    int a[5];
    volatile int sentinel = 1000;
};
}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "push_front puts the new value first") {
    Node* h = push_front(push_front(nullptr, 1), 2);
    ZTH_EXPECT_EQ(to_vec(h), (V{2, 1}));
    destroy(h);
}
ZTH_TEST("works", "push_back appends at the end (and starts a list when it is empty)") {
    Node* h = nullptr;
    for (int v : {1, 2, 3}) h = push_back(h, v);
    ZTH_EXPECT_EQ(to_vec(h), (V{1, 2, 3}));
    destroy(h);
}
ZTH_TEST("works", "list_length counts the nodes") {
    Node* h = build({4, 5, 6});
    ZTH_EXPECT_EQ(list_length(h), std::size_t{3});
    destroy(h);
}
ZTH_TEST("works", "list_find returns the node itself") {
    Node* h = build({4, 5, 6});
    ZTH_EXPECT_TRUE(list_find(h, 5) == h->next);
    ZTH_EXPECT_TRUE(list_find(h, 4) == h);
    destroy(h);
}
ZTH_TEST("works", "list_find returns nullptr when the value is absent") {
    Node* h = build({4, 5, 6});
    ZTH_EXPECT_TRUE(list_find(h, 7) == nullptr);
    destroy(h);
}
ZTH_TEST("works", "remove_first removes the head") {
    Node* h = remove_first(build({1, 2, 3}), 1);
    ZTH_EXPECT_EQ(to_vec(h), (V{2, 3}));
    destroy(h);
}
ZTH_TEST("works", "remove_first removes a middle node") {
    Node* h = remove_first(build({1, 2, 3}), 2);
    ZTH_EXPECT_EQ(to_vec(h), (V{1, 3}));
    destroy(h);
}
ZTH_TEST("works", "remove_first removes the last node") {
    Node* h = remove_first(build({1, 2, 3}), 3);
    ZTH_EXPECT_EQ(to_vec(h), (V{1, 2}));
    destroy(h);
}
ZTH_TEST("works", "remove_first leaves the list alone when the value is absent") {
    Node* h = build({1, 2, 3});
    Node* r = remove_first(h, 9);
    ZTH_EXPECT_TRUE(r == h);
    ZTH_EXPECT_EQ(to_vec(r), (V{1, 2, 3}));
    destroy(r);
}
ZTH_TEST("works", "remove_first removes only the first of several equal values") {
    Node* h = remove_first(build({1, 2, 1, 2}), 2);
    ZTH_EXPECT_EQ(to_vec(h), (V{1, 1, 2}));
    destroy(h);
}
ZTH_TEST("works", "list_reverse reverses by relinking the same nodes") {
    Node* h = build({1, 2, 3});
    Node* last = h->next->next;
    Node* r = list_reverse(h);
    ZTH_EXPECT_TRUE(r == last);
    ZTH_EXPECT_EQ(to_vec(r), (V{3, 2, 1}));
    destroy(r);
}
ZTH_TEST("works", "find_max returns a pointer to the first largest element") {
    int a[] = {3, 9, 2, 9, 1};
    ZTH_EXPECT_TRUE(find_max(a, a + 5) == &a[1]);
}
ZTH_TEST("works", "increment_all adds one to every element of the range") {
    int a[] = {1, 2, 3};
    increment_all(a, a + 3);
    ZTH_EXPECT_EQ(a[0], 2);
    ZTH_EXPECT_EQ(a[1], 3);
    ZTH_EXPECT_EQ(a[2], 4);
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "every list function accepts an empty list (nullptr)") {
    ZTH_EXPECT_EQ(list_length(nullptr), std::size_t{0});
    ZTH_EXPECT_TRUE(list_find(nullptr, 1) == nullptr);
    ZTH_EXPECT_TRUE(remove_first(nullptr, 1) == nullptr);
    ZTH_EXPECT_TRUE(list_reverse(nullptr) == nullptr);
    free_list(nullptr);
}
ZTH_TEST("robust", "remove_first of the only node gives an empty list") {
    ZTH_EXPECT_TRUE(remove_first(build({7}), 7) == nullptr);
}
ZTH_TEST("robust", "remove_first with all-equal values removes exactly one") {
    Node* h = remove_first(build({5, 5, 5}), 5);
    ZTH_EXPECT_EQ(to_vec(h), (V{5, 5}));
    destroy(h);
}
ZTH_TEST("robust", "list_reverse of one and of two nodes") {
    Node* one = build({1});
    ZTH_EXPECT_TRUE(list_reverse(one) == one);
    ZTH_EXPECT_TRUE(one->next == nullptr);
    destroy(one);
    Node* two = list_reverse(build({1, 2}));
    ZTH_EXPECT_EQ(to_vec(two), (V{2, 1}));
    destroy(two);
}
ZTH_TEST("robust", "push_back builds a long list in order") {
    Node* h = nullptr;
    for (int i = 0; i < 3000; ++i) h = push_back(h, i);
    V v = to_vec(h);
    ZTH_EXPECT_EQ(v.size(), std::size_t{3000});
    ZTH_EXPECT_EQ(v.front(), 0);
    ZTH_EXPECT_EQ(v.back(), 2999);
    destroy(h);
}
ZTH_TEST("robust", "half a million nodes: length, reverse and free_list use loops, not recursion") {
    Node* h = nullptr;
    for (int i = 0; i < 500000; ++i) h = new Node{i, h};
    ZTH_EXPECT_EQ(list_length(h), std::size_t{500000});
    h = list_reverse(h);
    ZTH_EXPECT_EQ(h->value, 0);  // built head-first, so the original head held 499999 and the old tail held 0
    free_list(h);  // a recursive version overflows the stack here
}
ZTH_TEST("robust", "find_max: empty range returns `last`") {
    Guarded g{{1, 2, 3, 4, 5}};
    int* end = g.a + 5;
    ZTH_EXPECT_TRUE(find_max(end, end) == end);
    ZTH_EXPECT_TRUE(find_max(g.a, g.a) == g.a);
}
ZTH_TEST("robust", "find_max never looks at `last` (one past the end) (off-by-one)") {
    Guarded g{{1, 2, 3, 4, 5}};
    ZTH_EXPECT_TRUE(find_max(g.a, g.a + 5) == &g.a[4]);  // not the sentinel (1000) that follows
}
ZTH_TEST("robust", "find_max: a single element, negative values, ties") {
    int one[] = {-4};
    ZTH_EXPECT_TRUE(find_max(one, one + 1) == &one[0]);
    int neg[] = {-9, -3, -7, -3};
    ZTH_EXPECT_TRUE(find_max(neg, neg + 4) == &neg[1]);
}
ZTH_TEST("robust", "increment_all on an empty range changes nothing, and never touches `last`") {
    Guarded g{{1, 2, 3, 4, 5}};
    increment_all(g.a, g.a);
    ZTH_EXPECT_EQ(g.a[0], 1);
    increment_all(g.a, g.a + 5);
    ZTH_EXPECT_EQ(g.a[4], 6);
    ZTH_EXPECT_EQ(static_cast<int>(g.sentinel), 1000);
}

// ---- Fast: nothing should allocate that does not have to ----------------------------------------
ZTH_TEST("fast", "push_front allocates exactly one node") {
    Node* h = build({1, 2});
    zth::mem::Scope m;
    Node* r = push_front(h, 0);
    ZTH_EXPECT_ALLOCS_AT_MOST(m, 1);
    ZTH_EXPECT_EQ(to_vec(r), (V{0, 1, 2}));
    destroy(r);
}
ZTH_TEST("fast", "list_reverse relinks: no allocation at all") {
    Node* h = build({1, 2, 3, 4, 5});
    zth::mem::Scope m;
    Node* r = list_reverse(h);
    ZTH_EXPECT_ALLOCS_AT_MOST(m, 0);
    destroy(r);
}
ZTH_TEST("fast", "remove_first, list_find and list_length allocate nothing") {
    Node* h = build({1, 2, 3, 4, 5});
    zth::mem::Scope m;
    (void)list_length(h);
    (void)list_find(h, 4);
    h = remove_first(h, 3);
    ZTH_EXPECT_ALLOCS_AT_MOST(m, 0);
    destroy(h);
}

// ---- Clean: every node is freed exactly once -----------------------------------------------------
ZTH_TEST("clean", "remove_first deletes the node it removes (head, middle, tail)") {
    zth::mem::Scope m;
    for (int victim : {1, 2, 3}) {
        Node* h = remove_first(build({1, 2, 3}), victim);
        destroy(h);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}
ZTH_TEST("clean", "free_list deletes every node exactly once") {
    zth::mem::Scope m;
    Node* h = nullptr;
    for (int i = 0; i < 100; ++i) h = push_front(h, i);
    free_list(h);
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}
ZTH_TEST("clean", "list_reverse neither leaks nor duplicates nodes") {
    zth::mem::Scope m;
    Node* h = list_reverse(build({1, 2, 3, 4}));
    free_list(h);
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}
ZTH_TEST("clean", "push_front and push_back allocate only the nodes they return") {
    zth::mem::Scope m;
    Node* h = nullptr;
    for (int i = 0; i < 10; ++i) h = push_front(h, i);
    for (int i = 0; i < 10; ++i) h = push_back(h, i);
    free_list(h);
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}
ZTH_TEST("clean", "a full lifecycle (build, search, remove, reverse, free) leaves nothing behind") {
    zth::mem::Scope m;
    Node* h = nullptr;
    for (int i = 0; i < 20; ++i) h = push_back(h, i % 5);
    (void)list_find(h, 3);
    for (int v : {0, 4, 4, 9, 2}) h = remove_first(h, v);
    h = list_reverse(h);
    free_list(h);
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}
