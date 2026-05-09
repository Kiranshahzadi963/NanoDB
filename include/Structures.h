#pragma once
#include <cstdlib>
#include <cstring>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// CustomStack<T>  — raw array-based stack, no STL
// Used by the Query Parser for Infix → Postfix conversion
// and expression evaluation.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
class CustomStack {
    T*  data;
    int cap;
    int top;

    void grow() {
        int  newCap  = cap * 2;
        T*   newData = new T[newCap];
        for (int i = 0; i <= top; i++) newData[i] = data[i];
        delete[] data;
        data = newData;
        cap  = newCap;
    }

public:
    explicit CustomStack(int initCap = 64)
        : cap(initCap), top(-1) {
        data = new T[cap];
    }

    ~CustomStack() { delete[] data; }

    void push(const T& val) {
        if (top + 1 >= cap) grow();
        data[++top] = val;
    }

    T pop() {
        if (isEmpty()) {
            fprintf(stderr, "Stack underflow!\n");
            exit(1);
        }
        return data[top--];
    }

    T& peek() { return data[top]; }
    bool isEmpty() const { return top < 0; }
    int  size()    const { return top + 1; }
};

// ─────────────────────────────────────────────────────────────────────────────
// QueryJob — a unit of work inserted into the Priority Queue
// priority 0 = admin (highest), 1 = normal user query
// ─────────────────────────────────────────────────────────────────────────────
struct QueryJob {
    int  priority;     // 0 = admin, 1 = user
    int  id;           // unique job id
    char query[512];   // raw query string

    QueryJob() : priority(1), id(0) { query[0] = '\0'; }
    QueryJob(int prio, int jobId, const char* q)
        : priority(prio), id(jobId) {
        strncpy(query, q, 511);
        query[511] = '\0';
    }

    // Lower priority value = higher urgency
    bool operator<(const QueryJob& o) const { return priority < o.priority; }
};

// ─────────────────────────────────────────────────────────────────────────────
// PriorityQueue — min-heap (binary heap) on raw array, no STL
// Admin jobs (priority=0) bubble to top automatically.
// ─────────────────────────────────────────────────────────────────────────────
class PriorityQueue {
    QueryJob* heap;
    int       cap;
    int       sz;

    void grow() {
        int       newCap  = cap * 2;
        QueryJob* newHeap = new QueryJob[newCap];
        for (int i = 0; i < sz; i++) newHeap[i] = heap[i];
        delete[] heap;
        heap = newHeap;
        cap  = newCap;
    }

    void siftUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (heap[parent].priority > heap[i].priority) {
                QueryJob tmp = heap[parent];
                heap[parent] = heap[i];
                heap[i]      = tmp;
                i = parent;
            } else break;
        }
    }

    void siftDown(int i) {
        while (true) {
            int smallest = i;
            int l = 2 * i + 1, r = 2 * i + 2;
            if (l < sz && heap[l].priority < heap[smallest].priority) smallest = l;
            if (r < sz && heap[r].priority < heap[smallest].priority) smallest = r;
            if (smallest == i) break;
            QueryJob tmp = heap[i]; heap[i] = heap[smallest]; heap[smallest] = tmp;
            i = smallest;
        }
    }

public:
    explicit PriorityQueue(int initCap = 128) : cap(initCap), sz(0) {
        heap = new QueryJob[cap];
    }
    ~PriorityQueue() { delete[] heap; }

    void enqueue(const QueryJob& job) {
        if (sz >= cap) grow();
        heap[sz++] = job;
        siftUp(sz - 1);
    }

    QueryJob dequeue() {
        QueryJob top = heap[0];
        heap[0] = heap[--sz];
        siftDown(0);
        return top;
    }

    bool isEmpty() const { return sz == 0; }
    int  size()    const { return sz; }
};
