#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "include/Logger.h"
#include "include/NanoDB.h"
#include "include/DataGenerator.h"

// ─────────────────────────────────────────────────────────────────────────────
// main.cpp — NanoDB startup
// Initializes schemas, loads (or generates) data, then runs test runner.
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    srand(42);  // reproducible data

    // ── Initialise logging ───────────────────────────────────────────────────
    Logger::init("logs/nanodb_execution.log", true);
    Logger::log("=== NanoDB started ===");

    // ── Parse CLI flag: --generate to force data regeneration ───────────────
    bool forceGenerate = false;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--generate") == 0) forceGenerate = true;

    // ── Create NanoDB engine (200 page pool) ─────────────────────────────────
    NanoDB db(200, "data/nanodb.db");

    // ── Create table schemas ─────────────────────────────────────────────────
    TableSchema custSchema  = DataGenerator::customerSchema();
    TableSchema ordSchema   = DataGenerator::ordersSchema();
    TableSchema lineSchema  = DataGenerator::lineitemSchema();

    db.createTable("customer", custSchema,  "data/customer.db");
    db.createTable("orders",   ordSchema,   "data/orders.db");
    db.createTable("lineitem", lineSchema,  "data/lineitem.db");

    // ── Load or generate data ────────────────────────────────────────────────
    TableEngine* custEng = db.getEngine("customer");
    bool needsGenerate   = forceGenerate || (custEng->getRowCount() < 100);

    if (needsGenerate) {
        Logger::log("Generating TPC-H dataset: 20K customers, 30K orders, 50K lineitems...");
        printf("\n[NanoDB] Generating dataset (this may take ~30 seconds)...\n");
        DataGenerator::generateCustomers(db, 20000);
        DataGenerator::generateOrders(db,   30000, 20000);
        DataGenerator::generateLineitems(db, 50000, 30000);
        db.flushAll();
        Logger::log("Dataset generation complete. 100K total rows.");
    } else {
        Logger::log("Data already exists (%d customer rows). Skipping generation.",
                    custEng->getRowCount());
    }

    // ── Build indexes ────────────────────────────────────────────────────────
    Logger::log("Building AVL indexes...");
    db.buildIndex("customer");
    db.buildIndex("orders");
    db.buildIndex("lineitem");

    db.printCatalog();

    Logger::log("NanoDB ready. Run test_runner for demo test cases.");
    printf("\n[NanoDB] Engine ready. Run ./test_runner for the demo.\n\n");

    Logger::close();
    return 0;
}
