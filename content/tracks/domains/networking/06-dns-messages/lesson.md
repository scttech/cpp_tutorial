# DNS Messages and Name Compression

> **You're an engineer on the networking team** writing a DNS resolver. Every DNS packet arrives over UDP from the open internet, so every byte of it is hostile until proven otherwise; and the format has one famously tricky feature, **name compression**, where a name can be "the rest is at offset 12". Compression pointers were behind real vulnerabilities in BIND, dnsmasq, glibc's resolver, systemd-resolved and many others: infinite loops, reads past the end, and names that expand to megabytes. You will write the codec so that none of those can happen.

## The wire format in one page

A message is a 12-byte header (id, flags, and four section counts, all big-endian 16-bit), followed by the questions (name, type, class), then answer, authority and additional **resource records** (name, type, class, 32-bit ttl, 16-bit rdlength, rdata).

A **name** is a list of labels, each a length byte (1 to 63) followed by that many bytes, ended by a zero byte: `www.example.com` is `03 77 77 77 07 65 78 61 6d 70 6c 65 03 63 6f 6d 00`. The root name is the single byte `00`. A whole name is at most 255 bytes.

## Compression pointers

If a length byte has its top two bits set (`11`), it and the next byte form a **pointer**: the other 14 bits are an offset from the start of the message where the rest of the name continues. So `web.example.net` appearing after `www.example.com` can be `03 77 65 62 c0 0c`, a label followed by "continue at offset 12". Pointers can lead to names that themselves end in pointers. The top bits `01` and `10` are reserved and must be refused.

## Reading names without being tricked

- **A pointer must point backwards, never into the header, and every jump of one name must land strictly before the previous one** (before the name's start for the first jump). Then loops are *impossible*: the offsets form a strictly decreasing sequence. A rule like "at most 100 jumps" is weaker (100 jumps through a 255-byte name can still be slow) and "target < the pointer's own position" is **not enough**: a pointer into the middle of its own label sequence (`01 61 c0 0c`) loops forever.
- **Use a loop, not recursion**: a chain of thousands of pointers is a stack overflow for a recursive decoder.
- **Count the expanded length** while reading: a handful of pointers can expand into a name far beyond 255 bytes.
- **`pos` after a name is after its *first* pointer** (or its zero byte), not after the name the pointer led to. That is the number one bug in hand-written decoders, because the *next field* starts right after the pointer.
- Check every length against the end of the packet before reading, labels must be printable (so the text form is unambiguous), and a failed decode leaves `pos` untouched.
- **Counts are claims.** `qdcount = 65535` in a 12-byte packet: do not `reserve(65535)` records. Read element by element and fail at the first missing byte.
- `rdlength` must fit in the packet, and for A / AAAA / names it must match the content exactly (`CNAME` whose name ends before or after `rdlength` is malformed). Nothing may follow the last record.

## Writing names with compression

While serializing keep a table `suffix -> offset` of every suffix of every name written so far (`example.com` and `com` for `www.example.com`), keyed **case-insensitively** (DNS names compare ignoring case; the first spelling wins). To write a name, walk its suffixes from the longest: the first one found in the table becomes a **pointer**; otherwise write the label and register the suffix. Two limits: a pointer has only 14 bits, so a name starting at offset 16384 or more can **never be the target** of a pointer (do not register it, or you would emit a corrupt pointer); and the final message may not exceed 65535 bytes. Names inside the rdata of NS, CNAME, PTR and MX are compressed too; `rdlength` is written last.

This exercise keeps rdata of those types in **uncompressed** form in memory (`parse` expands it, `serialize` compresses it), so a `Record` means the same wherever it came from.

## The tiers

- **Works**: name encoding limits (63 per label, 255 total), decoding with pointers and chains, the position after a pointer, six golden messages checked **byte for byte** against an independent model (query, response with CNAME/NS/glue, MX with case-insensitive reuse, the root, shared suffixes, and offsets above 16383), parse back, rdata expansion, everything `serialize` must refuse.
- **Robust**: 16 hostile name layouts, every truncation of a valid message, wrong counts, 14 lying `rdlength`s, a 5000-pointer chain, the 255/257-byte boundary, 65535 claimed records in 12 bytes, 400 random messages that must round-trip bit-exactly, 40000 random and mutated packets.
- **Fast**: 3000-answer messages, a deep chain decoded 300 times, 2000 distinct names compressed.
- **Clean**: no raw memory or exceptions; no `memcpy`, no `htons`, no pointer casts; `std::span` for every read.
