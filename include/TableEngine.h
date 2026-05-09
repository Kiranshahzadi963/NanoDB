#pragma once
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include "Schema.h"
#include "BufferPool.h"
#include "AVLTree.h"
#include "QueryParser.h"
#include "SystemCatalog.h"
#include "Logger.h"

// ─────────────────────────────────────────────────────────────────────────────
// ROWS_PER_PAGE — how many rows fit in one 4 KB page
// (computed at compile time for a fixed row size scenario;
//  the engine also handles variable sizes at runtime)
// ─────────────────────────────────────────────────────────────────────────────
static const int ROWS_PER_PAGE_ESTIMATE = 32;

// ─────────────────────────────────────────────────────────────────────────────
// TableEngine — manages all operations on one table:
//   • INSERT  (serializes row → buffer pool → disk)
//   • SELECT  (sequential scan with WHERE expression)
//   • SELECT  (AVL-index scan for primary key)
//   • UPDATE
//   • Persistence (load/save via BufferPool)
// ─────────────────────────────────────────────────────────────────────────────
class TableEngine {
    TableSchema  schema;
    BufferPool*  pool;
    AVLTree      index;          // primary key index (first INT column)
    FILE*        dataFile;
    char         dataPath[256];
    int          totalRows;
    int          rowsPerPage;
    SystemCatalog* catalog;
    char         tableName[TABLE_NAME_LEN];

    // ── Open (or create) the data file ──────────────────────────────────────
    void openFile() {
        dataFile = fopen(dataPath, "r+b");
        if (!dataFile) dataFile = fopen(dataPath, "w+b");
        if (!dataFile) {
            fprintf(stderr, "FATAL: Cannot open %s\n", dataPath);
            exit(1);
        }
    }

    // ── Compute slot offset within a page ──────────────────────────────────
    int slotOffset(int slot) const { return slot * schema.rowSize; }

public:
    TableEngine(const char* name, const TableSchema& s,
                BufferPool* bp, SystemCatalog* cat,
                const char* path)
        : schema(s), pool(bp), totalRows(0), dataFile(nullptr), catalog(cat)
    {
        strncpy(tableName, name, TABLE_NAME_LEN - 1);
        tableName[TABLE_NAME_LEN - 1] = '\0';
        strncpy(dataPath, path, 255);
        dataPath[255] = '\0';

        rowsPerPage = (schema.rowSize > 0) ?
                      (PAGE_SIZE / schema.rowSize) : ROWS_PER_PAGE_ESTIMATE;
        if (rowsPerPage < 1) rowsPerPage = 1;

        openFile();
        loadRowCount();
    }

    ~TableEngine() {
        if (dataFile) fclose(dataFile);
    }

    // ── Persist row count metadata at end of file ───────────────────────────
    void saveRowCount() {
        fseek(dataFile, -((long)sizeof(int)), SEEK_END);
        // Write to a fixed meta offset instead
        long metaOffset = (long)1000000 * schema.rowSize + 16;
        fseek(dataFile, metaOffset, SEEK_SET);
        fwrite(&totalRows, sizeof(int), 1, dataFile);
        fflush(dataFile);
    }

    void loadRowCount() {
        long metaOffset = (long)1000000 * schema.rowSize + 16;
        fseek(dataFile, metaOffset, SEEK_SET);
        int n = 0;
        if (fread(&n, sizeof(int), 1, dataFile) == 1 && n > 0 && n < 10000000)
            totalRows = n;
        else
            totalRows = 0;
    }

    // ── Rebuild AVL index from data on disk ─────────────────────────────────
    void buildIndex() {
        Logger::log("Index: Building AVL index for table '%s' (%d rows)...",
                    tableName, totalRows);

        for (int rowId = 0; rowId < totalRows; rowId++) {
            int pageId = rowId / rowsPerPage;
            int slot   = rowId % rowsPerPage;

            Page* pg = pool->fetchPage(pageId);
            char* base = pg->data + slotOffset(slot);

            // Read primary key (first column, assumed INT)
            int pk;
            memcpy(&pk, base, sizeof(int));
            index.insert(pk, pageId, slot);
        }
        Logger::log("Index: AVL tree built — %d nodes, height=%d",
                    index.size(), index.treeHeight());
    }

