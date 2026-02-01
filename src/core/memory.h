#ifndef MCGNG_MEMORY_H
#define MCGNG_MEMORY_H

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

namespace mcgng {

/**
 * Simple memory pool for efficient allocation of small objects.
 */
class MemoryPool {
public:
    explicit MemoryPool(size_t blockSize, size_t numBlocks = 256);
    ~MemoryPool();

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    void* allocate();
    void deallocate(void* ptr);

    size_t getBlockSize() const { return m_blockSize; }
    size_t getNumAllocated() const { return m_numAllocated; }
    size_t getCapacity() const { return m_blocks.size(); }

private:
    size_t m_blockSize;
    size_t m_numAllocated = 0;
    std::vector<uint8_t*> m_blocks;
    std::vector<void*> m_freeList;
    std::vector<std::unique_ptr<uint8_t[]>> m_memory;

    void grow();
};

/**
 * Memory statistics and tracking.
 */
struct MemoryStats {
    size_t totalAllocated = 0;
    size_t totalFreed = 0;
    size_t currentUsage = 0;
    size_t peakUsage = 0;
    size_t allocationCount = 0;
};

/**
 * Memory management system.
 */
class MemoryManager {
public:
    static MemoryManager& instance();

    void* allocate(size_t size, const char* tag = nullptr);
    void deallocate(void* ptr);

    const MemoryStats& getStats() const { return m_stats; }
    void resetStats();

    // Debug helpers
    void enableTracking(bool enable) { m_trackingEnabled = enable; }
    void dumpAllocations() const;

private:
    MemoryManager() = default;
    ~MemoryManager() = default;

    MemoryStats m_stats;
    bool m_trackingEnabled = false;
};

/**
 * RAII wrapper for temporary memory allocations.
 */
template<typename T>
class TempBuffer {
public:
    explicit TempBuffer(size_t count)
        : m_data(std::make_unique<T[]>(count))
        , m_size(count) {}

    T* data() { return m_data.get(); }
    const T* data() const { return m_data.get(); }
    size_t size() const { return m_size; }

    T& operator[](size_t index) { return m_data[index]; }
    const T& operator[](size_t index) const { return m_data[index]; }

private:
    std::unique_ptr<T[]> m_data;
    size_t m_size;
};

} // namespace mcgng

#endif // MCGNG_MEMORY_H
