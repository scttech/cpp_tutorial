#define ZTH_TRACK_ALLOCATIONS
#include <deque>
#include <string>
#include <vector>

#include "ring_logger.hpp"
#include "types.hpp"
#include "zth_test.hpp"

using namespace flightlog;

namespace {

constexpr std::size_t H = RingLogger::kHeaderBytes;

std::string seqs(const RingLogger& l) {
    std::string s;
    for (const Entry& e : l.snapshot()) s += std::to_string(e.seq) + " ";
    return s;
}
std::string msgs(const RingLogger& l) {
    std::string s;
    for (const Entry& e : l.snapshot()) s += e.message + "|";
    return s;
}

// The obvious slow reference: a deque of records.
struct Model {
    std::size_t cap;
    std::deque<Entry> recs;
    std::size_t used = 0;
    std::uint64_t total = 0, evicted = 0, truncated = 0, filtered = 0, lost = 0;
    Level min = Level::Debug;
    explicit Model(std::size_t c) : cap(c) {}
    bool log(Level l, std::string_view m) {
        if (l < min) {
            ++filtered;
            return false;
        }
        Entry e;
        e.seq = ++total;
        e.level = l;
        if (cap <= H) {
            ++lost;
            return false;
        }
        std::size_t len = m.size();
        if (H + len > cap) {
            len = cap - H;
            ++truncated;
        }
        while (cap - used < H + len) {
            used -= H + recs.front().message.size();
            recs.pop_front();
            ++evicted;
        }
        e.message = std::string(m.substr(0, len));
        recs.push_back(e);
        used += H + len;
        return true;
    }
};

bool same(const RingLogger& l, const Model& m, std::string& why) {
    if (l.size() != m.recs.size()) why = "size " + std::to_string(l.size()) + " instead of " + std::to_string(m.recs.size());
    else if (l.bytes_used() != m.used) why = "bytes_used " + std::to_string(l.bytes_used()) + " instead of " + std::to_string(m.used);
    else if (l.total_logged() != m.total) why = "total_logged";
    else if (l.evicted() != m.evicted) why = "evicted " + std::to_string(l.evicted()) + " instead of " + std::to_string(m.evicted);
    else if (l.truncated() != m.truncated) why = "truncated";
    else if (l.filtered() != m.filtered) why = "filtered";
    else if (l.lost() != m.lost) why = "lost";
    else {
        const std::vector<Entry> got = l.snapshot();
        for (std::size_t i = 0; i < got.size(); ++i) {
            if (got[i].seq != m.recs[i].seq || got[i].level != m.recs[i].level || got[i].message != m.recs[i].message) {
                why = "record " + std::to_string(i) + " differs: seq " + std::to_string(got[i].seq) + " `" + got[i].message + "` instead of seq " + std::to_string(m.recs[i].seq) + " `" + m.recs[i].message + "`";
                return false;
            }
        }
        return true;
    }
    return false;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "an empty logger") {
    RingLogger l(100);
    ZTH_EXPECT_EQ(l.size(), std::size_t{0});
    ZTH_EXPECT_EQ(l.bytes_used(), std::size_t{0});
    ZTH_EXPECT_EQ(l.capacity(), std::size_t{100});
    ZTH_EXPECT_TRUE(l.snapshot().empty());
    ZTH_EXPECT_EQ(l.dump(), std::string(""));
    ZTH_EXPECT_EQ(l.total_logged(), std::uint64_t{0});
    ZTH_EXPECT_TRUE(l.min_level() == Level::Debug);
    ZTH_EXPECT_EQ(H, std::size_t{13});
}
ZTH_TEST("works", "one record: stored, sequence number 1, header plus message bytes") {
    RingLogger l(100);
    ZTH_EXPECT_TRUE(l.log(Level::Info, "hello"));
    ZTH_EXPECT_EQ(l.size(), std::size_t{1});
    ZTH_EXPECT_EQ(l.bytes_used(), H + 5);
    ZTH_EXPECT_EQ(l.total_logged(), std::uint64_t{1});
    const auto s = l.snapshot();
    ZTH_EXPECT_EQ(s.size(), std::size_t{1});
    if (s.size() != 1) return;
    ZTH_EXPECT_EQ(s[0].seq, std::uint64_t{1});
    ZTH_EXPECT_TRUE(s[0].level == Level::Info);
    ZTH_EXPECT_EQ(s[0].message, std::string("hello"));
}
ZTH_TEST("works", "records come back oldest first, with their levels and consecutive sequence numbers") {
    RingLogger l(500);
    l.log(Level::Debug, "a");
    l.log(Level::Info, "bb");
    l.log(Level::Warn, "ccc");
    l.log(Level::Error, "dddd");
    const auto s = l.snapshot();
    ZTH_EXPECT_EQ(s.size(), std::size_t{4});
    if (s.size() != 4) return;
    const Level levels[] = {Level::Debug, Level::Info, Level::Warn, Level::Error};
    const char* texts[] = {"a", "bb", "ccc", "dddd"};
    for (std::size_t i = 0; i < 4; ++i) {
        ZTH_EXPECT_EQ(s[i].seq, static_cast<std::uint64_t>(i + 1));
        ZTH_EXPECT_TRUE(s[i].level == levels[i]);
        ZTH_EXPECT_EQ(s[i].message, std::string(texts[i]));
    }
    ZTH_EXPECT_EQ(l.bytes_used(), 4 * H + 10);
}
ZTH_TEST("works", "an empty message is a record of its own") {
    RingLogger l(100);
    ZTH_EXPECT_TRUE(l.log(Level::Warn, ""));
    ZTH_EXPECT_EQ(l.size(), std::size_t{1});
    ZTH_EXPECT_EQ(l.bytes_used(), H);
    ZTH_EXPECT_EQ(msgs(l), std::string("|"));
}
ZTH_TEST("works", "messages are bytes: embedded zeros, newlines and high bytes survive") {
    RingLogger l(200);
    const std::string weird("a\0b\nc\xff\x80\r", 8);
    ZTH_EXPECT_TRUE(l.log(Level::Info, weird));
    const auto s = l.snapshot();
    ZTH_EXPECT_EQ(s.size(), std::size_t{1});
    if (s.size() == 1) ZTH_EXPECT_EQ(s[0].message, weird);
}
ZTH_TEST("works", "the level filter: below the minimum nothing is stored, nothing is counted as accepted, no sequence number is used") {
    RingLogger l(500);
    l.set_min_level(Level::Warn);
    ZTH_EXPECT_TRUE(l.min_level() == Level::Warn);
    ZTH_EXPECT_FALSE(l.log(Level::Debug, "d"));
    ZTH_EXPECT_FALSE(l.log(Level::Info, "i"));
    ZTH_EXPECT_EQ(l.filtered(), std::uint64_t{2});
    ZTH_EXPECT_EQ(l.total_logged(), std::uint64_t{0});
    ZTH_EXPECT_EQ(l.size(), std::size_t{0});
    ZTH_EXPECT_TRUE(l.log(Level::Warn, "w"));      // exactly the minimum is accepted
    ZTH_EXPECT_TRUE(l.log(Level::Error, "e"));
    ZTH_EXPECT_EQ(seqs(l), std::string("1 2 "));
    ZTH_EXPECT_EQ(l.filtered(), std::uint64_t{2});
    l.set_min_level(Level::Debug);
    ZTH_EXPECT_TRUE(l.log(Level::Debug, "now ok"));
    ZTH_EXPECT_EQ(seqs(l), std::string("1 2 3 "));
}
ZTH_TEST("works", "when the buffer is full the oldest records are overwritten") {
    RingLogger l(3 * (H + 5));   // exactly three 5-byte records
    for (int i = 1; i <= 5; ++i) ZTH_EXPECT_TRUE(l.log(Level::Info, "msg" + std::to_string(i % 10) + "!"));
    ZTH_EXPECT_EQ(l.size(), std::size_t{3});
    ZTH_EXPECT_EQ(l.bytes_used(), 3 * (H + 5));
    ZTH_EXPECT_EQ(l.evicted(), std::uint64_t{2});
    ZTH_EXPECT_EQ(seqs(l), std::string("3 4 5 "));
    ZTH_EXPECT_EQ(msgs(l), std::string("msg3!|msg4!|msg5!|"));
}
ZTH_TEST("works", "a big record evicts as many old records as it needs, no more") {
    RingLogger l(100);
    for (int i = 0; i < 3; ++i) l.log(Level::Info, std::string(10, static_cast<char>('a' + i)));   // 3 x 23 = 69 bytes
    ZTH_EXPECT_EQ(l.bytes_used(), std::size_t{69});
    ZTH_EXPECT_TRUE(l.log(Level::Info, std::string(40, 'X')));   // needs 53: free is 31, so ONE eviction (free 54)
    ZTH_EXPECT_EQ(l.evicted(), std::uint64_t{1});
    ZTH_EXPECT_EQ(seqs(l), std::string("2 3 4 "));
    ZTH_EXPECT_EQ(l.bytes_used(), std::size_t{99});
    ZTH_EXPECT_TRUE(l.log(Level::Info, std::string(60, 'Y')));   // needs 73: everything older goes
    ZTH_EXPECT_EQ(l.evicted(), std::uint64_t{4});
    ZTH_EXPECT_EQ(seqs(l), std::string("5 "));
    ZTH_EXPECT_EQ(l.bytes_used(), std::size_t{73});
}
ZTH_TEST("works", "a record that fits exactly evicts nothing; one byte less room evicts") {
    RingLogger l(2 * (H + 4));
    l.log(Level::Info, "aaaa");
    ZTH_EXPECT_TRUE(l.log(Level::Info, "bbbb"));   // fills the buffer exactly
    ZTH_EXPECT_EQ(l.evicted(), std::uint64_t{0});
    ZTH_EXPECT_EQ(l.bytes_used(), 2 * (H + 4));
    RingLogger m(2 * (H + 4) - 1);
    m.log(Level::Info, "aaaa");
    ZTH_EXPECT_TRUE(m.log(Level::Info, "bbbb"));
    ZTH_EXPECT_EQ(m.evicted(), std::uint64_t{1});
    ZTH_EXPECT_EQ(m.size(), std::size_t{1});
}
ZTH_TEST("works", "a message longer than the whole buffer is cut to what fits, and everything older is evicted") {
    RingLogger l(40);
    l.log(Level::Info, "old");
    ZTH_EXPECT_TRUE(l.log(Level::Error, std::string(100, 'z')));
    ZTH_EXPECT_EQ(l.truncated(), std::uint64_t{1});
    ZTH_EXPECT_EQ(l.size(), std::size_t{1});
    ZTH_EXPECT_EQ(l.bytes_used(), std::size_t{40});
    ZTH_EXPECT_EQ(l.evicted(), std::uint64_t{1});
    ZTH_EXPECT_EQ(msgs(l), std::string(27, 'z') + "|");
    ZTH_EXPECT_EQ(seqs(l), std::string("2 "));
}
ZTH_TEST("works", "a record that fills the buffer exactly is not truncated") {
    RingLogger l(40);
    ZTH_EXPECT_TRUE(l.log(Level::Info, std::string(27, 'q')));
    ZTH_EXPECT_EQ(l.truncated(), std::uint64_t{0});
    ZTH_EXPECT_EQ(msgs(l), std::string(27, 'q') + "|");
    ZTH_EXPECT_TRUE(l.log(Level::Info, std::string(28, 'r')));
    ZTH_EXPECT_EQ(l.truncated(), std::uint64_t{1});
    ZTH_EXPECT_EQ(msgs(l), std::string(27, 'r') + "|");
}
ZTH_TEST("works", "a buffer too small for even a header stores nothing, but the log still gets a sequence number") {
    for (std::size_t cap : {std::size_t{0}, std::size_t{1}, std::size_t{12}, std::size_t{13}}) {
        RingLogger l(cap);
        ZTH_EXPECT_FALSE(l.log(Level::Error, "x"));
        ZTH_EXPECT_FALSE(l.log(Level::Error, ""));
        ZTH_EXPECT_EQ(l.lost(), std::uint64_t{2});
        ZTH_EXPECT_EQ(l.total_logged(), std::uint64_t{2});
        ZTH_EXPECT_EQ(l.size(), std::size_t{0});
        ZTH_EXPECT_EQ(l.bytes_used(), std::size_t{0});
        ZTH_EXPECT_TRUE(l.snapshot().empty());
        ZTH_EXPECT_EQ(l.dump(), std::string(""));
        ZTH_EXPECT_EQ(l.truncated(), std::uint64_t{0});
    }
    RingLogger l(14);   // room for a header and ONE byte
    ZTH_EXPECT_TRUE(l.log(Level::Info, "abc"));
    ZTH_EXPECT_EQ(msgs(l), std::string("a|"));
    ZTH_EXPECT_EQ(l.truncated(), std::uint64_t{1});
    ZTH_EXPECT_EQ(l.lost(), std::uint64_t{0});
}
ZTH_TEST("works", "dump writes one line per record, oldest first") {
    RingLogger l(500);
    l.log(Level::Debug, "starting");
    l.log(Level::Info, "listening on :8080");
    l.log(Level::Warn, "low disk");
    l.log(Level::Error, "boom");
    ZTH_EXPECT_EQ(l.dump(), std::string("#1 DEBUG starting\n#2 INFO listening on :8080\n#3 WARN low disk\n#4 ERROR boom\n"));
}
ZTH_TEST("works", "clear forgets the records but not the counters, and the sequence numbers go on") {
    RingLogger l(100);
    l.log(Level::Info, "a");
    l.log(Level::Info, "b");
    l.clear();
    ZTH_EXPECT_EQ(l.size(), std::size_t{0});
    ZTH_EXPECT_EQ(l.bytes_used(), std::size_t{0});
    ZTH_EXPECT_EQ(l.dump(), std::string(""));
    ZTH_EXPECT_EQ(l.total_logged(), std::uint64_t{2});
    ZTH_EXPECT_TRUE(l.log(Level::Info, "c"));
    ZTH_EXPECT_EQ(seqs(l), std::string("3 "));
    ZTH_EXPECT_EQ(msgs(l), std::string("c|"));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "records that straddle the end of the buffer, header included, come back intact") {
    // 32 bytes of buffer and 18-byte records: the write position moves by 18 each time, so every possible split of a record is met.
    for (std::size_t cap : {std::size_t{32}, std::size_t{33}, std::size_t{37}, std::size_t{50}}) {
        RingLogger l(cap);
        for (int i = 1; i <= 400; ++i) {
            const std::string msg = std::string(5, static_cast<char>('A' + i % 26));
            l.log(Level::Warn, msg);
            const auto s = l.snapshot();
            if (s.empty() || s.back().message != msg || s.back().seq != static_cast<std::uint64_t>(i) || !(s.back().level == Level::Warn)) {
                zth::fail("capacity " + std::to_string(cap) + ": the newest record is wrong after log #" + std::to_string(i));
                return;
            }
            for (std::size_t k = 1; k < s.size(); ++k) {
                if (s[k].seq != s[k - 1].seq + 1) {
                    zth::fail("capacity " + std::to_string(cap) + ": the sequence numbers are not consecutive after log #" + std::to_string(i));
                    return;
                }
            }
        }
    }
}
ZTH_TEST("robust", "10 buffer sizes x 6000 random operations agree with a deque-based model: contents, bytes, every counter") {
    zth::Lcg rng(4242);
    for (std::size_t cap : {std::size_t{0}, std::size_t{5}, std::size_t{13}, std::size_t{14}, std::size_t{20}, std::size_t{37}, std::size_t{64}, std::size_t{100}, std::size_t{257}, std::size_t{1000}}) {
        RingLogger l(cap);
        Model m(cap);
        for (int step = 0; step < 6000; ++step) {
            const int op = rng.below(100);
            if (op < 3) {
                const Level lv = static_cast<Level>(rng.below(4));
                l.set_min_level(lv);
                m.min = lv;
            } else if (op < 5) {
                l.clear();
                m.recs.clear();
                m.used = 0;
            } else {
                const Level lv = static_cast<Level>(rng.below(4));
                const int len = rng.below(10) == 0 ? rng.below(400) : rng.below(50);
                std::string msg;
                for (int i = 0; i < len; ++i) msg += static_cast<char>(rng.below(256));
                const bool a = l.log(lv, msg);
                const bool b = m.log(lv, msg);
                if (a != b) {
                    zth::fail("capacity " + std::to_string(cap) + ", step " + std::to_string(step) + ": log() returned " + (a ? "true" : "false") + ", expected " + (b ? "true" : "false"));
                    return;
                }
            }
            std::string why;
            if (step % 3 == 0 && !same(l, m, why)) {
                zth::fail("capacity " + std::to_string(cap) + ", step " + std::to_string(step) + ": " + why);
                return;
            }
        }
        std::string why;
        if (!same(l, m, why)) {
            zth::fail("capacity " + std::to_string(cap) + " at the end: " + why);
            return;
        }
    }
}
ZTH_TEST("robust", "long messages: lengths above 255 and above 65535 are stored whole when they fit") {
    RingLogger l(300000);
    const std::string a(300, 'a'), b(70000, 'b'), c(200000, 'c');
    ZTH_EXPECT_TRUE(l.log(Level::Info, a));
    ZTH_EXPECT_TRUE(l.log(Level::Info, b));
    ZTH_EXPECT_TRUE(l.log(Level::Info, c));
    const auto s = l.snapshot();
    ZTH_EXPECT_EQ(s.size(), std::size_t{3});
    if (s.size() != 3) return;
    ZTH_EXPECT_TRUE(s[0].message == a);
    ZTH_EXPECT_TRUE(s[1].message == b);
    ZTH_EXPECT_TRUE(s[2].message == c);
}
ZTH_TEST("robust", "sequence numbers above 255 and 65535 are kept whole") {
    RingLogger l(64);
    for (int i = 0; i < 70000; ++i) l.log(Level::Info, "x");
    const auto s = l.snapshot();
    ZTH_EXPECT_TRUE(!s.empty());
    if (s.empty()) return;
    ZTH_EXPECT_EQ(s.back().seq, std::uint64_t{70000});
    ZTH_EXPECT_EQ(s.front().seq, std::uint64_t{70000} - s.size() + 1);
    ZTH_EXPECT_EQ(l.total_logged(), std::uint64_t{70000});
}
ZTH_TEST("robust", "the message is copied: changing the caller's string afterwards changes nothing") {
    RingLogger l(200);
    std::string text = "original";
    l.log(Level::Info, text);
    text.assign("changed!");
    ZTH_EXPECT_EQ(msgs(l), std::string("original|"));
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "constructing the logger allocates its buffer once") {
    zth::mem::Scope construct;
    RingLogger l(4096);
    ZTH_EXPECT_ALLOCS_AT_MOST(construct, 1);
}
ZTH_TEST("fast", "logging allocates nothing, ever, even for messages longer than the small-string buffer") {
    RingLogger l(4096);
    const std::string_view long_message = "a message that is certainly longer than any small string optimisation buffer could ever hold";
    zth::mem::Scope scope;
    for (int i = 0; i < 100000; ++i) l.log(static_cast<Level>(i % 4), long_message);
    l.log(Level::Info, std::string_view(long_message.data(), 0));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(l.evicted() > 0);
}
ZTH_TEST("fast", "filtered logs and logs into a tiny buffer allocate nothing either") {
    RingLogger tiny(10);
    RingLogger filtered(1000);
    filtered.set_min_level(Level::Error);
    const std::string_view long_message = "a message that is certainly longer than any small string optimisation buffer could ever hold";
    zth::mem::Scope scope;
    for (int i = 0; i < 1000; ++i) {
        tiny.log(Level::Error, long_message);
        filtered.log(Level::Info, long_message);
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST_TIMEOUT("fast", "logging is O(message): 2 million logs into a 64 KiB buffer finish at once (no shifting or compacting of the buffer)", 20) {
    RingLogger l(64 * 1024);
    for (int i = 0; i < 2000000; ++i) l.log(Level::Info, "request handled in 12 ms");
    ZTH_EXPECT_TRUE(l.size() > 1000);
    ZTH_EXPECT_TRUE(l.bytes_used() <= l.capacity());
    ZTH_EXPECT_EQ(l.total_logged(), std::uint64_t{2000000});
    const auto last = l.snapshot();
    ZTH_EXPECT_EQ(last.back().seq, std::uint64_t{2000000});
}
ZTH_TEST_TIMEOUT("fast", "one huge message in a huge buffer, repeatedly", 20) {
    RingLogger l(3 * 1024 * 1024);
    const std::string big(1024 * 1024, 'm');
    for (int i = 0; i < 40; ++i) ZTH_EXPECT_TRUE(l.log(Level::Info, big));
    ZTH_EXPECT_EQ(l.size(), std::size_t{2});   // 2 x (1 MiB + 13) fit in 3 MiB, 3 do not
    ZTH_EXPECT_EQ(l.evicted(), std::uint64_t{38});
}
ZTH_TEST("fast", "memory stays bounded: a million logs never use more than the capacity") {
    RingLogger l(1000);
    for (int i = 0; i < 1000000; ++i) {
        l.log(Level::Info, "0123456789");
        if (l.bytes_used() > l.capacity()) {
            zth::fail("bytes_used exceeded the capacity at log " + std::to_string(i));
            return;
        }
    }
    ZTH_EXPECT_EQ(l.size(), std::size_t{1000 / 23});
    ZTH_EXPECT_EQ(l.bytes_used(), std::size_t{(1000 / 23) * 23});
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("ring_logger.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("ring_logger.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "one flat buffer: no deque, list, queue, map or per-record allocation") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("ring_logger.hpp"));
    for (const char* word : {"deque", "list", "queue", "forward_list", "map", "unordered_map", "set", "unique_ptr", "shared_ptr", "make_unique", "make_shared", "printf", "cout", "memmove", "rotate"})
        ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "the input is a string_view") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("ring_logger.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "string_view").empty());
}
