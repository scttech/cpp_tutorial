# Ring-Buffer Logger

> **You're an engineer on a systems team.** The service crashes once a week and the logs written to disk do not say why: the interesting lines were buffered and lost, or the disk filled and logging was switched off. Production systems from game engines to flight software use a **flight recorder**: the last N kilobytes of log messages kept *in memory*, in a fixed-size ring, cheap enough to leave on all the time and dumped when something goes wrong. Logging in the hot path must not allocate, must not shift data around, and must never use more memory than its budget.

## The design: a circle of bytes

One `std::vector<char>` allocated once. Records are laid end to end:

```
[level: 1][seq: 8][length: 4][message: length bytes]       (13-byte header)
```

State: `head_` (offset of the **oldest** record), `used_` (bytes in use), `count_` (records). Everything else follows:

- **Where to write:** `(head_ + used_) % capacity`.
- **Making room:** while `capacity - used_ < needed`, **drop the oldest** record: read its header at `head_` to learn its length, then `head_ = (head_ + size) % capacity`, `used_ -= size`.
- **Wrap-around:** a record can start 5 bytes before the end of the buffer and continue at offset 0, **even in the middle of its 13-byte header**. Do not special-case the "fits / doesn't fit" situation: write two small helpers, `write(pos, data, n)` and `read(pos, out, n)`, that copy in at most two pieces (up to the end, then from the beginning) and let everything else use them.
- **Encode integers byte by byte** (`(seq >> (8 * i)) & 0xFF`): no alignment problems, no endianness assumptions, no casts of `char*` to `uint64_t*`.

## The policy

| Situation | Outcome |
|---|---|
| level below `min_level` | `filtered++`, return `false`; **no sequence number is used** |
| accepted | `seq = ++total_logged` |
| buffer ≤ 13 bytes | cannot hold even an empty record: `lost++`, return `false` (the sequence number is still used) |
| record larger than the whole buffer | message **truncated** to `capacity - 13` bytes, `truncated++` (and it evicts everything else) |
| not enough free space | evict oldest records **one at a time, only as many as needed** (`evicted++` each) |

## The tiers

- **Works**: ordering, levels, sequence numbers, empty and binary messages, the filter, eviction of exactly the right number of records, exact fit, truncation, tiny buffers, `dump()` format, `clear()`.
- **Robust**: records straddling the end of the buffer at every offset, 60000 random operations on ten buffer sizes against a `std::deque` model (compared after every third step: contents, `bytes_used` and all seven counters), 200 KB messages (the length field really has 4 bytes), 70000 sequence numbers (the sequence field really has 8).
- **Fast**: construction allocates once; **100000 logs allocate nothing** (even for messages longer than the small-string buffer); 2 million logs in a 64 KiB buffer inside the timeout (a compaction or `memmove` per log would take minutes); memory never exceeds the capacity.
- **Clean**: no raw memory or exceptions, a single flat buffer (no `deque`/`list`/`map`, no `unique_ptr` per record, no `rotate`/`memmove`), and `string_view` for the input.

## Tips

Write `write` and `read` first and test them mentally with `pos = size - 3`, `n = 10`. `std::min(n, size - pos)` is the length of the first piece. `drop_oldest` must read the header *through `read`* because the header itself may wrap.
