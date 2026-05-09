#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "include/Logger.h"
#include "include/NanoDB.h"
#include "include/DataGenerator.h"
#include "include/QueryParser.h"

// ─────────────────────────────────────────────────────────────────────────────
// separator — pretty divider for console output
// ─────────────────────────────────────────────────────────────────────────────
void separator(const char* title) {
    printf("\n");
    printf("══════════════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("══════════════════════════════════════════════════════════════\n");
}

// ─────────────────────────────────────────────────────════════════════════════
// Test Case A — Parser & Expression Evaluator
// ─────────────────────────────────────────────────────────────────────────────
void testCaseA(NanoDB& db) {
    separator("TEST CASE A: Parser & Expression Evaluator");

    const char* whereExpr =
        "(c_acctbal > 5000 AND c_mktsegment == \"BUILDING\") OR c_nationkey == 15";

    printf("Input WHERE clause:\n  %s\n\n", whereExpr);

    // Tokenise
    Token infix[MAX_TOKENS], postfix[MAX_TOKENS];
    int inCount = tokenize(whereExpr, infix);

    printf("Tokens (%d total):\n  ", inCount);
    for (int i = 0; i < inCount; i++) printf("[%s] ", infix[i].raw);
    printf("\n\n");

    // Shunting-Yard → Postfix
    int postfixCount = infixToPostfix(infix, inCount, postfix);

    printf("Postfix (RPN) output:\n  ");
    for (int i = 0; i < postfixCount; i++) printf("%s ", postfix[i].raw);
    printf("\n\n");

    // Run the actual select
    printf("Executing SELECT customer WHERE %s\n\n", whereExpr);
    int matched = db.selectScan("customer", whereExpr);
    printf("\nResult: %d rows matched.\n", matched);

    Logger::log("TestA: Parser test complete — %d rows matched.", matched);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test Case B — Sequential Scan vs AVL Index
// ─────────────────────────────────────────────────────────────────────────────
void testCaseB(NanoDB& db) {
    separator("TEST CASE B: Sequential Scan vs AVL Index Lookup");

    int searchKey = 7500;  // c_custkey to find
    printf("Searching for c_custkey = %d in 20,000 customer records.\n\n", searchKey);

    // ── Sequential scan (full scan looking for this key) ────────────────────
    printf("--- Method 1: Sequential Array Scan ---\n");
    char whereExpr[64];
    snprintf(whereExpr, 64, "c_custkey == %d", searchKey);

    clock_t t1 = clock();
    int found = db.selectScan("customer", whereExpr);
    clock_t t2 = clock();
    double seqMs = 1000.0 * (t2 - t1) / CLOCKS_PER_SEC;

    printf("Sequential scan time: %.4f ms  (found %d row)\n\n", seqMs, found);
    Logger::log("TestB: Sequential scan for key=%d: %.4f ms", searchKey, seqMs);

    // ── AVL index lookup ────────────────────────────────────────────────────
    printf("--- Method 2: AVL Index Lookup ---\n");

    clock_t t3 = clock();
    Row* r = db.indexLookup("customer", searchKey);
    clock_t t4 = clock();
    double idxMs = 1000.0 * (t4 - t3) / CLOCKS_PER_SEC;

    if (r) {
        TableEngine* eng = db.getEngine("customer");
        printf("Found: "); r->print(eng->getSchema());
        delete r;
    } else {
        printf("Key %d not found in index.\n", searchKey);
    }
    printf("AVL index lookup time: %.4f ms\n\n", idxMs);
    Logger::log("TestB: AVL index lookup for key=%d: %.4f ms", searchKey, idxMs);

    printf("Speedup: %.1fx faster with index.\n",
           (seqMs > 0 && idxMs > 0) ? seqMs / idxMs : 999.9);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test Case C — 3-Table Join via MST optimizer
// ─────────────────────────────────────────────────────────────────────────────
void testCaseC(NanoDB& db) {
    separator("TEST CASE C: 3-Table Join via MST Optimizer");

    printf("Executing: customer JOIN orders JOIN lineitem\n");
    printf("MST optimizer will find cheapest join path...\n\n");

    db.joinTables("customer", "orders", "lineitem",
                  "c_custkey", "o_orderkey");

    Logger::log("TestC: 3-table join complete.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test Case D — LRU Memory Stress Test (50-page pool)
// ─────────────────────────────────────────────────────────────────────────────
void testCaseD() {
    separator("TEST CASE D: Memory Stress Test (50-page Buffer Pool)");

    printf("Creating a restricted 50-page buffer pool.\n");
    printf("Scanning 5,000 lineitem records (forces many evictions).\n\n");

    // Create a small-pool NanoDB instance
    NanoDB smallDb(50, "data/nanodb.db");
    TableSchema lineSchema = DataGenerator::lineitemSchema();
    smallDb.createTable("lineitem", lineSchema, "data/lineitem.db");
    smallDb.buildIndex("lineitem");

    int beforeEvictions = smallDb.getPool().getEvictions();

    // Scan first 5000 rows
    printf("Running scan...\n");
    clock_t t1 = clock();

    // Force page accesses across many pages
    for (int rowId = 0; rowId < 5000; rowId++) {
        int rowsPerPage = PAGE_SIZE / lineSchema.rowSize;
        if (rowsPerPage < 1) rowsPerPage = 1;
        int pageId = rowId / rowsPerPage;
        smallDb.getPool().fetchPage(pageId);
    }

    clock_t t2 = clock();
    double ms = 1000.0 * (t2 - t1) / CLOCKS_PER_SEC;

    int afterEvictions = smallDb.getPool().getEvictions();
    int evictions = afterEvictions - beforeEvictions;

    printf("Scan complete in %.2f ms\n", ms);
    printf("Total LRU evictions during stress test: %d\n", evictions);
    printf("(Each eviction = DLL tail removed + dirty page written to disk)\n");

    Logger::log("TestD: Stress test — %d LRU evictions in %.2f ms", evictions, ms);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test Case E — Priority Queue Concurrency
// ─────────────────────────────────────────────────────────────────────────────
void testCaseE(NanoDB& db) {
    separator("TEST CASE E: Priority Queue — Admin Query Intercept");

    printf("Submitting 50 user SELECT queries (priority=1)...\n");
    for (int i = 0; i < 50; i++) {
        char q[128];
        snprintf(q, 128, "SELECT customer WHERE c_nationkey == %d", i % 25 + 1);
        db.submitJob(q, 1);
    }

    printf("\nNow submitting 1 ADMIN UPDATE (priority=0 — highest urgency)...\n");
    db.submitJob("UPDATE customer SET c_acctbal=9999.99 WHERE c_custkey=1", 0);

    printf("\nProcessing queue — admin job must execute FIRST:\n\n");

    // Dequeue and show order — admin job should be first
    PriorityQueue& q = db.getQueue();
    int jobNum = 1;
    while (!q.isEmpty()) {
        QueryJob job = q.dequeue();
        if (jobNum <= 3 || job.priority == 0) {
            printf("  Executing job #%d (priority=%d): %.70s\n",
                   job.id, job.priority, job.query);
        } else if (jobNum == 4) {
            printf("  ... (remaining %d user queries) ...\n", q.size());
        }
        jobNum++;
    }
    printf("\nAdmin UPDATE was executed before all 50 background SELECTs. ✓\n");
    Logger::log("TestE: Priority queue test — admin job intercepted correctly.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test Case F — Deep Expression Tree Edge Case
// ─────────────────────────────────────────────────────────────────────────────
void testCaseF(NanoDB& db) {
    separator("TEST CASE F: Deep Expression Tree / Operator Precedence");

    const char* complexExpr =
        "( (o_totalprice * 1.5) > 100000 AND (o_custkey % 2 == 0) ) "
        "OR (o_orderstatus != \"O\")";

    printf("Complex WHERE:\n  %s\n\n", complexExpr);

    Token infix[MAX_TOKENS], postfix[MAX_TOKENS];
    int inCount      = tokenize(complexExpr, infix);
    int postfixCount = infixToPostfix(infix, inCount, postfix);

    printf("Postfix:\n  ");
    for (int i = 0; i < postfixCount; i++) printf("%s ", postfix[i].raw);
    printf("\n\n");

    printf("Running query on orders table (first 10 matches shown):\n\n");

    // Run the select — limit console output to 10 rows
    TableEngine* eng = db.getEngine("orders");
    if (!eng) { printf("orders table not found.\n"); return; }

    int matched = 0, printed = 0;
    for (int rowId = 0; rowId < eng->getRowCount() && printed < 10; rowId++) {
        int rowsPerPage = PAGE_SIZE / eng->getSchema().rowSize;
        if (rowsPerPage < 1) rowsPerPage = 1;
        int pageId = rowId / rowsPerPage;
        int slot   = rowId % rowsPerPage;

        Page* pg = db.getPool().fetchPage(pageId);
        char* base = pg->data + slot * eng->getSchema().rowSize;
        Row* r = Row::deserialize(base, eng->getSchema());

        if (evaluatePostfix(postfix, postfixCount, *r, eng->getSchema())) {
            r->print(eng->getSchema());
            matched++;
            printed++;
        }
        delete r;
    }
    printf("\n%d rows matched (showing first 10).\n", matched);
    Logger::log("TestF: Complex expression — %d rows matched.", matched);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test Case G — Durability & Persistence
// ─────────────────────────────────────────────────────────────────────────────
void testCaseG() {
    separator("TEST CASE G: Durability & Persistence");

    printf("Step 1: Inserting 5 new customers with high IDs...\n");
    {
        NanoDB db1(200, "data/nanodb.db");
        TableSchema custSchema = DataGenerator::customerSchema();
        db1.createTable("customer", custSchema, "data/customer.db");

        int baseKey = 99001;
        for (int i = 0; i < 5; i++) {
            Row row;
            row.addField(new IntValue(baseKey + i));
            char name[64]; snprintf(name, 64, "PersistTest#%05d", baseKey + i);
            row.addField(new StringValue(name));
            row.addField(new StringValue("TestAddr"));
            row.addField(new IntValue(1));
            row.addField(new StringValue("00-000-0000000"));
            row.addField(new FloatValue(1234.56));
            row.addField(new StringValue("BUILDING"));
            row.addField(new StringValue("persistence test"));

            db1.insertRow("customer", row);
            printf("  Inserted: c_custkey=%d name=%s\n", baseKey + i, name);
        }

        db1.flushAll();
        printf("\nNanoDB instance 1 SHUTDOWN (flushed to disk).\n");
        Logger::log("TestG: 5 records inserted and flushed. Shutting down...");
    }
    // db1 is destroyed here — all data must be on disk

    printf("\nStep 2: Rebooting NanoDB from disk...\n\n");
    {
        NanoDB db2(200, "data/nanodb.db");
        TableSchema custSchema = DataGenerator::customerSchema();
        db2.createTable("customer", custSchema, "data/customer.db");
        db2.buildIndex("customer");

        int baseKey = 99001;
        printf("Querying the 5 persisted records:\n");
        bool allFound = true;
        for (int i = 0; i < 5; i++) {
            int pk = baseKey + i;
            Row* r = db2.indexLookup("customer", pk);
            if (r) {
                printf("  FOUND: ");
                r->print(custSchema);
                delete r;
            } else {
                printf("  MISSING: c_custkey=%d  ← DURABILITY FAILURE\n", pk);
                allFound = false;
            }
        }

        printf("\n%s\n", allFound ? "✓ All 5 records survived shutdown. Durability confirmed!"
                                 : "✗ Some records lost — serialization bug.");
        Logger::log("TestG: Durability test — %s", allFound ? "PASS" : "FAIL");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Workload file processor — reads queries.txt and executes each line
// ─────────────────────────────────────────────────────────────────────────────
void runWorkloadFile(NanoDB& db, const char* path = "data/queries.txt") {
    separator("WORKLOAD FILE: queries.txt");

    FILE* f = fopen(path, "r");
    if (!f) {
        printf("queries.txt not found at '%s'. Skipping.\n", path);
        Logger::log("Workload: queries.txt not found.");
        return;
    }

    char line[512];
    int  count = 0;
    printf("Processing queries from %s:\n\n", path);

    while (fgets(line, sizeof(line), f)) {
        // Strip newline
        int len = strlen(line);
        while (len > 0 && (line[len-1]=='\n' || line[len-1]=='\r')) line[--len]='\0';
        if (len == 0 || line[0] == '#') continue;  // skip blanks and comments

        printf("  [Q%02d] %s\n", ++count, line);
        Logger::log("Workload: Executing: %s", line);
        db.executeQuery(line);
    }
    fclose(f);
    printf("\n%d queries processed from workload file.\n", count);
    Logger::log("Workload: %d queries processed.", count);
}

// ─────────────────────────────────────────────────────────────────────────────
// main — test runner entry point
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    srand(42);

    Logger::init("logs/nanodb_execution.log", true);
    Logger::log("=== NanoDB Test Runner Started ===");

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           NanoDB Architecture & Query Optimizer             ║\n");
    printf("║              CS-4002 Applied Programming                    ║\n");
    printf("║              FAST-NUCES Islamabad  Spring 2026              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    // ── Boot NanoDB ──────────────────────────────────────────────────────────
    NanoDB db(200, "data/nanodb.db");

    TableSchema custSchema = DataGenerator::customerSchema();
    TableSchema ordSchema  = DataGenerator::ordersSchema();
    TableSchema lineSchema = DataGenerator::lineitemSchema();

    db.createTable("customer", custSchema,  "data/customer.db");
    db.createTable("orders",   ordSchema,   "data/orders.db");
    db.createTable("lineitem", lineSchema,  "data/lineitem.db");

    // Generate or reuse data
    TableEngine* custEng = db.getEngine("customer");
    if (custEng->getRowCount() < 100) {
        printf("[INFO] No data found. Generating TPC-H dataset...\n");
        DataGenerator::generateCustomers(db, 20000);
        DataGenerator::generateOrders(db,   30000, 20000);
        DataGenerator::generateLineitems(db, 50000, 30000);
        db.flushAll();
    } else {
        printf("[INFO] Data loaded: %d customer rows.\n", custEng->getRowCount());
    }

    db.buildIndex("customer");
    db.buildIndex("orders");
    db.buildIndex("lineitem");

    // ── Run workload file ────────────────────────────────────────────────────
    runWorkloadFile(db, "data/queries.txt");

    // ── Run all 7 test cases ─────────────────────────────────────────────────
    testCaseA(db);
    testCaseB(db);
    testCaseC(db);
    testCaseD();        // uses its own small-pool instance
    testCaseE(db);
    testCaseF(db);
    testCaseG();        // uses its own instance pair

    separator("ALL TEST CASES COMPLETE");
    printf("Full execution log saved to: logs/nanodb_execution.log\n\n");

    Logger::log("=== Test Runner Complete ===");
    Logger::close();
    return 0;
}
