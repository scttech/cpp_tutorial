// A demo of your pipeline (read-only). Press Run to see what it prints.
#include <iostream>
#include <string>

#include "pipeline.hpp"

// A stage written AFTER the pipeline, by someone else: it needs only the Stage interface. It adds the length of every line.
class AddLength final : public Stage {
public:
    std::string name() const override { return "length"; }
    std::optional<std::string> process(std::string line) override { return line + " (" + std::to_string(line.size()) + ")"; }
    std::unique_ptr<Stage> clone() const override { return std::make_unique<AddLength>(*this); }
};

int main() {
    const std::string text =
        "  Hello, World  \n"
        "\n"
        "hello, world\n"
        "  Hello, World  \n"
        "this line has an ERROR in it\n"
        "Goodbye\n";

    Registry registry = Registry::with_builtins();
    registry.add("length", [](std::string_view) { return std::make_unique<AddLength>(); });  // a plugin

    Pipeline pipeline = build_pipeline(registry, "trim | lower | drop-blank | dedupe | reject:error | replace:world=>there | length | number");
    std::cout << "stages:";
    for (const std::string& name : pipeline.names()) std::cout << " " << name;
    std::cout << "\n";

    const Result result = pipeline.run(text);
    std::cout << result.text() << "\n";
    for (const StageError& e : result.errors) std::cout << "line " << e.line << " rejected by " << e.stage << ": " << e.message << "\n";

    // A copy is independent, and every run starts with fresh stages: numbering starts at 1 again.
    Pipeline copy = pipeline;
    std::cout << "second run, first line: " << copy.run(text).lines.at(0) << "\n";
}
