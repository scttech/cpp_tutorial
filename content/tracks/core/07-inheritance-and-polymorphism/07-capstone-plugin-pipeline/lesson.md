# Capstone: Plugin Pipeline

Build a **text-processing pipeline** whose steps are interchangeable **plugins**: a `Pipeline` holds an ordered list of `Stage` objects behind `std::unique_ptr<Stage>`; text flows through them line by line. It uses everything in this module: an **interface** with virtual functions, **ownership** by smart pointers, the **clone** idiom to copy through a base pointer, **composition** (a pipeline *has* stages), a **registry** so that new stages can be added by code written later, and run-time **error handling** that does not let one bad line spoil the rest.

```cpp
Registry registry = Registry::with_builtins();
registry.add("length", [](std::string_view) { return std::make_unique<AddLength>(); });   // a plugin: written elsewhere, found by name

Pipeline p = build_pipeline(registry, "trim | lower | drop-blank | dedupe | reject:error | replace:world=>there | length | number");
Result r = p.run(text);
r.lines;    // the lines that made it through all the stages
r.errors;   // {input line, stage, message} for each line a stage rejected
```

`main.cpp` (read-only) runs exactly this: press **Run** to see it.

## The pieces

**`Stage`** (given): an interface with `name()`, `process(std::string line)` and `clone()`. `process` returns the line to pass on (changed or not), or `std::nullopt` to **drop** it, and may **throw** to reject a line. A stage may keep **state** between the lines of one run (a counter, the previous line).

**The nine stages** (you write them; each is a `final` class that overrides all three functions):

| class | name | what it does |
|---|---|---|
| `Trim` | `trim` | removes whitespace (space, tab, `\r`, `\f`, `\v`) at both ends |
| `Lowercase` / `Uppercase` | `lower` / `upper` | ASCII letters only |
| `DropBlank` | `drop-blank` | drops empty and whitespace-only lines; the others pass **unchanged** |
| `Replace(from, to)` | `replace` | every non-overlapping occurrence of `from`, left to right; the replacement is **not** scanned again; empty `from` → `std::invalid_argument` |
| `NumberLines(start = 1)` | `number` | **stateful**: `"N: "` in front of each line that reaches it |
| `Dedupe` | `dedupe` | **stateful**: drops a line equal to the previous line that reached it |
| `Limit(n)` | `limit` | **stateful**: passes the first `n` lines that reach it |
| `Reject(needle)` | `reject` | throws `std::runtime_error` for a line containing `needle`; empty `needle` → `std::invalid_argument` |

**`Pipeline`**: `add` (ownership; a null pointer is `std::invalid_argument`; returns `*this`), `emplace<S>(args...)`, `size()`, `names()`, and `run(text)`:

- The text is split at `'\n'`; a **final** `'\n'` does not start another line (`"a\n"` is one line, `""` is none, `"\n"` is one empty line).
- Every line goes through the stages in order. A stage that returns nothing **drops** the line: later stages never see it. A stage that **throws** a `std::exception` drops the line too and adds a `StageError{input line number, stage name, what()}`; the other lines carry on. Line numbers count the **input** lines, dropped ones included.
- A `Pipeline` is a **recipe**: `run` is `const` and works on **fresh clones** of the stages, so two runs are independent (`number` starts at 1 each time) and the stages in the pipeline never change.
- A `Pipeline` is **copyable** (a deep copy: clone every stage) and movable.

**`Registry`**: stage names to **factories** (`std::function<std::unique_ptr<Stage>(std::string_view arg)>`). `make("name:arg")` splits at the **first** colon; `with_builtins()` registers the nine stages, with the argument rules in the starter file (a number must be *all* digits). Anything wrong is `std::invalid_argument`.

**`build_pipeline(registry, "trim | upper")`**: specs separated by `|`, blanks around each ignored; a blank recipe is an empty pipeline; an empty spec between bars is an error.

## The tiers

- **Works**: each stage, the pipeline, the registry, the recipe parser.
- **Robust**: state restarts on every run; `clone()` copies the *real* class with its *current state*; deep copies, moves and no leaks; error line numbers; every malformed spec; bytes above 127; a long line and a 20000-line text.
- **Clean**: no raw `new`/`delete` (`make_unique` everywhere), `override` on all 27 overriding functions and no `virtual` outside `Stage`, no `dynamic_cast` or `typeid`.

Start with the stages (they are independent), then the pipeline, then the registry. The tests are grouped the same way.
