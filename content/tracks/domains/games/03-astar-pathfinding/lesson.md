# A* Pathfinding

> **You're an engineer at a game studio.** Monsters walk through walls in the demo, and the first attempt at fixing it (breadth-first search) makes the frame rate crawl on big maps and ignores that mud is slower than road. You need the algorithm every game uses: **A\***.

## The problem

`find_path(grid, start, goal)` returns the **cheapest** path as a list of cells, **including both ends**, moving up/down/left/right. Every cell has a cost to **enter** it (`1` open ground, `2`..`9` rough ground, `0` a wall); the cost of a path is the sum of the costs of the cells it enters (the start is free).

- `start == goal`: `{start}`. A wall or off-map start/goal, or an unreachable goal: an **empty** vector.
- Among equally cheap paths, any one is fine. The tests compare the *cost* with a reference Dijkstra on 400 random maps.

## How A* works

Keep a **priority queue** of cells ordered by `f = g + h`: `g` is the best known cost from the start, `h` an estimate of the cost still to pay. Repeatedly take the smallest `f`, and for each neighbour that you can reach more cheaply than before, record the new `g`, the parent, and push it. When the **goal is taken out of the queue**, its `g` is final (not when it is pushed!), and the parents give the path backwards.

- **`h` must never overestimate** (it must be *admissible*), otherwise the path may not be the cheapest. Every cell costs at least 1, so the Manhattan distance `|dx| + |dy|` is safe. `h = 0` is plain Dijkstra, correct but explores much more.
- **Settle each cell once.** A cell can be pushed several times with decreasing `g`; mark it *settled* when it comes out first and skip later stale entries.
- **Ties.** With `f` equal, prefer the entry with the larger `g` (closer to the goal). On open ground this makes the search go straight instead of filling a diamond.
- Use flat `std::vector`s indexed by `y * width + x` for `g`, parent and settled: no map node per cell.
- Read the map only with `grid.cost(x, y)` (it counts the calls; the cost of looking at cells is what the Fast tier measures). Rebuild the path in a **loop**, not by recursion: it can have 20000 cells.

## The tiers

- **Works**: straight and winding paths, no path, rough ground, the cost rule, a spiral.
- **Robust**: walls and off-map ends, tiny maps, edges, expensive-but-short vs cheap-but-long, 400 random maps against Dijkstra, a snake maze with a 20000-cell path.
- **Fast**: at most 30000 cell lookups corner to corner on an open 200x200 map (Dijkstra needs 160000), each cell expanded once (unreachable goal), the heuristic is really used, linear growth, at most 100 allocations.
- **Clean**: no raw memory, a `std::priority_queue` for the open list, flat vectors (no map/set/list).
