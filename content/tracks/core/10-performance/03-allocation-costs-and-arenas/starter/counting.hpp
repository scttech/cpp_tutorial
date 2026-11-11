#pragma once
// READ-ONLY. A memory_resource that hands out real memory and COUNTS what it is asked to do. It plays "the heap" for your Arena: every
// call that reaches it is one trip to the heap, which is the thing an arena exists to avoid.
//   allocations / deallocations   calls so far          live()   blocks handed out and not given back
//   bad_frees                     deallocate() of a pointer that was never allocated by this resource (a bug in the caller)
// Its blocks start 16 bytes past a 64-byte boundary, as real heap blocks often do, so code that forgets to align the ADDRESS is caught.
#include <cstddef>
#include <map>
#include <memory_resource>
#include <new>

class CountingResource : public std::pmr::memory_resource {
public:
    std::size_t allocations = 0;
    std::size_t deallocations = 0;
    std::size_t bad_frees = 0;
    std::size_t live() const { return blocks_.size(); }
    ~CountingResource() override {
        for (auto& entry : blocks_) ::operator delete(entry.second, std::align_val_t{64});
    }

protected:
    void* do_allocate(std::size_t size, std::size_t align) override {
        ++allocations;
        if (size > (std::size_t{1} << 30)) return nullptr;  // an absurd request is refused (no exceptions needed)
        void* raw = ::operator new(size + 128, std::align_val_t{64});
        char* p = static_cast<char*>(raw) + (align <= 16 ? 16 : 0);
        blocks_[p] = raw;
        return p;
    }
    void do_deallocate(void* p, std::size_t, std::size_t) override {
        auto it = blocks_.find(p);
        if (it == blocks_.end()) {
            ++bad_frees;
            return;
        }
        ++deallocations;
        ::operator delete(it->second, std::align_val_t{64});
        blocks_.erase(it);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }

private:
    std::map<void*, void*> blocks_;  // pointer handed out -> pointer from operator new
};
