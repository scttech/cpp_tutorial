#pragma once
// YOUR RING-BUFFER LOGGER: a flight recorder with a FIXED memory budget. It keeps the most recent log records in one flat byte buffer
// and overwrites the oldest ones when it is full. Logging must never allocate.
//
//   RingLogger(capacity_bytes)       allocates the buffer once, here and nowhere else
//   set_min_level(l) / min_level()   logs below it are FILTERED (default: Debug, nothing is filtered)
//   bool log(level, message)         true if the record was stored
//   size()                           records currently stored          bytes_used()   bytes they occupy (headers included)
//   capacity()                       bytes in the buffer
//   total_logged()                   logs ACCEPTED (not filtered) so far: this is also the last sequence number
//   evicted()                        records overwritten to make room  truncated()    records whose message was cut to fit
//   filtered()                       logs refused by the level         lost()         accepted logs that could not be stored at all
//   snapshot()                       the stored records, OLDEST first, as Entry{seq, level, message}
//   dump()                           one line per record, oldest first:   #<seq> <LEVEL> <message>\n    (level names: see types.hpp)
//   clear()                          forgets the stored records; the counters and the sequence numbers keep going
//
// RECORDS are laid end to end in the buffer, each one   [level: 1 byte][seq: 8 bytes, little endian][length: 4 bytes, little endian][message bytes]
// so a record takes kHeaderBytes (= 13) + message length bytes. The buffer is CIRCULAR: a record (even its 13-byte header!) may start near
// the end and continue at the beginning.
//
//  log(): 1. below min_level -> filtered++, return false (no sequence number is used).  2. seq = ++total_logged.
//         3. capacity <= kHeaderBytes: nothing fits -> lost++, return false.   4. if 13 + length > capacity, cut the message to capacity - 13 bytes and truncated++.
//         5. while the free space is less than the record, drop the OLDEST record (evicted++).  6. write the record at (head + used) % capacity.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace flightlog {

class RingLogger {
public:
    static constexpr std::size_t kHeaderBytes = 13;

    explicit RingLogger(std::size_t capacity_bytes) : capacity_(capacity_bytes) {}

    void set_min_level(Level level) { (void)level; }  // TODO
    Level min_level() const { return Level::Debug; }  // TODO

    bool log(Level level, std::string_view message) {
        (void)level;
        (void)message;
        return false;  // TODO
    }

    std::size_t size() const { return 0; }        // TODO
    std::size_t bytes_used() const { return 0; }  // TODO
    std::size_t capacity() const { return capacity_; }
    std::uint64_t total_logged() const { return 0; }  // TODO
    std::uint64_t evicted() const { return 0; }       // TODO
    std::uint64_t truncated() const { return 0; }     // TODO
    std::uint64_t filtered() const { return 0; }      // TODO
    std::uint64_t lost() const { return 0; }          // TODO

    std::vector<Entry> snapshot() const { return {}; }  // TODO
    std::string dump() const { return ""; }             // TODO
    void clear() {}                                     // TODO

private:
    std::size_t capacity_;
    // TODO: the byte buffer (std::vector<char> sized once), the offset of the oldest record (head), the bytes in use, the record count, the counters
};

}  // namespace flightlog
