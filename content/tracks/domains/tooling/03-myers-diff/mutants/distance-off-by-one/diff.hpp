// MUTANT: distance() is one too large
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

inline Lines split_lines(std::string_view text) {
    Lines out;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t nl = text.find('\n', start);
        if (nl == std::string_view::npos) {
            out.emplace_back(text.substr(start));
            break;
        }
        out.emplace_back(text.substr(start, nl - start + 1));
        start = nl + 1;
    }
    return out;
}

inline std::string join_lines(const Lines& lines) {
    std::string out;
    for (const std::string& l : lines) out += l;
    return out;
}

namespace detail {

struct Trimmed {
    std::size_t prefix, suffix;
};
inline Trimmed trim(const Lines& a, const Lines& b) {
    const std::size_t n = a.size(), m = b.size();
    std::size_t pre = 0;
    while (pre < n && pre < m && a[pre] == b[pre]) ++pre;
    std::size_t suf = 0;
    while (suf < n - pre && suf < m - pre && a[n - 1 - suf] == b[m - 1 - suf]) ++suf;
    return {pre, suf};
}

}  // namespace detail

inline std::vector<Edit> diff(const Lines& a, const Lines& b) {
    const detail::Trimmed t = detail::trim(a, b);
    const std::size_t pre = t.prefix, suf = t.suffix;
    const long N = static_cast<long>(a.size() - pre - suf), M = static_cast<long>(b.size() - pre - suf);
    std::vector<Edit> script;
    for (std::size_t i = 0; i < pre; ++i) script.push_back({Op::Equal, i, i});

    std::vector<Edit> middle;   // built backwards
    if (N > 0 || M > 0) {
        const long maxd = N + M, off = maxd + 1;
        std::vector<long> v(static_cast<std::size_t>(2 * maxd + 3), 0);
        std::vector<std::vector<long>> snapshots;   // snapshots[d] = v[-d .. d] after step d
        long found = -1;
        for (long d = 0; d <= maxd && found < 0; ++d) {
            for (long k = -d; k <= d; k += 2) {
                long x;
                if (k == -d || (k != d && v[static_cast<std::size_t>(off + k - 1)] < v[static_cast<std::size_t>(off + k + 1)])) x = v[static_cast<std::size_t>(off + k + 1)];
                else x = v[static_cast<std::size_t>(off + k - 1)] + 1;
                long y = x - k;
                while (x < N && y < M && a[pre + static_cast<std::size_t>(x)] == b[pre + static_cast<std::size_t>(y)]) {
                    ++x;
                    ++y;
                }
                v[static_cast<std::size_t>(off + k)] = x;
                if (x >= N && y >= M) {
                    found = d;
                    break;
                }
            }
            if (found < 0) snapshots.emplace_back(v.begin() + (off - d), v.begin() + (off + d + 1));
        }
        long x = N, y = M;
        for (long d = found; d > 0; --d) {
            const std::vector<long>& pv = snapshots[static_cast<std::size_t>(d - 1)];
            const auto at = [&](long k) { return pv[static_cast<std::size_t>(k + (d - 1))]; };
            const long k = x - y;
            const bool down = (k == -d || (k != d && at(k - 1) < at(k + 1)));
            const long prev_k = down ? k + 1 : k - 1;
            const long prev_x = at(prev_k), prev_y = prev_x - prev_k;
            while (x > prev_x && y > prev_y) {
                middle.push_back({Op::Equal, pre + static_cast<std::size_t>(x - 1), pre + static_cast<std::size_t>(y - 1)});
                --x;
                --y;
            }
            if (down) middle.push_back({Op::Insert, pre + static_cast<std::size_t>(prev_x), pre + static_cast<std::size_t>(prev_y)});
            else middle.push_back({Op::Delete, pre + static_cast<std::size_t>(prev_x), pre + static_cast<std::size_t>(prev_y)});
            x = prev_x;
            y = prev_y;
        }
        while (x > 0 && y > 0) {
            middle.push_back({Op::Equal, pre + static_cast<std::size_t>(x - 1), pre + static_cast<std::size_t>(y - 1)});
            --x;
            --y;
        }
        std::reverse(middle.begin(), middle.end());
    }
    // inside every run of changes: deletions first, then insertions, with the index fields recomputed
    std::size_t i = 0;
    while (i < middle.size()) {
        if (middle[i].op == Op::Equal) {
            script.push_back(middle[i++]);
            continue;
        }
        std::size_t j = i;
        std::size_t dels = 0, ins = 0;
        while (j < middle.size() && middle[j].op != Op::Equal) (middle[j++].op == Op::Delete ? dels : ins) += 1;
        const std::size_t a0 = middle[i].a, b0 = middle[i].b;
        for (std::size_t k = 0; k < dels; ++k) script.push_back({Op::Delete, a0 + k, b0});
        for (std::size_t k = 0; k < ins; ++k) script.push_back({Op::Insert, a0 + dels, b0 + k});
        i = j;
    }
    for (std::size_t s = 0; s < suf; ++s) script.push_back({Op::Equal, a.size() - suf + s, b.size() - suf + s});
    return script;
}

