#pragma once
#include <cstdlib>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// DoublyLinkedList<T>
// A generic doubly-linked list built with raw pointers — NO STL.
// Used by:
//   • LRU Cache  (O(1) move-to-front and tail eviction)
//   • Hash map chaining (collision resolution)
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
struct DLLNode {
    T        data;
    DLLNode* prev;
    DLLNode* next;

    explicit DLLNode(const T& d) : data(d), prev(nullptr), next(nullptr) {}
};

template <typename T>
class DoublyLinkedList {
public:
    DLLNode<T>* head;
    DLLNode<T>* tail;
    int         size;

    DoublyLinkedList() : head(nullptr), tail(nullptr), size(0) {}

    ~DoublyLinkedList() {
        DLLNode<T>* cur = head;
        while (cur) {
            DLLNode<T>* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
    }

    // Insert at front — O(1)
    DLLNode<T>* pushFront(const T& val) {
        DLLNode<T>* node = new DLLNode<T>(val);
        node->next = head;
        node->prev = nullptr;
        if (head) head->prev = node;
        head = node;
        if (!tail) tail = node;
        size++;
        return node;
    }

    // Remove specific node — O(1) because we have the pointer
    void remove(DLLNode<T>* node) {
        if (!node) return;
        if (node->prev) node->prev->next = node->next;
        else            head = node->next;
        if (node->next) node->next->prev = node->prev;
        else            tail = node->prev;
        delete node;
        size--;
    }

    // Move existing node to front — O(1)
    void moveToFront(DLLNode<T>* node) {
        if (node == head) return;
        // Detach
        if (node->prev) node->prev->next = node->next;
        if (node->next) node->next->prev = node->prev;
        if (node == tail) tail = node->prev;
        // Re-attach at front
        node->prev = nullptr;
        node->next = head;
        if (head) head->prev = node;
        head = node;
        if (!tail) tail = node;
    }

    // Remove and return tail data (for LRU eviction) — O(1)
    bool popTail(T& out) {
        if (!tail) return false;
        out = tail->data;
        DLLNode<T>* old = tail;
        tail = tail->prev;
        if (tail) tail->next = nullptr;
        else      head = nullptr;
        delete old;
        size--;
        return true;
    }

    bool isEmpty() const { return size == 0; }
};
