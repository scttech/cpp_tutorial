#pragma once
// YOUR DIFF AND PATCH TOOL (namespace difftool). Text is handled as LINES; a line keeps its own terminator.
//
// using Lines = std::vector<std::string>
// split_lines(text) -> Lines     cut after every '\n' (the '\n' stays in the line; a "\r\n" is just a line ending in "\r\n"); a last line without '\n' is a line of its own; "" has no lines.
//                                So "a\nb" is {"a\n", "b"}: and the line "b" is NOT equal to "b\n" (that is what makes a missing final newline show up in a diff).
// join_lines(lines) -> string    the concatenation (the inverse of split_lines)
//
// Op { Equal, Delete, Insert };  Edit { Op op; size_t a; size_t b; }
//   Equal:  line a of A equals line b of B.
//   Delete: line a of A is removed; b is the number of B lines already produced (the index in B where the deletion happens).
//   Insert: line b of B is added;   a is the number of A lines already consumed.
// diff(A, B) -> vector<Edit>     a SHORTEST edit script (fewest Delete + Insert), found with Myers' O(ND) algorithm (E. Myers, "An O(ND) Difference Algorithm", 1986):
//     for d = 0, 1, 2 ...: for each diagonal k = -d, -d + 2, ..., d: start at the farther of the two neighbours (x = v[k+1] by an INSERT/down step if k == -d or v[k-1] < v[k+1], else x = v[k-1] + 1 by a DELETE/right step),
//     then follow the "snake" of equal lines; stop at the first d that reaches (N, M). Remember v after every d, and backtrack through those copies to build the script.
//     Every line of A and of B appears exactly once in the script, in order. Within each run of changes between two Equal edits, all Deletes come first, then all Inserts.
//     Trim the lines that A and B share at the start and at the end before running the algorithm (this must not change the minimality). Time O((N + M) D), memory O(D^2) is fine.
//     Several shortest scripts usually exist (they pair up different equal lines): any shortest one is correct.
// distance(A, B) -> size_t       the number of Delete + Insert edits of a shortest script (D), computed WITHOUT building the script (no copies of v): O(N + M + D^2) time is fine, O(N + M) memory.
//
// unified(A, B, edits, name_a, name_b, context = 3) -> string     the unified diff of the given script (it need not be the output of diff()). Empty string if there is no change at all.
//     "--- name_a\n+++ name_b\n" then the hunks. A hunk is a group of changes with up to `context` unchanged lines before the first and after the last change. Two changes are in the same hunk when at most
//     2 * context unchanged lines separate them. Hunk header: "@@ -A_START[,A_COUNT] +B_START[,B_COUNT] @@\n" with 1-based starts, and counts = the number of lines of A / B in the hunk. ",COUNT" is left out when COUNT is 1.
//     When COUNT is 0 the start is the number of the line BEFORE the hunk (0 at the beginning of a file): "@@ -0,0 +1,3 @@" for a new file, "@@ -3,0 +4,2 @@" for lines added after line 3.
//     Body lines: ' ' + line for Equal, '-' + line for Delete, '+' + line for Insert. A line that does not end in '\n' is followed by "\n\\ No newline at end of file\n".
//
// apply_patch(A, patch_text) -> optional<Lines>      apply a unified diff to A. Lines before the first "@@" (the --- and +++ lines, "diff ..." lines) are ignored. Hunks must be in order and not overlap.
//     Every ' ' and '-' line must match A exactly at its position (no fuzz, no offset search); the header counts must match the body, and the new-file start in the header must match where the hunk lands.
//     A body line that is completely empty (just "\n") counts as an empty context line. "\ No newline at end of file" removes the '\n' of the line before it. Anything that does not fit (bad header, wrong counts,
//     unknown line prefix, context mismatch, hunk beyond the end of A, text after the last hunk other than the no-newline marker, truncated patch) -> nullopt. An empty patch returns A unchanged.
//     For all A and B:  apply_patch(A, unified(A, B, diff(A, B), "a", "b")) == B.
#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace difftool {

using Lines = std::vector<std::string>;

enum class Op { Equal, Delete, Insert };

struct Edit {
    Op op;
    std::size_t a;
    std::size_t b;
};

inline Lines split_lines(std::string_view) { return {}; }   // TODO
inline std::string join_lines(const Lines&) { return ""; }   // TODO
inline std::vector<Edit> diff(const Lines&, const Lines&) { return {}; }   // TODO
inline std::size_t distance(const Lines&, const Lines&) { return 0; }   // TODO
inline std::string unified(const Lines&, const Lines&, const std::vector<Edit>&, std::string_view, std::string_view, std::size_t = 3) { return ""; }   // TODO
inline std::optional<Lines> apply_patch(const Lines&, std::string_view) { return std::nullopt; }   // TODO

}  // namespace difftool
