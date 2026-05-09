# NanoDB — Architecture & Query Optimizer
**CS-4002 Applied Programming | FAST-NUCES Islamabad | Spring 2026**

---

## Overview
NanoDB is a mini relational database engine built from scratch in C++ with **zero STL containers**. It implements:

| Component | Implementation |
|-----------|----------------|
| Memory Layer | Fixed-size raw array Buffer Pool |
| Cache Eviction | Doubly Linked List LRU (O(1)) |
| Transaction Log | Ring Buffer (WAL) |
| Type System | Polymorphic `FieldValue` base class + operator overloading |
| System Catalog | Custom HashMap (djb2 + chaining, O(1) lookup) |
| Query Parser | Tokenizer → Shunting-Yard Infix→Postfix → Stack evaluator |
| Priority Scheduler | Binary Min-Heap Priority Queue |
| Indexer | Self-balancing AVL Tree (O(log N)) |
| Join Optimizer | Graph representation + Prim's MST algorithm |

---

## Build & Run

### Requirements
- g++ with C++17 support
- Linux / macOS (or WSL on Windows)

### Compile
```bash
make
```

### Generate data + run all tests
```bash
make run
```

### Run test runner only (if data already exists)
```bash
./test_runner
```

### Check for memory leaks
```bash
make valgrind
```

---

## Project Structure

```
NanoDB/
├── include/
│   ├── Types.h           — FieldValue, IntValue, FloatValue, StringValue
│   ├── Schema.h          — ColumnDef, TableSchema, Row
│   ├── DoublyLinkedList.h— Generic DLL (for LRU cache)
│   ├── Structures.h      — CustomStack, PriorityQueue (min-heap)
│   ├── HashMap.h         — Custom hash map (djb2, chaining)
│   ├── Logger.h          — Execution log writer
│   ├── BufferPool.h      — Pager, LRU eviction, WAL ring buffer
│   ├── AVLTree.h         — Self-balancing AVL index
│   ├── QueryParser.h     — Tokenizer, Infix→Postfix, evaluator
│   ├── JoinOptimizer.h   — Graph + Prim's MST join optimizer
│   ├── SystemCatalog.h   — Table metadata via HashMap
│   ├── TableEngine.h     — Insert, scan, index lookup, update
│   ├── NanoDB.h          — Top-level orchestrator
│   └── DataGenerator.h   — TPC-H synthetic data generator
├── main.cpp              — Database initialiser
├── test_runner.cpp       — All 7 demo test cases (A–G)
├── data/
│   ├── queries.txt       — 50 SQL-like workload queries
│   ├── customer.db       — Customer table (binary)
│   ├── orders.db         — Orders table (binary)
│   └── lineitem.db       — Lineitem table (binary)
├── logs/
│   └── nanodb_execution.log — Full execution log
└── Makefile
```

---

## Test Cases

| Test | What it demonstrates |
|------|----------------------|
| **A** | Infix→Postfix conversion via custom Stack + WHERE evaluation |
| **B** | Sequential scan O(N) vs AVL index O(log N) — visible speedup |
| **C** | 3-table join routed via MST (Prim's algorithm) |
| **D** | LRU eviction count under 50-page memory constraint |
| **E** | Admin job (priority=0) intercepts 50 background jobs |
| **F** | Deep nested expression: arithmetic + modulo + logical operators |
| **G** | Insert 5 rows → shutdown → reboot → verify rows survived |

---

## Key Design Decisions

### Why AVL over Red-Black Tree?
AVL trees are more strictly balanced (height ≤ 1.44 log₂ N vs ~2 log₂ N for RB).  
For read-heavy workloads (TPC-H benchmarks), fewer comparisons matter more than slightly faster inserts.

### Why Prim's MST for join ordering?
MST gives the globally minimum-cost spanning tree of table-join edges in O(V²) — appropriate for small numbers of tables (≤16). Alternative: Kruskal's with union-find (also implemented in comments).

### LRU in O(1)
HashMap maps pageId → DLL node pointer, enabling O(1) moveToFront and O(1) tail eviction. A simple array would be O(N).

---

## GitHub Repository
https://github.com/YOUR_USERNAME/NanoDB

*(Replace with your actual repo URL after pushing)*

---

## Complexity Summary

| Structure | Operation | Time | Space |
|-----------|-----------|------|-------|
| Buffer Pool | fetchPage (hit) | O(1) | O(P) |
| Buffer Pool | fetchPage (miss+evict) | O(1) | O(P) |
| LRU (DLL) | evict tail / move to front | O(1) | O(P) |
| HashMap | put / get | O(1) avg | O(N) |
| AVL Tree | search / insert / delete | O(log N) | O(N) |
| Priority Queue | enqueue / dequeue | O(log N) | O(N) |
| Parser | tokenize + shunting-yard | O(T) | O(T) |
| MST (Prim's) | join optimization | O(V²) | O(V²) |

P = pool capacity, N = number of records, T = token count, V = number of tables
