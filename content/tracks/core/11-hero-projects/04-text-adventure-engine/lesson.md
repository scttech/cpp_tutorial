# Hero Project: Text Adventure Engine

> **A game is a data file plus an engine.** Zork, Colossal Cave and every interactive-fiction engine since share one idea: the *world* (rooms, items, doors) lives in a data file; the *engine* is a small program that loads it and executes commands. Add a room and nothing in the engine changes; add a verb and nothing in the engine's core changes either. This project is that engine, and its design goal is exactly that: **open for extension**.

## What you build

| File | You write | Role |
|---|---|---|
| `loader.hpp` | `load_world(text)` | the data file → a `World`, or the first mistake with its line number |
| `actions.hpp` | one class per verb, derived from `Action` | `look go take drop inventory open close unlock lock turn` |
| `game.hpp` | `class Game` | the state of one play-through and the dispatch: `execute(line)` finds the `Action` for the verb and calls it |

`model.hpp` (read-only) defines everything shared: `World`, `Room`, `Item`, the item **states as a `std::variant`** (`Plain`, `Switch{on}`, `Door{lock, key}`), the `Action` interface (`virtual std::string perform(Game&, const std::vector<std::string>& args) const = 0`), and `LoadResult`. Where an item is: in a `Room::items` set or in `World::inventory`.

## The world file

```
# comment                                   start cellar
alias n go north                            room cellar "A damp cellar." dark
exit up kitchen via trapdoor                item lamp "a brass lamp" in inventory portable switch off
item oak-door "an oak door" in hall door locked key key
```

`start <room>` · `alias <word> <words…>` · `room <id> "<description>" [dark] [win]` · `exit <direction> <room> [via <item>]` (belongs to the room above) · `item <id> "<name>" in <room|inventory> [portable] [switch on|off] [door locked|closed|open [key <item>]]`.

- Ids, directions and alias words are **lower-case words** (`a-z`, digits, `-`, `_`). A `"quoted text"` is one word and has no quote inside; a `#` inside quotes is not a comment. Indentation, tabs, trailing blanks and `\r\n` do not matter. **Line numbers count every line**, blank and comment lines included.
- A mistake **found while reading a line** stops loading at once: `UnknownDirective`, `BadSyntax` (any malformed line, uppercase ids, an unclosed quote, repeated or unknown flags, `switch` together with `door`, two exits with the same direction in a room, the room name `inventory`), `DuplicateId` (a room, item, alias or `start` defined twice: the line of the second one), `ExitOutsideRoom`.
- References to rooms and items may point **forward** in the file. They are checked at the end: `UnknownReference` on the line that mentions the unknown id (if several, the smallest line). No `start` line at all is `MissingStart` (line 0). The order: reading errors first, then unknown references, then `MissingStart`.

## Playing: `execute(line)`

1. If the game is over: `The game is over.` 2. Split into lower-case words; none: `Say something.` 3. If the first word is an **alias**, replace it (once) by what it stands for. 4. Remove the articles `the`, `a`, `an` from the arguments. 5. Look the verb up in the table: unknown: `I don't understand.` 6. `perform`.

The exact messages (the tests compare them character for character):

- **look**: the description, then `You see: a, b.` (item names, sorted by id, only if there are any), then `Exits: north, up.` (in file order, only if any). In a dark room without light: `It is pitch dark.` **look at X** (`x`, `examine` are aliases): `This is a brass lamp. It is off.` (state sentences ` It is on.`/` off.`/` locked.`/` closed.`/` open.`; none for plain items).
- **go**: `You can't go that way.` · `The way is blocked by an oak door.` (an exit `via` a door that is not open; a `via` item that is not a door never blocks) · otherwise move and describe the new room; entering a `win` room appends `\n*** You have won! ***` and ends the game.
- **take**: `Taken.` · `You already have that.` · `You can't take that.` (not portable) · `You don't see that here.` **drop**: `Dropped.` · `You aren't carrying that.` **inventory**: `You are carrying: a, b.` (sorted by id) or `You are carrying nothing.`
- **open / close**: `You open it.` `It is already open.` `It is locked.` `You close it.` `It is already closed.` (a locked door is closed) `You can't open that.` / `You can't close that.`
- **unlock X with K / lock X with K**: checked in this order: the door is reachable; the key is carried (`You aren't carrying that.`); it is a door (`You can't unlock that.`); then for unlock `It isn't locked.`, `That doesn't fit.`, `You unlock it.`; for lock `Close it first.` (open), `It is already locked.`, `That doesn't fit.`, `You lock it.` Missing parts: `What do you want to unlock?`, `Unlock it with what?`; a different shape: `I don't understand.`
- **turn on/off X** (or `turn X on/off`): `You turn it on.` `It is already on.` `You can't switch that.` `Turn what on or off?` `Turn it on or off?`
- Missing noun: `What do you want to take/drop/open/close?` `Where do you want to go?` `What do you want to look at?` Extra words (`take lamp rope`, `go north now`): `I don't understand.`
- **Darkness**: a dark room is lit if a switch that is *on* is carried **or** lies in the room. Without light you cannot reach anything in the room; anything you cannot reach says `You can't see anything.` (with light: `You don't see that here.`). Carried things always work; moving and dropping work in the dark.

## The design rule

`game.hpp` **must not mention a verb** (apart from `start()` asking for `look`). Every verb is an `Action` in the table; `Game::add_action("shout", std::make_unique<Shout>())` adds one, and the tests do exactly that with classes you have never seen, and replace `look` with their own. If your `execute` has an `if (verb == "take")`, the extension tests fail by design.

## The tiers

- **Works**: loading, every verb with all its messages, aliases, articles, the whole walkthrough, extension with new `Action`s.
- **Robust**: all load errors with lines and their precedence, messy but valid files, darkness and light in every combination, aliases that point nowhere, the game-over state, 20000 random commands that must leave the world consistent (every item in exactly one place).
- **Fast**: the world is *moved* (the types count their copies: loading and playing copy no `Room`, `Item` or `World`); loading is linear with few allocations; a command costs the same in a 20000-room world as in a 100-room world.
- **Clean**: no raw `new`/`delete`, no exceptions, no `dynamic_cast`, at least eight `Action` classes, no verbs in `game.hpp`, states read through the variant.

## How to approach it

Loader first (the Works tests for it need nothing else), then `look` and `go`, then the item verbs, then doors and switches. `game.hpp` and `actions.hpp` include each other on purpose: the skeleton shows how; the `Game` class comes first, `#include "actions.hpp"` after it.
