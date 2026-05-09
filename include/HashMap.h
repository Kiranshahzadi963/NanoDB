#pragma once
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "DoublyLinkedList.h"

// ─────────────────────────────────────────────────────────────────────────────
// HashMap<V>  — open addressing with chaining via linked list buckets.
// Key   : C-string (char[])
// Value : template type V
// Achieves O(1) average lookup, O(N/k) worst-case with chaining.
// Used by the System Catalog to store table name → metadata mappings.
// ─────────────────────────────────────────────────────────────────────────────

static const int KEY_LEN = 64;

template <typename V>
struct KVPair {
    char key[KEY_LEN];
    V    value;

    KVPair() { key[0] = '\0'; }
    KVPair(const char* k, const V& v) : value(v) {
        strncpy(key, k, KEY_LEN - 1);
        key[KEY_LEN - 1] = '\0';
    }
};

template <typename V>
class HashMap {
    static const int BUCKET_COUNT = 64;

    // Each bucket is a singly-linked chain of KVPairs
    struct ChainNode {
        KVPair<V>  kv;
        ChainNode* next;
        ChainNode(const char* k, const V& v) : next(nullptr), kv(k, v) {}
    };

    ChainNode* buckets[BUCKET_COUNT];
    int        count;

    // djb2 hash function — fast and good distribution
    int hash(const char* key) const {
        unsigned long h = 5381;
        int c;
        while ((c = *key++)) h = ((h << 5) + h) + c;
        return (int)(h % BUCKET_COUNT);
    }

public:
    HashMap() : count(0) {
        for (int i = 0; i < BUCKET_COUNT; i++) buckets[i] = nullptr;
    }

    ~HashMap() {
        for (int i = 0; i < BUCKET_COUNT; i++) {
            ChainNode* cur = buckets[i];
            while (cur) {
                ChainNode* nxt = cur->next;
                delete cur;
                cur = nxt;
            }
        }
    }

    // Insert or update — O(1) amortized
    void put(const char* key, const V& value) {
        int idx = hash(key);
        ChainNode* cur = buckets[idx];
        while (cur) {
            if (strcmp(cur->kv.key, key) == 0) {
                cur->kv.value = value;
                return;
            }
            cur = cur->next;
        }
        // Not found — prepend new node
        ChainNode* node = new ChainNode(key, value);
        node->next   = buckets[idx];
        buckets[idx] = node;
        count++;
    }

    // Lookup — O(1) amortized; returns nullptr if not found
    V* get(const char* key) {
        int idx = hash(key);
        ChainNode* cur = buckets[idx];
        while (cur) {
            if (strcmp(cur->kv.key, key) == 0) return &cur->kv.value;
            cur = cur->next;
        }
        return nullptr;
    }

    bool contains(const char* key) { return get(key) != nullptr; }

    void remove(const char* key) {
        int idx = hash(key);
        ChainNode* cur  = buckets[idx];
        ChainNode* prev = nullptr;
        while (cur) {
            if (strcmp(cur->kv.key, key) == 0) {
                if (prev) prev->next  = cur->next;
                else      buckets[idx] = cur->next;
                delete cur;
                count--;
                return;
            }
            prev = cur; cur = cur->next;
        }
    }

    int size() const { return count; }

    // Iterate all entries — calls func(key, value) for each
    template <typename Func>
    void forEach(Func func) {
        for (int i = 0; i < BUCKET_COUNT; i++) {
            ChainNode* cur = buckets[i];
            while (cur) {
                func(cur->kv.key, cur->kv.value);
                cur = cur->next;
            }
        }
    }
};
