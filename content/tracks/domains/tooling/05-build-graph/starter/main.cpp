// What `make` and `ninja` do: order the targets, find out what is out of date, and say what can run in parallel. Read-only. Press Run.
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "build.hpp"

static void list(const char* title, const std::vector<std::string>& names) {
    std::printf("%s:", title);
    for (const auto& n : names) std::printf(" %s", n.c_str());
    std::printf("\n");
}

int main() {
    // declaration order is the tie-break for everything below
    const auto g = build::Graph::make({
        {"app", {"main.o", "util.o", "libm.a"}},
        {"main.o", {"main.cpp", "util.h"}},
        {"util.o", {"util.cpp", "util.h"}},
        {"libm.a", {"math.o"}},
        {"math.o", {"math.cpp"}},
        {"main.cpp", {}}, {"util.cpp", {}}, {"util.h", {}}, {"math.cpp", {}},
    });
    if (!g.graph) return 1;
    list("build order", g.graph->order());
    for (std::size_t i = 0; i < g.graph->levels({"app"}).size(); ++i) list(("  can run in parallel (round " + std::to_string(i + 1) + ")").c_str(), g.graph->levels({"app"})[i]);
    list("changing util.h affects", g.graph->dependents("util.h"));

    std::unordered_map<std::string, std::int64_t> time = {{"main.cpp", 10}, {"util.cpp", 10}, {"util.h", 10}, {"math.cpp", 10}, {"main.o", 20}, {"util.o", 20}, {"math.o", 20}, {"libm.a", 21}, {"app", 30}};
    const auto nothing = g.graph->plan({"app"}, time);
    list("\nnothing changed, rebuild", nothing.rebuild);
    time["util.h"] = 25;   // a header was edited
    list("after editing util.h, rebuild", g.graph->plan({"app"}, time).rebuild);
    time.erase("math.o");  // an object file was deleted
    list("after deleting math.o, rebuild", g.graph->plan({"app"}, time).rebuild);

    const auto bad = build::Graph::make({{"a", {"b"}}, {"b", {"c"}}, {"c", {"a"}}, {"d", {"a"}}});
    if (bad.error) {
        std::printf("\ncycle:");
        for (const auto& n : bad.error->cycle) std::printf(" %s", n.c_str());
        std::printf("\n");
    }
}