inline std::size_t distance(const Lines& a, const Lines& b) {
    const detail::Trimmed t = detail::trim(a, b);
    const long N = static_cast<long>(a.size() - t.prefix - t.suffix), M = static_cast<long>(b.size() - t.prefix - t.suffix);
    if (N == 0 && M == 0) return 0;
    const long maxd = N + M, off = maxd + 1;
    std::vector<long> v(static_cast<std::size_t>(2 * maxd + 3), 0);
    for (long d = 0; d <= maxd; ++d)
        for (long k = -d; k <= d; k += 2) {
            long x;
            if (k == -d || (k != d && v[static_cast<std::size_t>(off + k - 1)] < v[static_cast<std::size_t>(off + k + 1)])) x = v[static_cast<std::size_t>(off + k + 1)];
            else x = v[static_cast<std::size_t>(off + k - 1)] + 1;
            long y = x - k;
            while (x < N && y < M && a[t.prefix + static_cast<std::size_t>(x)] == b[t.prefix + static_cast<std::size_t>(y)]) {
                ++x;
                ++y;
            }
            v[static_cast<std::size_t>(off + k)] = x;
            if (x >= N && y >= M) return static_cast<std::size_t>(d) + 1;
        }
    return static_cast<std::size_t>(maxd);
}

namespace detail {
inline void put_line(std::string& out, char prefix, const std::string& line) {
    out += prefix;
    out += line;
    if (line.empty() || line.back() != '\n') out += "\n\\ No newline at end of file\n";
}
inline std::string range(std::size_t start, std::size_t count) {
    std::string s = std::to_string(count == 0 ? start : start + 1);
    if (count != 1) s += "," + std::to_string(count);
    return s;
}
}  // namespace detail

inline std::string unified(const Lines& a, const Lines& b, const std::vector<Edit>& edits, std::string_view name_a, std::string_view name_b, std::size_t context = 3) {
    std::string out;
    std::size_t i = 0, hunk_floor = 0;
    while (i < edits.size()) {
        while (i < edits.size() && edits[i].op == Op::Equal) ++i;
        if (i == edits.size()) break;
        std::size_t lo = i, lead = 0;
        while (lo > hunk_floor && lead < context && edits[lo - 1].op == Op::Equal) {
            --lo;
            ++lead;
        }
        std::size_t hi = i;   // one past the last edit of the hunk
        for (;;) {
            while (hi < edits.size() && edits[hi].op != Op::Equal) ++hi;
            std::size_t gap = 0;
            while (hi + gap < edits.size() && edits[hi + gap].op == Op::Equal) ++gap;
            if (hi + gap == edits.size() || gap > 2 * context) {
                hi += std::min(gap, context);
                break;
            }
            hi += gap;
        }
        if (out.empty()) out += "--- " + std::string(name_a) + "\n+++ " + std::string(name_b) + "\n";
        std::size_t a_count = 0, b_count = 0;
        for (std::size_t k = lo; k < hi; ++k) {
            if (edits[k].op != Op::Insert) ++a_count;
            if (edits[k].op != Op::Delete) ++b_count;
        }
        out += "@@ -" + detail::range(edits[lo].a, a_count) + " +" + detail::range(edits[lo].b, b_count) + " @@\n";
        for (std::size_t k = lo; k < hi; ++k) {
            const Edit& e = edits[k];
            if (e.op == Op::Equal) detail::put_line(out, ' ', a[e.a]);
            else if (e.op == Op::Delete) detail::put_line(out, '-', a[e.a]);
            else detail::put_line(out, '+', b[e.b]);
        }
        hunk_floor = hi;
        i = hi;
    }
    return out;
}

