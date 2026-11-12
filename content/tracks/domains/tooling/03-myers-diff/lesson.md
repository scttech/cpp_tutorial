# Myers Diff and Unified Patches

> **You're on the developer-tools team.** `git diff`, code review, merge tools, `patch`, test frameworks that print what changed, and your editor's "compare files" all answer the same question: *what is the smallest set of lines to delete and insert to turn this file into that one?* The classic answer is Eugene Myers' 1986 algorithm, which GNU diff, Git and most libraries use. It is short, elegant and full of off-by-one traps, and unlike most algorithm exercises you can check your answer against a trusted oracle: a reference that finds the *shortest* script with a brute-force table, and a round trip (`apply(A, diff(A, B)) == B`).

## What a diff is

A diff is an **edit script**: a sequence of *Equal*, *Delete* and *Insert* steps that walks through both files in order. The best script has the fewest Deletes plus Inserts; their number `D` is the *edit distance*, and `D = N + M - 2 * LCS`, where the LCS is the longest common subsequence of lines. Computing the LCS by dynamic programming costs `O(N * M)` time **and** memory: for two 100000-line files that is 10^10 cells. Real changes are small, which Myers exploits: his algorithm runs in `O((N + M) * D)`.

## The algorithm in one picture

Draw an edit grid: A along the top (x), B down the side (y). A path from (0,0) to (N,M) moves **right** (delete a line of A), **down** (insert a line of B) or **diagonally for free** where the lines are equal. Diagonal `k = x - y`. For each cost `d = 0, 1, 2, ...` and each diagonal `k` of the right parity, compute the furthest `x` reachable with exactly `d` edits, `v[k]`:

- arrive from diagonal `k-1` by a right step (`x = v[k-1] + 1`) or from `k+1` by a down step (`x = v[k+1]`): take the one that got further; on the edge diagonals only one choice exists;
- then slide along the diagonal while the lines are equal (the "snake");
- the first `d` for which a path reaches `(N, M)` is the edit distance.

To recover the script you need the path, so **save a copy of `v` after every `d`** and walk backwards: at step `d`, on diagonal `k`, the previous step came from `k-1` or `k+1` (re-run the same comparison on the saved copy), and the snake is every diagonal move in between. The copies cost `O(D^2)` memory; the refinement that avoids them (divide and conquer on the "middle snake") is described in the paper and in the notes.

Three more practical points the tests insist on:

1. **Trim** the common prefix and suffix first. It is free, and most real diffs shrink to a handful of lines. Be careful when the prefix and the suffix would overlap (`ABAB` vs `ABABAB`).
2. **Several shortest scripts exist** (`AB` to `BA` can keep either letter). Any shortest script is correct; do not assume your tie-breaking equals Git's. The tests therefore check *validity* and *length*, not one particular alignment. What they do fix is the order inside a block of changes: deletions first, then insertions, because that is how humans read a diff.
3. **A line includes its newline.** `"b"` and `"b\n"` are different lines, so a missing final newline is a real change (and `\ No newline at end of file` is how diff says so).

## Unified format

```
--- old.txt
+++ new.txt
@@ -2,7 +2,7 @@
 context
-removed
+added
 context
```

The header `@@ -start,count +start,count @@` has 1-based line numbers; a count of 1 is written without `,1`; an empty range (`count == 0`) is written with the line number *before* the gap (`-0,0` for an empty file). Changes at most `2 * context` lines apart share a hunk (their context regions would touch or overlap anyway).

## `patch`: the proof

`apply_patch` reads the format back and applies it **strictly**: every context and removed line must match, the numbers in the header must match the body, hunks must come in order. No fuzz and no sliding to find the context: if the file is not what the patch was made against, say so. The Robust tier's round trip, `apply_patch(A, unified(A, B)) == B` for thousands of random pairs including final-newline combinations, is the strongest test of everything above at once.

## The tiers

- **Works**: line splitting, edit fields, shortest scripts, the paper's example, ordering inside blocks, hunks with exact golden output (context at the file ends, `,1`, empty ranges, merging threshold, no-newline markers), `apply_patch` on valid and a long list of invalid patches, overflowing numbers.
- **Robust**: 1500 random pairs against the O(NM) reference, realistic similar texts, tricky shapes, the round trip, CRLF and look-alike lines, arbitrary scripts.
- **Fast**: 100000 lines with 40 changes; 1500 against 1500 different lines (D = 3000); big identical inputs without big allocations; megabyte lines.
- **Clean**: no raw memory or exceptions; no library shortcuts; Myers' history of `v` is kept explicitly.
