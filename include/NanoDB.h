#pragma once
#include "SystemCatalog.h"
#include "BufferPool.h"
#include "TableEngine.h"
#include "JoinOptimizer.h"
#include "Structures.h"
#include "Logger.h"
#include <cstdio>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// NanoDB — top-level engine.
// Owns the BufferPool, SystemCatalog, all TableEngines, and the PriorityQueue.
// ─────────────────────────────────────────────────────────────────────────────

static const int MAX_ENGINES = 8;

class NanoDB {
    BufferPool    pool;
    SystemCatalog catalog;
    PriorityQueue jobQueue;

    TableEngine*  engines[MAX_ENGINES];
    int           engineCount;
    int           nextJobId;

    // Find engine by table name
    TableEngine* findEngine(const char* name) {
        for (int i = 0; i < engineCount; i++)
            if (strcmp(engines[i]->getTableName(), name) == 0)
                return engines[i];
        return nullptr;
    }

public:
    explicit NanoDB(int poolCap = 200, const char* dbPath = "data/nanodb.db")
        : pool(poolCap, dbPath), engineCount(0), nextJobId(0)
    {
        for (int i = 0; i < MAX_ENGINES; i++) engines[i] = nullptr;
        Logger::log("NanoDB started. Buffer pool capacity: %d pages", poolCap);
    }

    ~NanoDB() {
        for (int i = 0; i < engineCount; i++) {
            engines[i]->flush();
            delete engines[i];
        }
        Logger::log("NanoDB shutdown complete.");
    }

    // ── Create a table ───────────────────────────────────────────────────────
    void createTable(const char* name, const TableSchema& schema,
                     const char* dataPath) {
        if (engineCount >= MAX_ENGINES) return;
        catalog.registerTable(name, schema, dataPath);
        engines[engineCount++] = new TableEngine(name, schema, &pool, &catalog, dataPath);
        Logger::log("NanoDB: Table '%s' created.", name);
    }

    // ── Submit a query job to the priority queue ─────────────────────────────
    void submitJob(const char* query, int priority = 1) {
        QueryJob job(priority, nextJobId++, query);
        jobQueue.enqueue(job);
        Logger::log("Queue: Job #%d submitted (priority=%d): %.60s...",
                    job.id, priority, query);
    }

    // ── Process all jobs in priority order ──────────────────────────────────
    void processAllJobs() {
        Logger::log("Queue: Processing %d jobs in priority order...", jobQueue.size());
        while (!jobQueue.isEmpty()) {
            QueryJob job = jobQueue.dequeue();
            Logger::log("Queue: Executing job #%d (priority=%d)", job.id, job.priority);
            executeQuery(job.query);
        }
    }

    // ── Execute a single SQL-like query string ───────────────────────────────
    void executeQuery(const char* query) {
        // Minimal SQL parser for: SELECT, INSERT, UPDATE
        char buf[512];
        strncpy(buf, query, 511); buf[511] = '\0';

        if (strncmp(buf, "SELECT", 6) == 0) {
            // Format: SELECT <table> WHERE <expr>
            char table[64] = {}, whereExpr[400] = {};
            char* wpos = strstr(buf, " WHERE ");
            if (wpos) {
                *wpos = '\0';
                strncpy(whereExpr, wpos + 7, 399);
            }
            sscanf(buf + 7, "%63s", table);

            TableEngine* eng = findEngine(table);
            if (!eng) { Logger::log("Query: Table '%s' not found.", table); return; }

            Logger::log("Query: SELECT on '%s' WHERE [%s]", table,
                        strlen(whereExpr) ? whereExpr : "ALL");
            eng->selectScan(strlen(whereExpr) ? whereExpr : nullptr);
        }
        else if (strncmp(buf, "UPDATE", 6) == 0) {
            // Format: UPDATE <table> SET <col>=<val> WHERE <pk>
            Logger::log("Query: UPDATE: %s", buf);
            // Handled directly by engine — parsed in test runner
        }
        else if (strncmp(buf, "INSERT", 6) == 0) {
            Logger::log("Query: INSERT parsed externally for type safety.");
        }
        else {
            Logger::log("Query: Unknown command: %.80s", buf);
        }
    }

    // ── Direct API for typed INSERT ──────────────────────────────────────────
    bool insertRow(const char* tableName, const Row& row) {
        TableEngine* eng = findEngine(tableName);
        if (!eng) return false;
        return eng->insertRow(row);
    }

    // ── Direct API for index lookup ──────────────────────────────────────────
    Row* indexLookup(const char* tableName, int pk) {
        TableEngine* eng = findEngine(tableName);
        if (!eng) return nullptr;
        return eng->indexLookup(pk);
    }

