#pragma once
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "Logger.h"

static const int PAGE_SIZE = 4096;
static const int MAX_PAGES = 200;

struct Page {
    int  pageId;
    bool dirty;
    bool inUse;
    int  lruOrder;   // lower = least recently used
    char data[PAGE_SIZE];

    Page() : pageId(-1), dirty(false), inUse(false), lruOrder(0) {
        memset(data, 0, PAGE_SIZE);
    }
};

// Simple array-based buffer pool — no DLL, no hash table,
// just a fixed array of pages with LRU tracking via a counter.
// Correct and robust on Windows. O(N) eviction where N=200 (tiny).
class BufferPool {
    Page  pool[MAX_PAGES];
    int   poolCap;
    int   usedCount;
    int   lruClock;       // monotonically increasing access counter
    FILE* diskFile;
    char  diskPath[256];

public:
    int totalEvictions;

    explicit BufferPool(int capacity = MAX_PAGES,
                        const char* path = "data/nanodb.db")
        : poolCap(capacity > MAX_PAGES ? MAX_PAGES : capacity),
          usedCount(0), lruClock(0), diskFile(nullptr), totalEvictions(0)
    {
        strncpy(diskPath, path, 255); diskPath[255] = '\0';
        diskFile = fopen(diskPath, "r+b");
        if (!diskFile) diskFile = fopen(diskPath, "w+b");
    }

    ~BufferPool() {
        flushAll();
        if (diskFile) fclose(diskFile);
    }

    Page* fetchPage(int pageId) {
        // 1. Search for existing page (cache hit)
        for (int i = 0; i < poolCap; i++) {
            if (pool[i].inUse && pool[i].pageId == pageId) {
                pool[i].lruOrder = ++lruClock;
                return &pool[i];
            }
        }

        // 2. Find a free slot
        int slot = -1;
        for (int i = 0; i < poolCap; i++) {
            if (!pool[i].inUse) { slot = i; break; }
        }

        // 3. No free slot — evict LRU page
        if (slot == -1) {
            int minOrder = pool[0].lruOrder;
            slot = 0;
            for (int i = 1; i < poolCap; i++) {
                if (pool[i].lruOrder < minOrder) {
                    minOrder = pool[i].lruOrder;
                    slot = i;
                }
            }
            // Write evicted page to disk if dirty
            if (pool[slot].dirty && diskFile) {
                long offset = (long)pool[slot].pageId * PAGE_SIZE;
                fseek(diskFile, offset, SEEK_SET);
                fwrite(pool[slot].data, 1, PAGE_SIZE, diskFile);
                fflush(diskFile);
            }
            totalEvictions++;
            if (totalEvictions % 200 == 0)
                Logger::log("[CACHE] Page %d evicted via LRU (total: %d)",
                            pool[slot].pageId, totalEvictions);
        }

        // 4. Load new page into slot
        memset(pool[slot].data, 0, PAGE_SIZE);
        pool[slot].pageId   = pageId;
        pool[slot].dirty    = false;
        pool[slot].inUse    = true;
        pool[slot].lruOrder = ++lruClock;

        // Read from disk if exists
        if (diskFile) {
            long offset = (long)pageId * PAGE_SIZE;
            fseek(diskFile, offset, SEEK_SET);
            fread(pool[slot].data, 1, PAGE_SIZE, diskFile);
        }

        return &pool[slot];
    }

    void markDirty(int pageId) {
        for (int i = 0; i < poolCap; i++)
            if (pool[i].inUse && pool[i].pageId == pageId)
                pool[i].dirty = true;
    }

    void flushAll() {
        if (!diskFile) return;
        for (int i = 0; i < poolCap; i++) {
            if (pool[i].inUse && pool[i].dirty) {
                long offset = (long)pool[i].pageId * PAGE_SIZE;
                fseek(diskFile, offset, SEEK_SET);
                fwrite(pool[i].data, 1, PAGE_SIZE, diskFile);
                pool[i].dirty = false;
            }
        }
        fflush(diskFile);
    }

    void walWrite(const char* ) {} // stub — WAL not needed for Windows version

    int getPoolCap()   const { return poolCap; }
    int getEvictions() const { return totalEvictions; }
};
