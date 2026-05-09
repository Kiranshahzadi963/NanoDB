#pragma once
#include "HashMap.h"
#include "Schema.h"
#include "Logger.h"
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// TableMeta — everything the catalog stores about one table
// ─────────────────────────────────────────────────────────────────────────────
struct TableMeta {
    TableSchema schema;
    char        filePath[256];   // where this table's data lives on disk
    int         rowCount;        // number of rows currently inserted
    int         pageCount;       // number of pages used

    TableMeta() : rowCount(0), pageCount(0) { filePath[0] = '\0'; }
};

// ─────────────────────────────────────────────────────────────────────────────
// SystemCatalog — maps table names → TableMeta via our custom HashMap
// Achieves O(1) average-case lookup (djb2 hash, chaining collision resolution)
// ─────────────────────────────────────────────────────────────────────────────
class SystemCatalog {
    HashMap<TableMeta> catalog;

public:
    // Register a new table
    void registerTable(const char* name, const TableSchema& schema,
                       const char* filePath) {
        if (catalog.contains(name)) {
            Logger::log("Catalog: Table '%s' already registered.", name);
            return;
        }
        TableMeta meta;
        meta.schema = schema;
        strncpy(meta.filePath, filePath, 255);
        meta.filePath[255] = '\0';
        meta.rowCount  = 0;
        meta.pageCount = 0;
        catalog.put(name, meta);
        Logger::log("Catalog: Registered table '%s' at path '%s'", name, filePath);
    }

    TableMeta* getTable(const char* name) {
        return catalog.get(name);
    }

    bool tableExists(const char* name) {
        return catalog.contains(name);
    }

    void incrementRowCount(const char* name, int by = 1) {
        TableMeta* m = catalog.get(name);
        if (m) m->rowCount += by;
    }

    void printAll() const {
        printf("\n=== System Catalog ===\n");
        // forEach is templated — use a lambda-compatible functor
        const_cast<SystemCatalog*>(this)->catalog.forEach(
            [](const char* key, TableMeta& meta) {
                printf("  Table: %-20s  Rows: %6d  File: %s\n",
                       key, meta.rowCount, meta.filePath);
            }
        );
        printf("======================\n\n");
    }
};
