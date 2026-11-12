# Table-Driven State Machine

> **You're a firmware engineer.** Almost every device is a state machine: a door, a charger, a modem, a button that distinguishes click from long press. Written as nested `switch` statements the logic spreads over hundreds of lines and nobody can see all the rules. Written as a **table** (one row per rule) it fits on a page, can sit in flash as `const` data, can be reviewed against the specification line by line, and the engine that interprets it is 60 lines you test once.

## The model

A row says: *in state `from`, on `event`, if `guard` allows, run `action` and go to `to`.* Hooks give a state an **entry** and an **exit** action (turn the light on when the door is open, off when it closes).

| Piece | Meaning |
|---|---|
| `Transition{from, event, to, guard, action, internal, any_state}` | one rule; `guard` and `action` are plain function pointers (`nullptr` = none) |
| `Hooks{state, on_entry, on_exit}` | optional per-state actions |
| `Result` | `Handled`, `NoTransition` (ignored, **not** an error), `GuardRejected` |

## The semantics you must get right

1. **Rows are tried in table order.** A row *matches* when its event equals the event and (`any_state` or `from == state`). The first matching row whose guard passes (or that has none) is taken, so **table order is priority**. A failed guard **falls through** to the next matching row: this is how you write "if the stock is empty refund, else dispense".
2. **A guard is only called for matching rows, and never after a row was taken.** Guards may be expensive (read a sensor) or have side effects; the counting tests make sure you do not evaluate every row.
3. **Order of a transition:** `on_exit(current)` → `action` → `on_entry(new)` → *then* the state variable changes. Entry hooks therefore see data written by the action.
4. **Self-transitions are real.** A row from A to A runs exit and entry again (restart a timer). If you want "react but stay, no hooks", the row must be **internal**: only its action runs, `to` is ignored.
5. **Wildcard rows** (`any_state`) apply in every state, but only after the specific rows before them in the table (e.g. "Reset from anywhere").
6. **Ignoring is normal.** An event nobody handles in the current state returns `NoTransition` and changes nothing: no hooks, no action.
7. **`start()`** runs the initial state's entry hook once; `dispatch()` calls it if you forgot, so the first event never skips the initial entry.

## The tiers

- **Works**: every rule above, with logs that show the exact order of hooks and actions, plus a vending-slot machine.
- **Robust**: 300 random tables and 18000 random events compared with a short reference interpreter (result, state, action log, guard-call counts, counters); two machines sharing a table; an empty table.
- **Fast**: a million dispatches with zero allocations, guard-call counts (a 2000-row table where one guard runs), a small object.
- **Clean**: no raw memory or exceptions; no `std::function`, containers, `virtual` or RTTI (firmware style); no `switch`/`case`: the logic is in the table.
