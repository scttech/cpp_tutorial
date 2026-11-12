#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <vector>

#include "dns.hpp"
#include "types.hpp"
#include "zth_test.hpp"

using namespace dns;
using Bytes = std::vector<std::uint8_t>;

namespace {

Bytes from_hex(const std::string& h) {
    Bytes b;
    for (std::size_t i = 0; i + 1 < h.size(); i += 2) b.push_back(static_cast<std::uint8_t>(std::stoi(h.substr(i, 2), nullptr, 16)));
    return b;
}
std::string to_hex(const Bytes& b) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (std::uint8_t x : b) {
        s += d[x >> 4];
        s += d[x & 15];
    }
    return s;
}
std::span<const std::uint8_t> sp(const Bytes& b) { return std::span<const std::uint8_t>(b.data(), b.size()); }
Bytes wire(const std::string& name) { return *encode_name(name); }
Bytes mx(std::uint16_t pref, const std::string& name) {
    Bytes b = {static_cast<std::uint8_t>(pref >> 8), static_cast<std::uint8_t>(pref & 255)};
    const Bytes n = wire(name);
    b.insert(b.end(), n.begin(), n.end());
    return b;
}
Bytes header(std::uint16_t id, std::uint16_t flags, int qd, int an, int ns, int ar) {
    Bytes b;
    for (int v : {static_cast<int>(id), static_cast<int>(flags), qd, an, ns, ar}) {
        b.push_back(static_cast<std::uint8_t>(v >> 8));
        b.push_back(static_cast<std::uint8_t>(v & 255));
    }
    return b;
}
void append(Bytes& a, const Bytes& b) { a.insert(a.end(), b.begin(), b.end()); }
Bytes question_tail() { return Bytes{0, 1, 0, 1}; }