namespace detail {
// "N" or "N,M" starting at text[p]; advances p
inline bool number(std::string_view t, std::size_t& p, std::size_t& out) {
    if (p >= t.size() || t[p] < '0' || t[p] > '9') return false;
    std::size_t v = 0;
    while (p < t.size() && t[p] >= '0' && t[p] <= '9') {
        const std::size_t digit = static_cast<std::size_t>(t[p] - '0');
        if (v > (static_cast<std::size_t>(-1) - digit) / 10) return false;
        v = v * 10 + digit;
        ++p;
    }
    out = v;
    return true;
}
inline bool range_spec(std::string_view t, std::size_t& p, std::size_t& start, std::size_t& count) {
    if (!number(t, p, start)) return false;
    count = 1;
    if (p < t.size() && t[p] == ',') {
        ++p;
        if (!number(t, p, count)) return false;
    }
    return true;
}
inline bool header(std::string_view line, std::size_t& a_start, std::size_t& a_count, std::size_t& b_start, std::size_t& b_count) {
    if (line.substr(0, 4) != "@@ -") return false;
    std::size_t p = 4;
    if (!range_spec(line, p, a_start, a_count)) return false;
    if (line.substr(p, 2) != " +") return false;
    p += 2;
    if (!range_spec(line, p, b_start, b_count)) return false;
    return line.substr(p, 3) == " @@";
}
}  // namespace detail

inline std::optional<Lines> apply_patch(const Lines& a, std::string_view patch_text) {
    const Lines p = split_lines(patch_text);
    Lines out;
    std::size_t pos = 0;   // lines of `a` consumed so far
    std::size_t i = 0;
    while (i < p.size() && p[i].compare(0, 2, "@@") != 0) ++i;   // skip --- / +++ / diff lines
    while (i < p.size()) {
        std::size_t a_start, a_count, b_start, b_count;
        std::string_view head = p[i];
        while (!head.empty() && (head.back() == '\n' || head.back() == '\r')) head.remove_suffix(1);
        if (!detail::header(head, a_start, a_count, b_start, b_count)) return std::nullopt;
        ++i;
        const std::size_t a_idx = a_count == 0 ? a_start : a_start - (a_start > 0 ? 1 : 0);
        if (a_count > 0 && a_start == 0) return std::nullopt;
        if (a_idx < pos || a_idx > a.size()) return std::nullopt;
        const std::size_t b_idx = b_count == 0 ? b_start : b_start - (b_start > 0 ? 1 : 0);
        if (b_count > 0 && b_start == 0) return std::nullopt;
        while (pos < a_idx) out.push_back(a[pos++]);
        if (b_idx != out.size()) return std::nullopt;   // the header says where the hunk lands in the new file
        std::size_t old_left = a_count, new_left = b_count;
        while (old_left > 0 || new_left > 0) {
            if (i >= p.size()) return std::nullopt;
            std::string line = p[i++];
            char kind = line[0];
            std::string text = line.substr(1);
            if (line == "\n") {
                kind = ' ';
                text = "\n";
            }
            if (i < p.size() && p[i].compare(0, 2, "\\ ") == 0) {   // "\ No newline at end of file"
                if (text.empty() || text.back() != '\n') return std::nullopt;
                text.pop_back();
                ++i;
            }
            if (kind == ' ' || kind == '-') {
                if (old_left == 0 || pos >= a.size() || a[pos] != text) return std::nullopt;
                --old_left;
                ++pos;
                if (kind == ' ') {
                    if (new_left == 0) return std::nullopt;
                    --new_left;
                    out.push_back(text);
                }
            } else if (kind == '+') {
                if (new_left == 0) return std::nullopt;
                --new_left;
                out.push_back(text);
            } else {
                return std::nullopt;
            }
        }
        while (i < p.size() && p[i].compare(0, 2, "\\ ") == 0) return std::nullopt;   // a marker with no line before it
        if (i < p.size() && p[i].compare(0, 2, "@@") != 0) return std::nullopt;       // text after the hunk
    }
    while (pos < a.size()) out.push_back(a[pos++]);
    return out;
}

}  // namespace difftool