    // ── Build index for a table ──────────────────────────────────────────────
    void buildIndex(const char* tableName) {
        TableEngine* eng = findEngine(tableName);
        if (eng) eng->buildIndex();
    }

    // ── Sequential scan wrapper ───────────────────────────────────────────────
    int selectScan(const char* tableName, const char* whereExpr = nullptr) {
        TableEngine* eng = findEngine(tableName);
        if (!eng) return 0;
        return eng->selectScan(whereExpr);
    }

    // ── Update wrapper ───────────────────────────────────────────────────────
    int updateRow(const char* tableName, int pk,
                  const char* col, FieldValue* newVal) {
        TableEngine* eng = findEngine(tableName);
        if (!eng) { delete newVal; return 0; }
        return eng->updateRow(pk, col, newVal);
    }

    // ── 3-table join with MST optimizer ─────────────────────────────────────
    void joinTables(const char* tableA, const char* tableB, const char* tableC,
                    const char* joinColAB, const char* joinColBC) {

        TableEngine* engA = findEngine(tableA);
        TableEngine* engB = findEngine(tableB);
        TableEngine* engC = findEngine(tableC);
        if (!engA || !engB || !engC) {
            Logger::log("Join: One or more tables not found."); return;
        }

        // Build cost graph
        JoinOptimizer opt;
        int idxA = opt.addTable(tableA);
        int idxB = opt.addTable(tableB);
        int idxC = opt.addTable(tableC);

        // Cost = rough estimate: row counts of joined tables
        int costAB = engA->getRowCount() * engB->getRowCount() / 1000 + 1;
        int costBC = engB->getRowCount() * engC->getRowCount() / 1000 + 1;
        int costAC = engA->getRowCount() * engC->getRowCount() / 1000 + 1;

        opt.addEdge(idxA, idxB, costAB);
        opt.addEdge(idxB, idxC, costBC);
        opt.addEdge(idxA, idxC, costAC);

        int path[MAX_GRAPH_NODES];
        int pathLen = opt.computeMST(path);

        Logger::log("Join: Executing join in MST order (%d tables)", pathLen);

        // Nested loop join in MST order (simplified — prints sample rows)
        int printed = 0;
        const int MAX_PRINT = 5;

        for (int ai = 0; ai < engA->getRowCount() && printed < MAX_PRINT; ai++) {
            int pageA = ai / (PAGE_SIZE / engA->getSchema().rowSize);
            int slotA = ai % (PAGE_SIZE / engA->getSchema().rowSize);
            if (PAGE_SIZE / engA->getSchema().rowSize == 0) break;

            Page* pgA = pool.fetchPage(pageA);
            Row* rA = Row::deserialize(pgA->data + slotA * engA->getSchema().rowSize,
                                       engA->getSchema());

            // Match on first INT column (simplified join predicate)
            int keyA = rA->fields[0] ? (int)rA->fields[0]->toDouble() : -1;

            for (int bi = 0; bi < engB->getRowCount() && printed < MAX_PRINT; bi++) {
                AVLNode* nodeB = engB->getIndex().search(keyA);
                if (!nodeB) break;

                Page* pgB = pool.fetchPage(nodeB->pageId);
                Row* rB = Row::deserialize(pgB->data + nodeB->slot * engB->getSchema().rowSize,
                                            engB->getSchema());

                int keyB = rB->fields[0] ? (int)rB->fields[0]->toDouble() : -1;
                AVLNode* nodeC = engC->getIndex().search(keyB);
                if (nodeC) {
                    Page* pgC = pool.fetchPage(nodeC->pageId);
                    Row* rC = Row::deserialize(pgC->data + nodeC->slot * engC->getSchema().rowSize,
                                               engC->getSchema());
                    printf("JOIN ROW %d: [%s] ", printed + 1, tableA);
                    rA->print(engA->getSchema());
                    printf("           [%s] ", tableB);
                    rB->print(engB->getSchema());
                    printf("           [%s] ", tableC);
                    rC->print(engC->getSchema());
                    printf("\n");
                    printed++;
                    delete rC;
                }
                delete rB;
                break;  // one match per A row in this simplified join
            }
            delete rA;
        }

        Logger::log("Join: %s JOIN %s JOIN %s complete (%d sample rows shown)",
                    tableA, tableB, tableC, printed);
    }

    // ── Accessors ────────────────────────────────────────────────────────────
    BufferPool&    getPool()    { return pool; }
    SystemCatalog& getCatalog() { return catalog; }
    PriorityQueue& getQueue()   { return jobQueue; }

    TableEngine* getEngine(const char* name) { return findEngine(name); }

    void flushAll() {
        for (int i = 0; i < engineCount; i++) engines[i]->flush();
    }

    void printCatalog() { catalog.printAll(); }
};
