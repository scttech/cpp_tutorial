# Binary Protocol Codec

> **You're an engineer on the networking team.** Text protocols (HTTP, JSON) are easy to read and slow to parse; binary protocols (Protocol Buffers, gRPC, Kafka, Redis RESP3 bulk lengths, MQTT, TLS, game netcode) are compact and fast but unforgiving: every byte has a position and a meaning, every length comes from the peer, and the receiver must never trust one. You will build the three layers that nearly every binary protocol has: fixed-size integers in a defined byte order, variable-length integers, and checksummed length-prefixed frames decoded from a stream.

## Byte order

An integer in memory has the *host's* byte order (little-endian on x86 and ARM-in-practice). On the wire a protocol fixes one: "network byte order" is **big-endian** (most significant byte first), many modern formats are little-endian. Write and read integers with **shifts** (`(v >> 8*i) & 0xFF`), never by casting a pointer or `memcpy`ing the integer: shifts mean the same thing on every host, avoid unaligned access, and avoid strict-aliasing undefined behaviour. (C++23 has `std::byteswap`; C++20 `std::endian` tells you the host order.)

## Varints (LEB128)

Most numbers are small. A varint stores 7 bits per byte, least significant group first, and sets the top bit of every byte except the last: `300` is `AC 02` (`0b0101100` with the continuation bit, then `0b10`). 0 to 127 take one byte, a 64-bit maximum takes 10. Protobuf, WebAssembly, DWARF and Git use it. Negative numbers would always need 10 bytes, so signed values are **zigzag** encoded first: `(n << 1) ^ (n >> 63)` maps 0, -1, 1, -2, 2 ... to 0, 1, 2, 3, 4 ...

**Decoding is where the bugs are.** The reader of untrusted bytes must refuse:

- a varint longer than 10 bytes, or whose 10th byte is more than 1 (it would overflow 64 bits silently);
- a **non-canonical** encoding such as `80 00` for 0 (several encodings of one value let an attacker slip past checks that compare encoded bytes);
- a truncated varint (the continuation bit set on the last available byte).

## Readers that fail cleanly

Every getter returns `std::optional`, and a failed read **leaves the position where it was**, so the caller can wait for more data and retry. A string is `varint length` + bytes: check `length <= remaining` **before** doing anything with it. A length of `0xFFFFFFFFFFFFFFFF` must not reach `new`, `resize` or `reserve`. The views (`std::span`, `std::string_view`) point into the input: nothing is copied.

## Frames from a stream

`version | type | length (varint) | payload | checksum`, checksum = XOR of everything before it, seeded with `0x5A` (a toy; real protocols use CRC-32 or an AEAD tag). The decoder is a **byte-wise state machine**, so any split of the stream gives the same frames. It validates as early as possible: the version at byte 0, the length when its last byte arrives (**before storing any payload**, so a hostile length cannot make the decoder allocate gigabytes), the checksum at the end. After an error it stops consuming.

## The tiers

- **Works**: byte-exact goldens for every integer width and byte order, ten well-known varints, twelve zigzag values (including `INT64_MIN`), reading back, truncation, strict varints, strings with lying lengths, zero-copy views, frame encoding and decoding byte by byte, errors, `reset`.
- **Robust**: 20000 random round-trips, minimal varint length for every bit count, garbage never moves a failed read, 40 re-slicings of a 40-frame stream, every single-bit flip of a frame is detected.
- **Fast**: reading allocates nothing, writing after `reserve` allocates nothing, a 4 GB length is refused with no payload stored, 20 MB of frames in linear time.
- **Clean**: no raw memory or exceptions; no `memcpy`/`bit_cast`/unions/`htonl`/`byteswap` (shifts only); `span` and `string_view`.
