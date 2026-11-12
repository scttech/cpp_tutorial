# A Command-Line Argument Parser

> **You're on the developer-tools team.** Every internal tool eventually grows a command line, and every hand-rolled `argv` loop eventually misparses something: a filename that starts with a dash, `-5` read as an option, `-vvv` silently ignored, `--output` as the last argument reading past the end. Users' muscle memory comes from `ls`, `grep` and `git`: `-la`, `-ofile`, `--long=value`, `--` to end options. You will write the parser that follows those conventions *exactly*, with errors that tell the user which argument was wrong, and a help screen generated from the same declaration.

## Declare, then parse

You describe the interface once: options (long name, optional short letter, flag or value, repeatable, required, default) and positionals (required, optional, variadic). Two things follow from the one declaration: the **parser** and the **help text**, so they cannot disagree. `check()` validates the declaration itself (a duplicated letter, a variadic argument that is not last) so mistakes in *your* program are found at the first run, not by a user.

## The conventions, argument by argument

| Argument | Meaning |
|---|---|
| `--` | end of options; everything after is positional (`rm -- -rf` removes a file named `-rf`) |
| `-` | a positional (by convention: standard input) |
| `-5`, `-3.14` | negative numbers are positionals, unless a digit option exists |
| `--name=value` / `--name value` | a long option; `--name value` takes the next argument **whatever it looks like** (`--output -x` has the value `-x`) |
| `--na` | a unique prefix of a long name works; an ambiguous one is an error; an exact match always wins |
| `-abc` | a cluster: `-a`, `-b`, `-c` in turn; the first option that takes a value swallows the **rest** of the cluster (`-ofile`, `-vo file`, `-o=x` has the value `=x`) |
| `-vvv` | a repeatable flag counted three times |

The loop that handles all of this is short, but each line has a corner: after a value option has consumed the rest of the cluster, stop scanning the cluster; after `--` set a flag and *keep* the later `--` as data; check a prefix against *all* long names before deciding it is unique.

## Errors you can act on

An error is a **kind** plus the **offending text** exactly as typed: `unknown option '--colour'`, `option '-o' needs a value`, `missing required option '--pattern'`. Report the first problem and do not continue. The kinds are enumerated so that a program can react (`ErrorKind::MissingValue` might print the help, `UnknownOption` might suggest the nearest name) instead of parsing a message.

## Numbers

Converting `"42"` to an int is where tools get sloppy. `atoi("abc")` is 0 and `atoi("99999999999999")` is undefined; `stoi` throws and accepts `"5x"`; `strtol` needs three checks. `std::from_chars` is exact: no locale, no exceptions, no leading whitespace or `+`, a result code for overflow, and a pointer to where it stopped, which you compare with the end to reject trailing junk. Add a range check and the caller gets `optional<long long>`.

## The help screen

Generated, deterministic, aligned in columns. The test compares your output with a golden string, so the layout rules in the header comment are the specification.

## The tiers

- **Works**: every convention above, every error kind, defaults and repetition, definition checks, the exact help and usage layout, integer and floating-point conversion.
- **Robust**: 4000 random command lines against invariants, long forms versus clusters, "first problem wins", unusual definitions, number round trips, 20000 arguments and megabyte arguments.
- **Fast**: linear time in the argument count; a bounded number of allocations; lookups allocate nothing.
- **Clean**: no raw memory or exceptions; `from_chars` only; no `stoi`/`atoi`/`sscanf`/streams/regex/getopt.
