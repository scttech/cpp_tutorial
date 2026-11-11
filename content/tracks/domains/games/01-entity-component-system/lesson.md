# Entity-Component System

> **You're an engineer at a game studio.** The gameplay code has grown into a class hierarchy (`Enemy : Character : Entity`) and every new kind of object needs a new subclass; the physics loop chases pointers all over the heap and the frame rate shows it. The lead wants an **entity-component system**: an entity is just an id, its data is split into small components (`Position`, `Velocity`, `Health`...), and systems loop over components stored **contiguously**. This is how Unity DOTS, Bevy, EnTT and most modern engines work.

## What to build

Two header files. `types.hpp` (read-only) has `Entity`, the example components and the counters.

### `sparse_set.hpp`: `SparseSet<T>`

All components of one type, packed in a dense array so that iterating them is a straight walk through memory; a second array, indexed by entity id, tells where each entity's component lives.

```
sparse (by entity id):  [ - , 2 , - , 0 , 1 ]        dense entities:   [ 3 , 4 , 1 ]
                                                      dense components: [ c3, c4, c1 ]
```

- `emplace(id, value)` appends to the dense arrays and records the slot; if the entity already has the component, **replace the value** (no duplicates).
- `contains(id)` is only true if the dense slot points **back** at the same entity. (Slots are reused, so a stale sparse entry must never make `contains` lie.)
- `erase(id)` is **swap and pop**: move the *last* component into the hole, fix its sparse entry, drop the last element. O(1); no shifting.
- `entities()` and `components()` are the dense arrays, in the same order.

### `registry.hpp`: `Registry`

- **Generational handles.** `create()` hands out an `Entity{index, generation}`. `destroy(e)` removes all its components, **bumps the generation** of the slot and puts the slot on a free list; the next `create()` reuses it. An old handle (with the old generation) is now **stale**: `alive()` is false and nothing you do through it may touch the new entity.
- One `SparseSet` per component type, held behind a common base class (**type erasure**) in a map from `std::type_index`. `destroy` has to erase from *every* store without knowing the types.
- `add / get / has / remove / count` as in the header comment. Dead, stale or invented handles give `nullptr` / `false`, never a crash.
- `each<A, B, ...>(f)` calls `f(Entity, A&, B&, ...)` for every entity that has **all** of them. Walk the **smallest** store and test the others with `contains`: a query for the rare `Frozen` among 200000 `Position`s must cost 3 steps, not 200000.

## The tiers

- **Works**: components, `each` with 1 to 3 types, destroy and slot reuse, stale handles, the sparse set on its own.
- **Robust**: replace vs duplicate, erasing first/last/only elements, ids up to a million, stale sparse entries, 4000 random operations against a `std::map` model, dead/null/invented handles in every function, slot reuse (no growth under churn), `each` on missing types, and a random mix of everything against a model.
- **Fast**: each `add` moves a component at most 4 times and copies none; removal is swap-and-pop (50000 erases, at most 50000 moves); `each` walks the smallest store (3000 rare queries among 200000 entities); `each` copies and moves nothing; no allocation per entity or per component.
- **Clean**: no raw memory, a sparse set made of arrays (no maps or lists), `each` takes a template callable.

## Tips

Write `SparseSet` first and test it alone (the last Works test and the robust ones target it). For `Registry` start with entities (create/alive/destroy without components), then one component type, then type erasure, then `each`. A fold expression `(find_store<Cs>()->set.contains(id) && ...)` checks all the stores.
