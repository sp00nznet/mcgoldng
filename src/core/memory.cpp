#include "core/memory.h"
#include <algorithm>
#include <iostream>
#include <cstdlib>

namespace mcgng {

// MemoryPool implementation

MemoryPool::MemoryPool(size_t blockSize, size_t numBlocks)
    : m_blockSize(std::max(blockSize, sizeof(void*))) {
    m_blocks.reserve(numBlocks);
    m_freeList.reserve(numBlocks);
    grow();
}

MemoryPool::~MemoryPool() {
    // Memory is automatically freed via unique_ptr
}

void MemoryPool::grow() {
    // Allocate a chunk of memory
    size_t chunkSize = m_blockSize * 64;  // 64 blocks per chunk
    auto chunk = std::make_unique<uint8_t[]>(chunkSize);
    uint8_t* ptr = chunk.get();

    // Add all blocks to free list
    for (size_t i = 0; i < 64; ++i) {
        m_freeList.push_back(ptr + (i * m_blockSize));
    }

    m_memory.push_back(std::move(chunk));
}

void* MemoryPool::allocate() {
    if (m_freeList.empty()) {
        grow();
    }

    void* ptr = m_freeList.back();
    m_freeList.pop_back();
    ++m_numAllocated;
    return ptr;
}

void MemoryPool::deallocate(void* ptr) {
    if (ptr) {
        m_freeList.push_back(ptr);
        --m_numAllocated;
    }
}

// MemoryManager implementation

MemoryManager& MemoryManager::instance() {
    static MemoryManager instance;
    return instance;
}

void* MemoryManager::allocate(size_t size, const char* /*tag*/) {
    void* ptr = std::malloc(size);

    if (ptr) {
        m_stats.totalAllocated += size;
        m_stats.currentUsage += size;
        m_stats.allocationCount++;

        if (m_stats.currentUsage > m_stats.peakUsage) {
            m_stats.peakUsage = m_stats.currentUsage;
        }
    }

    return ptr;
}

void MemoryManager::deallocate(void* ptr) {
    if (ptr) {
        std::free(ptr);
        // Note: We can't track exact size freed without additional bookkeeping
    }
}

void MemoryManager::resetStats() {
    m_stats = MemoryStats{};
}

void MemoryManager::dumpAllocations() const {
    std::cout << "Memory Stats:\n";
    std::cout << "  Total Allocated: " << m_stats.totalAllocated << " bytes\n";
    std::cout << "  Current Usage:   " << m_stats.currentUsage << " bytes\n";
    std::cout << "  Peak Usage:      " << m_stats.peakUsage << " bytes\n";
    std::cout << "  Allocation Count: " << m_stats.allocationCount << "\n";
}

} // namespace mcgng
