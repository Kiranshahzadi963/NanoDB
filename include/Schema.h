#pragma once
#include "Types.h"
#include <cstdio>
#include <cstring>

static const int MAX_COLS   = 16;
static const int MAX_TABLES = 16;
static const int COL_NAME_LEN = 32;
static const int TABLE_NAME_LEN = 32;

// ─────────────────────────────────────────────────────────────────────────────
// ColumnDef — describes one column (name + type)
// ─────────────────────────────────────────────────────────────────────────────
struct ColumnDef {
    char     name[COL_NAME_LEN];
    DataType type;
    int      size;   // bytes on disk: 4 (INT), 8 (FLOAT), 64 (STRING)

    ColumnDef() : type(DataType::INT), size(4) { name[0] = '\0'; }
    ColumnDef(const char* n, DataType t) : type(t) {
        strncpy(name, n, COL_NAME_LEN - 1);
        name[COL_NAME_LEN - 1] = '\0';
        if (t == DataType::INT)    size = sizeof(int);
        else if (t == DataType::FLOAT) size = sizeof(double);
        else                           size = STR_CAP;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// TableSchema — holds all column definitions for one table
// ─────────────────────────────────────────────────────────────────────────────
struct TableSchema {
    char      tableName[TABLE_NAME_LEN];
    ColumnDef cols[MAX_COLS];
    int       colCount;
    int       rowSize;   // total bytes per serialized row

    TableSchema() : colCount(0), rowSize(0) { tableName[0] = '\0'; }

    void addColumn(const char* name, DataType type) {
        if (colCount >= MAX_COLS) return;
        cols[colCount] = ColumnDef(name, type);
        rowSize += cols[colCount].size;
        colCount++;
    }

    int colIndex(const char* name) const {
        for (int i = 0; i < colCount; i++)
            if (strcmp(cols[i].name, name) == 0) return i;
        return -1;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Row — one database row; owns its FieldValue pointers
// ─────────────────────────────────────────────────────────────────────────────
struct Row {
    FieldValue* fields[MAX_COLS];
    int         fieldCount;

    Row() : fieldCount(0) {
        for (int i = 0; i < MAX_COLS; i++) fields[i] = nullptr;
    }

    // Deep copy constructor
    Row(const Row& o) : fieldCount(o.fieldCount) {
        for (int i = 0; i < fieldCount; i++)
            fields[i] = o.fields[i] ? o.fields[i]->clone() : nullptr;
        for (int i = fieldCount; i < MAX_COLS; i++) fields[i] = nullptr;
    }

    ~Row() {
        for (int i = 0; i < fieldCount; i++) {
            delete fields[i];
            fields[i] = nullptr;
        }
    }

    void addField(FieldValue* fv) {
        if (fieldCount < MAX_COLS) fields[fieldCount++] = fv;
    }

    void print(const TableSchema& schema) const {
        for (int i = 0; i < fieldCount; i++) {
            printf("%s=", schema.cols[i].name);
            if (fields[i]) fields[i]->print();
            if (i < fieldCount - 1) printf(", ");
        }
        printf("\n");
    }

    // Serialize row into buffer (caller ensures buffer is rowSize bytes)
    void serialize(char* buf, const TableSchema& schema) const {
        int offset = 0;
        for (int i = 0; i < fieldCount; i++) {
            offset += fields[i]->serialize(buf + offset);
        }
        (void)schema;
    }

    // Deserialize row from buffer
    static Row* deserialize(const char* buf, const TableSchema& schema) {
        Row* r = new Row();
        int offset = 0;
        for (int i = 0; i < schema.colCount; i++) {
            FieldValue* fv = nullptr;
            if (schema.cols[i].type == DataType::INT) {
                fv = IntValue::deserialize(buf + offset);
            } else if (schema.cols[i].type == DataType::FLOAT) {
                fv = FloatValue::deserialize(buf + offset);
            } else {
                fv = StringValue::deserialize(buf + offset);
            }
            r->addField(fv);
            offset += schema.cols[i].size;
        }
        return r;
    }
};
