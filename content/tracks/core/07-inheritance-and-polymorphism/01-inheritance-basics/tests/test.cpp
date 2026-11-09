#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "animals.hpp"
#include "zth_test.hpp"

namespace {

using Log = std::vector<std::string>;

// Is `member` reachable from OUTSIDE the class? (False, not a compile error, when it is protected or private.)
template <class T>
constexpr bool energy_is_reachable = requires(T& t) { t.energy_; };
template <class T>
constexpr bool begin_is_reachable = requires(T& t) { t.begin(); };
template <class T>
constexpr bool index_is_reachable = requires(T& t) { t[0]; };
template <class T>
constexpr bool push_back_is_reachable = requires(T& t) { t.push_back(1); };

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "Animal: name, legs, energy 10, eat adds food") {
    Animal a("Fish", 0);
    ZTH_EXPECT_EQ(a.name(), std::string("Fish"));
    ZTH_EXPECT_EQ(a.legs(), 0);
    ZTH_EXPECT_EQ(a.energy(), 10);
    a.eat(5);
    ZTH_EXPECT_EQ(a.energy(), 15);
}
ZTH_TEST("works", "Dog: name, 4 legs, speak") {
    Dog d("Rex");
    ZTH_EXPECT_EQ(d.name(), std::string("Rex"));
    ZTH_EXPECT_EQ(d.legs(), 4);
    ZTH_EXPECT_EQ(d.speak(), std::string("Woof"));
    ZTH_EXPECT_EQ(d.energy(), 10);
}
ZTH_TEST("works", "Cat: name, 4 legs, speak") {
    Cat c("Tom");
    ZTH_EXPECT_EQ(c.name(), std::string("Tom"));
    ZTH_EXPECT_EQ(c.legs(), 4);
    ZTH_EXPECT_EQ(c.speak(), std::string("Meow"));
}
ZTH_TEST("works", "a Dog and a Cat are Animals (public inheritance); a Cat is not a Dog") {
    ZTH_EXPECT_TRUE((std::is_convertible_v<Dog*, Animal*>));
    ZTH_EXPECT_TRUE((std::is_convertible_v<Cat*, Animal*>));
    ZTH_EXPECT_FALSE((std::is_convertible_v<Cat*, Dog*>));
    ZTH_EXPECT_FALSE((std::is_convertible_v<Animal*, Dog*>));  // the other way needs a cast
}
ZTH_TEST("works", "Dog::fetch costs 3 energy") {
    Dog d("Rex");
    ZTH_EXPECT_TRUE(d.fetch());
    ZTH_EXPECT_EQ(d.energy(), 7);
    ZTH_EXPECT_TRUE(d.fetch());
    ZTH_EXPECT_EQ(d.energy(), 4);
}
ZTH_TEST("works", "Cat::climb costs 2 energy") {
    Cat c("Tom");
    ZTH_EXPECT_TRUE(c.climb());
    ZTH_EXPECT_EQ(c.energy(), 8);
}
ZTH_TEST("works", "Dog::learn adds to tricks") {
    Dog d("Rex");
    ZTH_EXPECT_TRUE(d.tricks().empty());
    d.learn("sit");
    d.learn("roll");
    ZTH_EXPECT_TRUE((d.tricks() == Log{"sit", "roll"}));
}
ZTH_TEST("works", "describe: the base version, and the derived versions that add the kind") {
    ZTH_EXPECT_EQ(Animal("Fish", 0).describe(), std::string("Fish has 0 legs"));
    ZTH_EXPECT_EQ(Dog("Rex").describe(), std::string("Rex has 4 legs (dog)"));
    ZTH_EXPECT_EQ(Cat("Tom").describe(), std::string("Tom has 4 legs (cat)"));
}
ZTH_TEST("works", "feed changes the real animal, not a copy") {
    Dog d("Rex");
    feed(d, 5);
    ZTH_EXPECT_EQ(d.energy(), 15);
}
ZTH_TEST("works", "feed_all feeds every animal in the list") {
    Dog d("Rex");
    Cat c("Tom");
    Animal a("Fish", 0);
    std::vector<Animal*> zoo{&d, &c, &a};
    feed_all(zoo, 4);
    ZTH_EXPECT_EQ(d.energy(), 14);
    ZTH_EXPECT_EQ(c.energy(), 14);
    ZTH_EXPECT_EQ(a.energy(), 14);
}
ZTH_TEST("works", "IntStack: push, top, pop, size, empty") {
    IntStack s;
    ZTH_EXPECT_TRUE(s.empty());
    s.push(1);
    s.push(2);
    s.push(3);
    ZTH_EXPECT_EQ(s.size(), std::size_t{3});
    ZTH_EXPECT_EQ(s.top(), 3);
    ZTH_EXPECT_TRUE(s.pop());
    ZTH_EXPECT_EQ(s.top(), 2);
    ZTH_EXPECT_FALSE(s.empty());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "a Dog is built base first: Animal(Rex) then Dog(Rex); destroyed in the reverse order") {
    trace_log().clear();
    {
        Dog d("Rex");
        ZTH_EXPECT_TRUE((trace_log() == Log{"Animal(Rex)", "Dog(Rex)"}));
    }
    ZTH_EXPECT_TRUE((trace_log() == Log{"Animal(Rex)", "Dog(Rex)", "~Dog(Rex)", "~Animal(Rex)"}));
}
ZTH_TEST("robust", "a Cat is built and destroyed in the same order") {
    trace_log().clear();
    {
        Cat c("Tom");
    }
    ZTH_EXPECT_TRUE((trace_log() == Log{"Animal(Tom)", "Cat(Tom)", "~Cat(Tom)", "~Animal(Tom)"}));
}
ZTH_TEST("robust", "the base class is started once, by the member-initializer list (no second Animal is built)") {
    trace_log().clear();
    {
        Dog a("A");
        Dog b("B");
    }
    ZTH_EXPECT_TRUE((trace_log() == Log{"Animal(A)", "Dog(A)", "Animal(B)", "Dog(B)", "~Dog(B)", "~Animal(B)", "~Dog(A)", "~Animal(A)"}));
}
ZTH_TEST("robust", "a plain Animal traces itself") {
    trace_log().clear();
    {
        Animal a("Fish", 0);
    }
    ZTH_EXPECT_TRUE((trace_log() == Log{"Animal(Fish)", "~Animal(Fish)"}));
}
ZTH_TEST("robust", "energy_ is protected: reachable by Dog and Cat, not from outside") {
    ZTH_EXPECT_FALSE(energy_is_reachable<Animal>);
    ZTH_EXPECT_FALSE(energy_is_reachable<Dog>);
    ZTH_EXPECT_FALSE(energy_is_reachable<Cat>);
}
ZTH_TEST("robust", "eat ignores zero and negative food") {
    Animal a("Fish", 0);
    a.eat(-5);
    ZTH_EXPECT_EQ(a.energy(), 10);
    a.eat(0);
    ZTH_EXPECT_EQ(a.energy(), 10);
    a.eat(1);
    ZTH_EXPECT_EQ(a.energy(), 11);
}
ZTH_TEST("robust", "fetch needs at least 3 energy: exactly 3 is enough, 2 is not") {
    Dog d("Rex");
    d.fetch();
    d.fetch();
    d.fetch();  // 10 -> 7 -> 4 -> 1
    ZTH_EXPECT_EQ(d.energy(), 1);
    ZTH_EXPECT_FALSE(d.fetch());
    ZTH_EXPECT_EQ(d.energy(), 1);  // nothing was spent
    d.eat(1);                      // 2: still not enough
    ZTH_EXPECT_FALSE(d.fetch());
    ZTH_EXPECT_EQ(d.energy(), 2);
    d.eat(1);                      // 3
    ZTH_EXPECT_TRUE(d.fetch());
    ZTH_EXPECT_EQ(d.energy(), 0);
}
ZTH_TEST("robust", "climb needs at least 2 energy: exactly 2 is enough, 1 is not") {
    Cat c("Tom");
    for (int i = 0; i < 4; ++i) ZTH_EXPECT_TRUE(c.climb());  // 10 -> 2
    ZTH_EXPECT_EQ(c.energy(), 2);
    ZTH_EXPECT_TRUE(c.climb());
    ZTH_EXPECT_EQ(c.energy(), 0);
    ZTH_EXPECT_FALSE(c.climb());
    c.eat(1);
    ZTH_EXPECT_FALSE(c.climb());
    ZTH_EXPECT_EQ(c.energy(), 1);
}
ZTH_TEST("robust", "describe is NOT virtual yet: through an Animal reference the Animal version runs") {
    Dog d("Rex");
    const Animal& a = d;
    ZTH_EXPECT_EQ(a.describe(), std::string("Rex has 4 legs"));
    ZTH_EXPECT_EQ(d.describe(), std::string("Rex has 4 legs (dog)"));
}
ZTH_TEST("robust", "feed through a reference works for a Dog seen as an Animal, and for several calls") {
    Dog d("Rex");
    Animal& a = d;
    feed(a, 3);
    feed(a, 4);
    ZTH_EXPECT_EQ(d.energy(), 17);
}
ZTH_TEST("robust", "feed_all with an empty list and with null pointers") {
    feed_all({}, 5);
    Dog d("Rex");
    std::vector<Animal*> zoo{nullptr, &d, nullptr};
    feed_all(zoo, 5);
    ZTH_EXPECT_EQ(d.energy(), 15);
}
ZTH_TEST("robust", "sliced_copy is an Animal that kept the name, legs and energy of the dog") {
    Dog d("Rex");
    d.fetch();
    d.learn("sit");
    const auto copy = sliced_copy(d);
    ZTH_EXPECT_TRUE((std::is_same_v<std::remove_cv_t<decltype(copy)>, Animal>));
    ZTH_EXPECT_EQ(copy.name(), std::string("Rex"));
    ZTH_EXPECT_EQ(copy.legs(), 4);
    ZTH_EXPECT_EQ(copy.energy(), 7);
    ZTH_EXPECT_EQ(copy.describe(), std::string("Rex has 4 legs"));  // no (dog): it is an Animal now
    ZTH_EXPECT_EQ(d.energy(), 7);                                    // the dog itself is untouched
}
ZTH_TEST("robust", "IntStack is LIFO and pop on an empty stack is false") {
    IntStack s;
    ZTH_EXPECT_FALSE(s.pop());
    s.push(10);
    s.push(20);
    ZTH_EXPECT_EQ(s.top(), 20);
    ZTH_EXPECT_TRUE(s.pop());
    ZTH_EXPECT_EQ(s.top(), 10);
    ZTH_EXPECT_TRUE(s.pop());
    ZTH_EXPECT_TRUE(s.empty());
    ZTH_EXPECT_EQ(s.size(), std::size_t{0});
    ZTH_EXPECT_FALSE(s.pop());
}
ZTH_TEST("robust", "IntStack::top of an empty stack throws std::out_of_range") {
    IntStack s;
    ZTH_EXPECT_THROWS(std::out_of_range, s.top());
}
ZTH_TEST("robust", "IntStack inherits from vector PRIVATELY: not convertible, and the vector interface is hidden") {
    ZTH_EXPECT_FALSE((std::is_convertible_v<IntStack*, std::vector<int>*>));
    ZTH_EXPECT_FALSE((std::is_convertible_v<IntStack&, std::vector<int>&>));
    ZTH_EXPECT_FALSE(begin_is_reachable<IntStack>);
    ZTH_EXPECT_FALSE(index_is_reachable<IntStack>);
    ZTH_EXPECT_FALSE(push_back_is_reachable<IntStack>);
    ZTH_EXPECT_TRUE(begin_is_reachable<std::vector<int>>);  // (sanity check of the test itself)
}