    // ── INSERT ───────────────────────────────────────────────────────────────
    bool insertRow(const Row& row) {
        int rowId  = totalRows;
        int pageId = rowId / rowsPerPage;
        int slot   = rowId % rowsPerPage;

        Page* pg = pool->fetchPage(pageId);

        // Serialize into page
        char* base = pg->data + slotOffset(slot);
        row.serialize(base, schema);
        pool->markDirty(pageId);

        // Write-ahead log
        pool->walWrite("INSERT row");

        // Update AVL index with primary key (first INT column)
        if (schema.colCount > 0 && row.fields[0] &&
            schema.cols[0].type == DataType::INT) {
            int pk = static_cast<IntValue*>(row.fields[0])->val;
            index.insert(pk, pageId, slot);
        }

        totalRows++;
        if (catalog) catalog->incrementRowCount(tableName);

        // Flush page to disk file directly for durability
        long offset = (long)pageId * PAGE_SIZE;
        fseek(dataFile, offset, SEEK_SET);
        fwrite(pg->data, 1, PAGE_SIZE, dataFile);

        return true;
    }

    // ── SELECT: sequential scan with optional WHERE clause ──────────────────
    // Returns number of matching rows printed
    int selectScan(const char* whereExpr = nullptr) {
        Logger::log("Scan: Sequential scan on '%s' (%d rows)", tableName, totalRows);

        Token   infix[MAX_TOKENS], postfix[MAX_TOKENS];
        int     postfixCount = 0;
        bool    hasWhere = (whereExpr && strlen(whereExpr) > 0);

        if (hasWhere) {
            int infixCount = tokenize(whereExpr, infix);
            postfixCount   = infixToPostfix(infix, infixCount, postfix);
        }

        clock_t start = clock();
        int matched = 0;

        for (int rowId = 0; rowId < totalRows; rowId++) {
            int pageId = rowId / rowsPerPage;
            int slot   = rowId % rowsPerPage;

            Page* pg   = pool->fetchPage(pageId);
            char* base = pg->data + slotOffset(slot);

            Row* r = Row::deserialize(base, schema);

            bool include = true;
            if (hasWhere) include = evaluatePostfix(postfix, postfixCount, *r, schema);

            if (include) {
                r->print(schema);
                matched++;
            }
            delete r;
        }

        clock_t end = clock();
        double ms = 1000.0 * (end - start) / CLOCKS_PER_SEC;
        Logger::log("Scan: Sequential scan complete — %d/%d rows matched in %.2f ms",
                    matched, totalRows, ms);
        return matched;
    }

    // ── SELECT: AVL index lookup by primary key ──────────────────────────────
    Row* indexLookup(int pk) {
        Logger::log("Index: AVL lookup for key=%d in table '%s'", pk, tableName);

        clock_t start = clock();
        AVLNode* node = index.search(pk);
        clock_t end   = clock();
        double ms = 1000.0 * (end - start) / CLOCKS_PER_SEC;

        if (!node) {
            Logger::log("Index: key=%d NOT FOUND (%.4f ms)", pk, ms);
            return nullptr;
        }

        Page* pg   = pool->fetchPage(node->pageId);
        char* base = pg->data + slotOffset(node->slot);
        Row*  r    = Row::deserialize(base, schema);

        Logger::log("Index: key=%d found at page=%d slot=%d in %.4f ms",
                    pk, node->pageId, node->slot, ms);
        return r;
    }

    // ── UPDATE: set a column value for rows matching a key ──────────────────
    int updateRow(int pk, const char* colName, FieldValue* newVal) {
        AVLNode* node = index.search(pk);
        if (!node) {
            Logger::log("Update: key=%d not found in '%s'", pk, tableName);
            return 0;
        }

        Page* pg   = pool->fetchPage(node->pageId);
        char* base = pg->data + slotOffset(node->slot);

        Row* r = Row::deserialize(base, schema);
        int ci = schema.colIndex(colName);
        if (ci >= 0 && r->fields[ci]) {
            delete r->fields[ci];
            r->fields[ci] = newVal->clone();
        }
        r->serialize(base, schema);
        pool->markDirty(node->pageId);

        // Persist
        long offset = (long)node->pageId * PAGE_SIZE;
        fseek(dataFile, offset, SEEK_SET);
        fwrite(pg->data, 1, PAGE_SIZE, dataFile);

        Logger::log("Update: key=%d column '%s' updated in '%s'", pk, colName, tableName);
        delete r;
        return 1;
    }

    // ── Flush all dirty pages to disk ───────────────────────────────────────
    void flush() {
        pool->flushAll();
        saveRowCount();
        fflush(dataFile);
        Logger::log("Flush: Table '%s' flushed to disk (%d rows)", tableName, totalRows);
    }

    // ── Accessors ────────────────────────────────────────────────────────────
    int          getRowCount()  const { return totalRows; }
    const char*  getTableName() const { return tableName; }
    TableSchema& getSchema()          { return schema; }
    AVLTree&     getIndex()           { return index; }
};