Message query_msg() {
    Message m;
    m.id = 0xBEEF;
    m.flags = 0x0100;
    m.questions.push_back({"www.example.com", 1, 1});
    return m;
}
Message response_msg() {
    Message m;
    m.id = 0x1234;
    m.flags = 0x8180;
    m.questions.push_back({"www.example.com", 1, 1});
    m.answers.push_back({"www.example.com", 5, 1, 300, wire("web.example.net")});
    m.answers.push_back({"web.example.net", 1, 1, 60, {192, 0, 2, 1}});
    m.authorities.push_back({"example.net", 2, 1, 86400, wire("ns1.example.net")});
    m.additionals.push_back({"ns1.example.net", 1, 1, 86400, {192, 0, 2, 53}});
    return m;
}
Message mx_msg() {
    Message m;
    m.id = 7;
    m.flags = 0x8400;
    m.questions.push_back({"example.com", 15, 1});
    m.answers.push_back({"example.com", 15, 1, 3600, mx(10, "mail.example.com")});
    m.answers.push_back({"EXAMPLE.com", 15, 1, 3600, mx(20, "MAIL2.Example.COM")});
    return m;
}
Message root_msg() {
    Message m;
    m.id = 1;
    m.flags = 0x8000;
    m.questions.push_back({".", 2, 1});
    m.answers.push_back({".", 2, 1, 518400, wire("a.root-servers.net")});
    m.answers.push_back({"", 2, 1, 518400, wire("b.root-servers.net")});
    return m;
}
Message txt_msg() {
    Message m;
    m.id = 9;
    m.flags = 0x8180;
    m.questions.push_back({"a.b.example.org", 16, 1});
    m.answers.push_back({"a.b.example.org", 16, 1, 10, {5, 'h', 'e', 'l', 'l', 'o'}});
    m.answers.push_back({"b.example.org", 16, 1, 10, {3, 'a', 'b', 'c'}});
    return m;
}
Message late_msg() {
    Message m;
    m.id = 3;
    m.flags = 0x8180;
    m.questions.push_back({"example.com", 16, 1});
    Bytes big;
    for (int i = 0; i < 85; ++i) {
        big.push_back(200);
        big.insert(big.end(), 200, 'x');
    }
    m.answers.push_back({"big.example.com", 16, 1, 1, big});
    m.answers.push_back({"late.example.com", 1, 1, 1, {1, 2, 3, 4}});
    m.answers.push_back({"late.example.com", 1, 1, 1, {5, 6, 7, 8}});
    return m;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "encode_name: labels with length bytes and a zero at the end") {
    ZTH_EXPECT_EQ(to_hex(wire("www.example.com")), std::string("03777777076578616d706c6503636f6d00"));
    ZTH_EXPECT_EQ(to_hex(wire("www.example.com.")), std::string("03777777076578616d706c6503636f6d00"));
    ZTH_EXPECT_EQ(to_hex(wire("a")), std::string("016100"));
    ZTH_EXPECT_EQ(to_hex(wire(".")), std::string("00"));
    ZTH_EXPECT_EQ(to_hex(wire("")), std::string("00"));
    ZTH_EXPECT_EQ(to_hex(wire("A-1_b")), std::string("05412d315f6200"));
}
ZTH_TEST("works", "encode_name refuses what cannot be a name") {
    for (const char* bad : {"a..b", ".a", "..", "a..", "a b", "a\x01" "b", "a/b\xFF", "a.\x7F"}) {
        if (encode_name(bad).has_value()) {
            zth::fail(std::string("`") + bad + "` should be rejected");
            return;
        }
    }
    ZTH_EXPECT_TRUE(encode_name(std::string(63, 'a')).has_value());
    ZTH_EXPECT_FALSE(encode_name(std::string(64, 'a')).has_value());
    // 63 + 63 + 63 + 61 labels = 3 * 64 + 62 + 1 = 255 bytes on the wire: the longest legal name; one more character is too long
    const std::string l63(63, 'x');
    ZTH_EXPECT_TRUE(encode_name(l63 + "." + l63 + "." + l63 + "." + std::string(61, 'y')).has_value());
    ZTH_EXPECT_FALSE(encode_name(l63 + "." + l63 + "." + l63 + "." + std::string(62, 'y')).has_value());
    ZTH_EXPECT_EQ(wire(l63 + "." + l63 + "." + l63 + "." + std::string(61, 'y')).size(), std::size_t{255});
}
ZTH_TEST("works", "decode_name reads plain names and moves pos past the terminator") {
    const Bytes m = from_hex("000000000000000000000000" "03777777076578616d706c6503636f6d00" "ff");
    std::size_t pos = 12;
    const auto n = decode_name(sp(m), pos);
    ZTH_EXPECT_TRUE(n.has_value() && *n == "www.example.com");
    ZTH_EXPECT_EQ(pos, std::size_t{12 + 17});
    std::size_t root = 12;
    const Bytes r = from_hex("000000000000000000000000" "00");
    const auto rn = decode_name(sp(r), root);
    ZTH_EXPECT_TRUE(rn.has_value() && *rn == ".");
    ZTH_EXPECT_EQ(root, std::size_t{13});
}
ZTH_TEST("works", "decode_name follows pointers, and pos ends up after the FIRST pointer, not after the name it led to") {
    // offset 12: example.com, offset 25: www + pointer to 12, offset 31: ftp + pointer to 25 (a chain), offset 37: a byte that must not be touched
    Bytes m = header(0, 0, 0, 0, 0, 0);
    append(m, from_hex("076578616d706c6503636f6d00"));
    append(m, from_hex("03777777c00c"));
    append(m, from_hex("03667470c019"));
    m.push_back(0xAA);
    std::size_t pos = 25;
    auto n = decode_name(sp(m), pos);
    ZTH_EXPECT_TRUE(n.has_value() && *n == "www.example.com");
    ZTH_EXPECT_EQ(pos, std::size_t{31});
    pos = 31;
    n = decode_name(sp(m), pos);
    ZTH_EXPECT_TRUE(n.has_value() && *n == "ftp.www.example.com");
    ZTH_EXPECT_EQ(pos, std::size_t{37});
    pos = 12;
    n = decode_name(sp(m), pos);
    ZTH_EXPECT_TRUE(n.has_value() && *n == "example.com" && pos == 25);
}
ZTH_TEST("works", "decode_name: a pointer on its own is a name, and names keep their case") {
    Bytes m = header(0, 0, 0, 0, 0, 0);
    append(m, from_hex("076578616d706c6503434f4d00"));   // example.COM
    append(m, from_hex("c00c"));
    std::size_t pos = 25;
    const auto n = decode_name(sp(m), pos);
    ZTH_EXPECT_TRUE(n.has_value() && *n == "example.COM");
    ZTH_EXPECT_EQ(pos, std::size_t{27});
}
ZTH_TEST("works", "decode_name leaves pos unchanged when it fails") {
    const Bytes m = from_hex("000000000000000000000000" "0377");   // truncated label
    std::size_t pos = 12;
    ZTH_EXPECT_FALSE(decode_name(sp(m), pos).has_value());
    ZTH_EXPECT_EQ(pos, std::size_t{12});
    const Bytes no_end = from_hex("000000000000000000000000" "03777777");   // a good label, then the packet ends: no terminator
    pos = 12;
    ZTH_EXPECT_FALSE(decode_name(sp(no_end), pos).has_value());
    ZTH_EXPECT_EQ(pos, std::size_t{12});
    // the bytes after the span are not part of the message, even if they would complete the label
    const Bytes longer = from_hex("000000000000000000000000" "03777777" "00");
    pos = 12;
    ZTH_EXPECT_FALSE(decode_name(std::span<const std::uint8_t>(longer.data(), 14), pos).has_value());   // cut inside the label "www"
    ZTH_EXPECT_EQ(pos, std::size_t{12});
}
ZTH_TEST("works", "serialize: a query is the header, the name and the type and class") {
    const auto w = serialize(query_msg());
    ZTH_EXPECT_TRUE(w.has_value());
    if (w) ZTH_EXPECT_EQ(to_hex(*w), std::string("beef0100000100000000000003777777076578616d706c6503636f6d0000010001"));
}
ZTH_TEST("works", "serialize compresses repeated suffixes (a response with CNAME, NS and glue)") {
    const auto w = serialize(response_msg());
    ZTH_EXPECT_TRUE(w.has_value());
    if (w) {
        ZTH_EXPECT_EQ(to_hex(*w), std::string("12348180000100020001000103777777076578616d706c6503636f6d0000010001c00c000500010000012c001103776562076578616d706c65036e657400c02d000100010000003c0004c0000201c03100020001000151800006036e7331c031c05a00010001000151800004c0000235"));
        ZTH_EXPECT_EQ(w->size(), std::size_t{112});
    }
}
ZTH_TEST("works", "compression is case-insensitive, the first spelling wins, and MX preferences are kept") {
    const auto w = serialize(mx_msg());
    ZTH_EXPECT_TRUE(w.has_value());
    if (w) ZTH_EXPECT_EQ(to_hex(*w), std::string("000784000001000200000000076578616d706c6503636f6d00000f0001c00c000f000100000e100009000a046d61696cc00cc00c000f000100000e10000a0014054d41494c32c00c"));
}
ZTH_TEST("works", "the root name: question, owner name and rdata target") {
    const auto w = serialize(root_msg());
    ZTH_EXPECT_TRUE(w.has_value());
    if (w) ZTH_EXPECT_EQ(to_hex(*w), std::string("000180000001000200000000000002000100000200010007e900001401610c726f6f742d73657276657273036e65740000000200010007e90000040162c01e"));
}
ZTH_TEST("works", "owner names are compressed against longer names written earlier (a.b.example.org then b.example.org)") {
    const auto w = serialize(txt_msg());
    ZTH_EXPECT_TRUE(w.has_value());
    if (w) ZTH_EXPECT_EQ(to_hex(*w), std::string("00098180000100020000000001610162076578616d706c65036f72670000100001c00c001000010000000a00060568656c6c6fc00e001000010000000a000403616263"));
}
ZTH_TEST("works", "a name written after offset 16383 cannot be pointed to, but it can still point back") {
    const auto w = serialize(late_msg());
    ZTH_EXPECT_TRUE(w.has_value());
    if (!w) return;
    ZTH_EXPECT_EQ(w->size(), std::size_t{17172});
    ZTH_EXPECT_EQ(to_hex(*w).substr(0, 96), std::string("000381800001000300000000076578616d706c6503636f6d000010000103626967c00c001000010000000142bdc87878"));
    const std::string hex = to_hex(*w);
    ZTH_EXPECT_EQ(hex.substr(hex.size() - 88), std::string("7878046c617465c00c0001000100000001000401020304046c617465c00c0001000100000001000405060708"));
}
ZTH_TEST("works", "parse reads every golden message back into the message it came from") {
    const std::vector<Message> all = {query_msg(), response_msg(), mx_msg(), root_msg(), txt_msg(), late_msg()};
    for (const Message& m : all) {
        const auto w = serialize(m);
        if (!w) {
            zth::fail("a golden message could not be serialized");
            return;
        }
        const auto back = parse(sp(*w));
        if (!back) {
            zth::fail("a serialized message could not be parsed (id " + std::to_string(m.id) + ")");
            return;
        }
        Message want = m;
        // names come back normalised: the root is ".", and compression keeps the FIRST spelling of a suffix
        for (Question& q : want.questions) if (q.name.empty()) q.name = ".";
        for (auto* sec : {&want.answers, &want.authorities, &want.additionals}) for (Record& r : *sec) if (r.name.empty()) r.name = ".";
        if (m.id == 7) {
            want.answers[1].name = "example.com";   // the whole name was a pointer to the first spelling
            want.answers[1].rdata = mx(20, "MAIL2.example.com");   // the suffix "Example.COM" was written first as "example.com"
        }
        if (!(*back == want)) {
            zth::fail("the message with id " + std::to_string(m.id) + " did not survive serialize + parse");
            return;
        }
    }
}
ZTH_TEST("works", "parse expands compression pointers inside rdata into uncompressed wire names") {
    const auto w = serialize(response_msg());
    if (!w) return;
    const auto m = parse(sp(*w));
    ZTH_EXPECT_TRUE(m.has_value());
    if (!m) return;
    ZTH_EXPECT_EQ(to_hex(m->answers[0].rdata), std::string("03776562076578616d706c65036e657400"));
    ZTH_EXPECT_EQ(to_hex(m->authorities[0].rdata), std::string("036e7331076578616d706c65036e657400"));
    ZTH_EXPECT_EQ(m->answers[1].rdata.size(), std::size_t{4});
}
ZTH_TEST("works", "header fields and counts") {
    Message m;
    m.id = 0xABCD;
    m.flags = 0x8583;
    const auto w = serialize(m);
    ZTH_EXPECT_TRUE(w.has_value());
    if (w) ZTH_EXPECT_EQ(to_hex(*w), std::string("abcd8583000000000000" "0000"));
    const auto back = parse(sp(*w));
    ZTH_EXPECT_TRUE(back.has_value() && back->id == 0xABCD && back->flags == 0x8583 && back->questions.empty());
}
ZTH_TEST("works", "serialize refuses what cannot be represented") {
    Message m = query_msg();
    m.questions[0].name = "bad..name";
    ZTH_EXPECT_FALSE(serialize(m).has_value());
    m = query_msg();
    m.questions[0].name = std::string(64, 'a') + ".com";
    ZTH_EXPECT_FALSE(serialize(m).has_value());
    m = response_msg();
    m.answers[1].rdata = {1, 2, 3};   // an A record needs 4 bytes
    ZTH_EXPECT_FALSE(serialize(m).has_value());
    m = response_msg();
    m.answers[0].rdata = {3, 'a', 'b'};   // a CNAME target that is cut short
    ZTH_EXPECT_FALSE(serialize(m).has_value());
    m = response_msg();
    m.answers[0].rdata = from_hex("0161c00c");   // a compressed target: rdata must be uncompressed
    ZTH_EXPECT_FALSE(serialize(m).has_value());
    m = response_msg();
    m.answers[0].rdata = from_hex("016100ff");   // trailing bytes after the name
    ZTH_EXPECT_FALSE(serialize(m).has_value());
    m = mx_msg();
    m.answers[0].rdata = {0, 10};   // an MX without a name
    ZTH_EXPECT_FALSE(serialize(m).has_value());
    m = txt_msg();
    m.answers[0].rdata.assign(70000, 'x');   // one rdata longer than 65535
    ZTH_EXPECT_FALSE(serialize(m).has_value());
    m = txt_msg();
    m.answers[0].rdata.assign(40000, 'x');   // each record fits, the message does not
    m.answers[1].rdata.assign(40000, 'y');
    ZTH_EXPECT_FALSE(serialize(m).has_value());
    m.answers[1].rdata.assign(10000, 'y');
    ZTH_EXPECT_TRUE(serialize(m).has_value());
    m = response_msg();
    m.additionals[0].type = 28;   // AAAA with 4 bytes
    ZTH_EXPECT_FALSE(serialize(m).has_value());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "malformed packets: pointer loops, forward pointers, pointers into the header") {
    struct Case {
        const char* what;
        Bytes packet;
    };
    Bytes q = header(1, 0, 1, 0, 0, 0);
    auto with = [&](const std::string& name_hex) {
        Bytes b = q;
        append(b, from_hex(name_hex));
        append(b, question_tail());
        return b;
    };
    std::string long_name_hex;
    for (int i = 0; i < 4; ++i) long_name_hex += "3f" + std::string(63 * 2, '6');
    long_name_hex += "00";
    const std::vector<Case> cases = {
        {"a pointer to itself", with("c00c")},
        {"two pointers pointing at each other", with("c00e" "c00c")},
        {"a pointer into the middle of its own name (a loop through a label)", with("0161c00c")},
        {"a forward pointer", with("c0ff")},
        {"a pointer to the very next byte", with("c00e" "00")},
        {"a pointer into the header", with("c000")},
        {"a pointer into the header, at its last byte", with("c00b")},
        {"a pointer to the end of the message", with("c01f")},
        {"a pointer cut in half", [&] { Bytes b = q; b.push_back(0xC0); return b; }()},
        {"label type 01 (0x41 would be a label of 65 bytes)", with("41" + std::string(65 * 2, '6') + "00")},
        {"label type 10 (0x81 would be a label of 129 bytes)", with("81" + std::string(129 * 2, '6') + "00")},
        {"a label longer than the packet", with("3f61" "00")},
        {"no terminator before the end", [&] { Bytes b = q; append(b, from_hex("03777777")); return b; }()},
        {"a control character in a label", with("0301" "7777" "00")},
        {"a dot inside a label", with("032e" "7777" "00")},
        {"a name of 257 bytes on the wire", with(long_name_hex)},
    };
    for (const Case& c : cases) {
        if (parse(sp(c.packet)).has_value()) {
            zth::fail(std::string("should be rejected: ") + c.what);
            return;
        }
    }
}
ZTH_TEST("robust", "a pointer that points FORWARD to a perfectly good name is still refused, even when the rest of the packet would be consistent") {
    Bytes b = header(1, 0, 2, 0, 0, 0);
    append(b, from_hex("c012" "00010001"));   // question 1: its name is a pointer to offset 18 (ahead of it)
    append(b, from_hex("016100" "00010001")); // offset 18: question 2, the name "a"
    ZTH_EXPECT_FALSE(parse(sp(b)).has_value());
    Bytes ok = header(1, 0, 2, 0, 0, 0);
    append(ok, from_hex("016100" "00010001"));   // question 1: "a"
    append(ok, from_hex("c00c" "00010001"));      // question 2: a pointer BACK to it
    const auto m = parse(sp(ok));
    ZTH_EXPECT_TRUE(m.has_value() && m->questions.size() == 2 && m->questions[1].name == "a");
}
ZTH_TEST("robust", "malformed packets: lengths, counts and trailing bytes") {
    const Bytes good = [] {
        Bytes b = *serialize(response_msg());
        return b;
    }();
    ZTH_EXPECT_TRUE(parse(sp(good)).has_value());
    // every truncation of a valid message is rejected, none crashes
    for (std::size_t len = 0; len < good.size(); ++len) {
        if (parse(std::span<const std::uint8_t>(good.data(), len)).has_value()) {
            zth::fail("a message cut at " + std::to_string(len) + " of " + std::to_string(good.size()) + " bytes was accepted");
            return;
        }
    }
    Bytes extra = good;
    extra.push_back(0);
    ZTH_EXPECT_FALSE(parse(sp(extra)).has_value());   // trailing garbage
    Bytes fewer = good;
    fewer[7] = 1;   // ancount 2 -> 1: the second answer is now trailing data
    ZTH_EXPECT_FALSE(parse(sp(fewer)).has_value());
    Bytes more = good;
    more[7] = 3;    // ancount 2 -> 3: a record that is not there
    ZTH_EXPECT_FALSE(parse(sp(more)).has_value());
}
ZTH_TEST("robust", "malformed packets: rdlength that lies") {
    auto rec = [&](const std::string& type_hex, const std::string& rdlen_hex, const std::string& rdata_hex) {
        Bytes b = header(1, 0, 0, 1, 0, 0);
        append(b, from_hex("016100"));
        append(b, from_hex(type_hex + "0001" "00000001" + rdlen_hex + rdata_hex));
        return b;
    };
    ZTH_EXPECT_TRUE(parse(sp(rec("0001", "0004", "01020304"))).has_value());
    ZTH_EXPECT_FALSE(parse(sp(rec("0001", "0005", "0102030405" "06"))).has_value());   // A with 5 bytes (and trailing)
    ZTH_EXPECT_FALSE(parse(sp(rec("0001", "0003", "010203"))).has_value());            // A with 3 bytes
    ZTH_EXPECT_FALSE(parse(sp(rec("0001", "0004", "010203"))).has_value());            // rdlength beyond the end
    ZTH_EXPECT_FALSE(parse(sp(rec("0001", "ffff", "01020304"))).has_value());
    ZTH_EXPECT_TRUE(parse(sp(rec("001c", "0010", "00112233445566778899aabbccddeeff"))).has_value());
    ZTH_EXPECT_FALSE(parse(sp(rec("001c", "000f", "00112233445566778899aabbccddee"))).has_value());   // AAAA with 15
    ZTH_EXPECT_TRUE(parse(sp(rec("0005", "0003", "016200"))).has_value());                // CNAME "b"
    ZTH_EXPECT_FALSE(parse(sp(rec("0005", "0002", "016200"))).has_value());               // rdlength shorter than the name
    ZTH_EXPECT_FALSE(parse(sp(rec("0005", "0004", "016200" "ff"))).has_value());          // rdlength longer than the name
    ZTH_EXPECT_FALSE(parse(sp(rec("0005", "0000", ""))).has_value());                     // no name at all
    ZTH_EXPECT_TRUE(parse(sp(rec("000f", "0005", "000a" "016200"))).has_value());         // MX 10 "b"
    ZTH_EXPECT_FALSE(parse(sp(rec("000f", "0002", "000a"))).has_value());                 // MX without a name
    ZTH_EXPECT_TRUE(parse(sp(rec("0010", "0004", "03616263"))).has_value());              // TXT is opaque
    ZTH_EXPECT_TRUE(parse(sp(rec("1234", "0000", ""))).has_value());                      // an unknown type with empty rdata
}
ZTH_TEST("robust", "a name that pointer-chains through 5000 pointers still decodes (no recursion, no loop)") {
    Bytes m = header(1, 0, 1, 0, 0, 0);
    append(m, from_hex("0161" "00"));   // offset 12: "a"
    std::size_t prev = 12;
    for (int i = 0; i < 5000; ++i) {
        const std::size_t here = m.size();
        m.push_back(static_cast<std::uint8_t>(0xC0 | (prev >> 8)));
        m.push_back(static_cast<std::uint8_t>(prev & 255));
        prev = here;
    }
    std::size_t pos = prev;
    const auto n = decode_name(sp(m), pos);
    ZTH_EXPECT_TRUE(n.has_value() && *n == "a");
    ZTH_EXPECT_EQ(pos, prev + 2);
}
ZTH_TEST("robust", "a name that would be 256 bytes long after following pointers is refused, one of 255 is fine") {
    const std::string l63(63, 'x');
    Message ok = query_msg();
    ok.questions[0].name = l63 + "." + l63 + "." + l63 + "." + std::string(61, 'y');
    ZTH_EXPECT_TRUE(serialize(ok).has_value());
    const auto w = serialize(ok);
    ZTH_EXPECT_TRUE(parse(sp(*w)).has_value());
    // 3 labels of 63 written plainly, then a pointer to a 62-byte tail: total 3 * 64 + 63 + ... > 255
    const std::size_t tail = 12;   // offset 12: a 62-byte label (wire 64), the question that follows points at it
    Bytes second;
    for (int i = 0; i < 3; ++i) {
        second.push_back(63);
        second.insert(second.end(), 63, 'z');
    }
    Bytes packet = header(1, 0, 2, 0, 0, 0);
    append(packet, from_hex("3e" + std::string(62 * 2, '7') + "00"));
    append(packet, question_tail());
    append(packet, second);   // 3 * 64 = 192 bytes of labels
    packet.push_back(0xC0);
    packet.push_back(static_cast<std::uint8_t>(tail));
    append(packet, question_tail());
    ZTH_EXPECT_FALSE(parse(sp(packet)).has_value());   // 192 + 64 + 1 = 257 > 255
}
ZTH_TEST("robust", "claimed counts are claims: 65535 of everything in a 12-byte packet fails fast without allocating for them") {
    Bytes b = header(1, 0, 65535, 65535, 65535, 65535);
    zth::mem::Scope scope;
    const auto m = parse(sp(b));
    ZTH_EXPECT_FALSE(m.has_value());
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 2);
    Bytes c = header(1, 0, 0, 40000, 0, 0);
    ZTH_EXPECT_FALSE(parse(sp(c)).has_value());
}
ZTH_TEST("robust", "random messages survive serialize + parse + serialize unchanged (the second serialization equals the first)") {
    zth::Lcg rng(2026);
    const char* labels[] = {"a", "bb", "www", "Example", "COM", "net", "mail", "x-y", "srv1", "org"};
    auto name = [&] {
        std::string n;
        const int count = rng.below(5);
        for (int i = 0; i < count; ++i) n += (i ? "." : "") + std::string(labels[rng.below(10)]);
        return n;
    };
    for (int round = 0; round < 400; ++round) {
        Message m;
        m.id = static_cast<std::uint16_t>(rng.next());
        m.flags = static_cast<std::uint16_t>(rng.next());
        const int nq = rng.below(3);
        for (int i = 0; i < nq; ++i) m.questions.push_back({name(), static_cast<std::uint16_t>(1 + rng.below(30)), 1});
        for (auto* sec : {&m.answers, &m.authorities, &m.additionals}) {
            const int n = rng.below(4);
            for (int i = 0; i < n; ++i) {
                Record r;
                r.name = name();
                r.cls = 1;
                r.ttl = static_cast<std::uint32_t>(rng.next());
                switch (rng.below(5)) {
                    case 0: r.type = 1; r.rdata = {1, 2, 3, static_cast<std::uint8_t>(rng.below(256))}; break;
                    case 1: r.type = 5; r.rdata = wire(name()); break;
                    case 2: r.type = 15; r.rdata = mx(static_cast<std::uint16_t>(rng.below(100)), name()); break;
                    case 3: r.type = 2; r.rdata = wire(name()); break;
                    default: r.type = 16; r.rdata.assign(static_cast<std::size_t>(rng.below(30)), 'q'); break;
                }
                sec->push_back(r);
            }
        }
        const auto w1 = serialize(m);
        if (!w1) {
            zth::fail("round " + std::to_string(round) + ": a valid message was refused");
            return;
        }
        const auto p = parse(sp(*w1));
        if (!p) {
            zth::fail("round " + std::to_string(round) + ": its own serialization was refused by parse");
            return;
        }
        const auto w2 = serialize(*p);
        if (!w2 || *w1 != *w2) {
            zth::fail("round " + std::to_string(round) + ": parse then serialize changed the bytes");
            return;
        }
    }
}
ZTH_TEST("robust", "garbage: 20000 random packets and 20000 mutated valid packets never crash or loop") {
    zth::Lcg rng(55);
    const Bytes good = *serialize(response_msg());
    for (int round = 0; round < 20000; ++round) {
        Bytes b;
        if (round % 2 == 0) {
            b.resize(static_cast<std::size_t>(12 + rng.below(60)));
            for (auto& x : b) x = static_cast<std::uint8_t>(rng.below(4) == 0 ? 0xC0 | rng.below(4) : rng.below(256));
            b[4] = b[6] = b[8] = b[10] = 0;
            b[5] = static_cast<std::uint8_t>(rng.below(3));
            b[7] = static_cast<std::uint8_t>(rng.below(3));
        } else {
            b = good;
            for (int k = 0; k < 1 + rng.below(3); ++k) b[static_cast<std::size_t>(rng.below(static_cast<int>(b.size())))] = static_cast<std::uint8_t>(rng.below(256));
        }
        const auto m = parse(sp(b));
        if (m) {
            const auto again = serialize(*m);
            if (!again) {
                zth::fail("a parsed message could not be serialized again (round " + std::to_string(round) + ")");
                return;
            }
        }
    }
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "a message with 3000 answers (about 60 KB) is serialized and parsed in linear time", 20) {
    Message m;
    m.id = 1;
    m.questions.push_back({"example.com", 1, 1});
    for (int i = 0; i < 3000; ++i) m.answers.push_back({"host" + std::to_string(i % 500) + ".example.com", 1, 1, 60, {10, 0, static_cast<std::uint8_t>(i >> 8), static_cast<std::uint8_t>(i)}});
    for (int round = 0; round < 20; ++round) {
        const auto w = serialize(m);
        ZTH_EXPECT_TRUE(w.has_value() && w->size() < 65535);
        const auto p = parse(sp(*w));
        ZTH_EXPECT_TRUE(p.has_value() && p->answers.size() == 3000);
    }
}
ZTH_TEST_TIMEOUT("fast", "decoding a deep pointer chain 8000 times is fast: each name costs the length of its chain, not more", 20) {
    Bytes m = header(0, 0, 0, 0, 0, 0);
    append(m, from_hex("0161" "00"));
    std::size_t prev = 12;
    for (int i = 0; i < 3000; ++i) {
        const std::size_t here = m.size();
        m.push_back(static_cast<std::uint8_t>(0xC0 | (prev >> 8)));
        m.push_back(static_cast<std::uint8_t>(prev & 255));
        prev = here;
    }
    for (int i = 0; i < 300; ++i) {
        std::size_t pos = prev;
        const auto n = decode_name(sp(m), pos);
        if (!n || *n != "a") {
            zth::fail("the chain did not decode");
            return;
        }
    }
}
ZTH_TEST_TIMEOUT("fast", "compression lookups are hashed: 2000 distinct owner names under one zone", 20) {
    Message m;
    m.id = 1;
    m.questions.push_back({"zone.example", 255, 1});
    for (int i = 0; i < 2000; ++i) m.answers.push_back({"n" + std::to_string(i) + ".sub" + std::to_string(i % 40) + ".zone.example", 16, 1, 1, {1, 'x'}});
    for (int round = 0; round < 10; ++round) {
        const auto w = serialize(m);
        ZTH_EXPECT_TRUE(w.has_value());
    }
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("dns.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("dns.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "byte order by shifts: no memcpy, no pointer casts of integers, no htons/ntohs, no unions") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("dns.hpp"));
    for (const char* word : {"memcpy", "memmove", "bit_cast", "union", "htons", "htonl", "ntohs", "ntohl", "byteswap", "stoi", "stoul", "sscanf", "regex"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "every read is bounds-checked through std::span, not raw pointers") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("dns.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "span").empty());
}
