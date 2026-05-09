#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "NanoDB.h"

static const char* SEGMENTS[]   = {"BUILDING","AUTOMOBILE","MACHINERY","HOUSEHOLD","FURNITURE"};
static const char* ORDERSTATUS[]= {"O","F","P"};
static const char* PRIORITIES[] = {"1-URGENT","2-HIGH","3-MEDIUM","4-NOT SPECIFIED","5-LOW"};
static const char* SHIP_MODES[] = {"AIR","FOB","MAIL","RAIL","SHIP","TRUCK","EXPRESS"};

class DataGenerator {
    static int randInt(int lo, int hi) { return lo + rand() % (hi - lo + 1); }
    static double randFloat(double lo, double hi) {
        return lo + (hi - lo) * ((double)rand() / RAND_MAX);
    }
    static const char* pick(const char** arr, int n) { return arr[rand() % n]; }

public:
    // ── Schemas ──────────────────────────────────────────────────────────────
    // customer: INT, STRING, STRING, INT, STRING, FLOAT, STRING, STRING
    // row size = 4 + 64 + 64 + 4 + 64 + 8 + 64 + 64 = 336 bytes
    static TableSchema customerSchema() {
        TableSchema s;
        strncpy(s.tableName, "customer", TABLE_NAME_LEN - 1);
        s.addColumn("c_custkey",    DataType::INT);
        s.addColumn("c_name",       DataType::STRING);
        s.addColumn("c_address",    DataType::STRING);
        s.addColumn("c_nationkey",  DataType::INT);
        s.addColumn("c_phone",      DataType::STRING);
        s.addColumn("c_acctbal",    DataType::FLOAT);
        s.addColumn("c_mktsegment", DataType::STRING);
        s.addColumn("c_comment",    DataType::STRING);
        return s;
    }

    // orders: INT, INT, STRING, FLOAT, STRING, STRING, INT
    // row size = 4 + 4 + 64 + 8 + 64 + 64 + 4 = 212 bytes
    static TableSchema ordersSchema() {
        TableSchema s;
        strncpy(s.tableName, "orders", TABLE_NAME_LEN - 1);
        s.addColumn("o_orderkey",     DataType::INT);
        s.addColumn("o_custkey",      DataType::INT);
        s.addColumn("o_orderstatus",  DataType::STRING);
        s.addColumn("o_totalprice",   DataType::FLOAT);
        s.addColumn("o_orderdate",    DataType::STRING);
        s.addColumn("o_orderpriority",DataType::STRING);
        s.addColumn("o_shippriority", DataType::INT);
        return s;
    }

    // lineitem: INT(PK), INT, INT, INT, FLOAT, FLOAT, FLOAT, FLOAT, STRING, STRING
    // row size = 4+4+4+4 + 8+8+8+8 + 64+64 = 176 bytes  (fits ~23 rows/page)
    static TableSchema lineitemSchema() {
        TableSchema s;
        strncpy(s.tableName, "lineitem", TABLE_NAME_LEN - 1);
        s.addColumn("l_linenumber",    DataType::INT);      // PK — must be first
        s.addColumn("l_orderkey",      DataType::INT);
        s.addColumn("l_partkey",       DataType::INT);
        s.addColumn("l_suppkey",       DataType::INT);
        s.addColumn("l_quantity",      DataType::FLOAT);
        s.addColumn("l_extendedprice", DataType::FLOAT);
        s.addColumn("l_discount",      DataType::FLOAT);
        s.addColumn("l_tax",           DataType::FLOAT);
        s.addColumn("l_returnflag",    DataType::STRING);
        s.addColumn("l_shipmode",      DataType::STRING);
        return s;
    }

    // ── Generators ───────────────────────────────────────────────────────────
    static void generateCustomers(NanoDB& db, int count) {
        Logger::log("DataGen: Generating %d customer records...", count);
        for (int i = 1; i <= count; i++) {
            Row row;
            row.addField(new IntValue(i));
            char name[64]; snprintf(name, 64, "Customer#%09d", i);
            row.addField(new StringValue(name));
            char addr[64]; snprintf(addr, 64, "Addr_%d_St", randInt(1,9999));
            row.addField(new StringValue(addr));
            row.addField(new IntValue(randInt(1, 25)));
            char phone[32]; snprintf(phone, 32, "%02d-%03d-%04d",
                                     randInt(10,99), randInt(100,999), randInt(1000,9999));
            row.addField(new StringValue(phone));
            row.addField(new FloatValue(randFloat(-999.99, 9999.99)));
            row.addField(new StringValue(pick(SEGMENTS, 5)));
            row.addField(new StringValue("NanoDB customer"));
            db.insertRow("customer", row);
            if (i % 5000 == 0)
                Logger::log("DataGen: %d customers inserted...", i);
        }
        Logger::log("DataGen: Customer generation complete.");
    }

    static void generateOrders(NanoDB& db, int count, int custCount) {
        Logger::log("DataGen: Generating %d order records...", count);
        for (int i = 1; i <= count; i++) {
            Row row;
            row.addField(new IntValue(i));
            row.addField(new IntValue(randInt(1, custCount)));
            row.addField(new StringValue(pick(ORDERSTATUS, 3)));
            row.addField(new FloatValue(randFloat(900.0, 500000.0)));
            char date[32]; snprintf(date, 32, "199%d-%02d-%02d",
                                    randInt(2,8), randInt(1,12), randInt(1,28));
            row.addField(new StringValue(date));
            row.addField(new StringValue(pick(PRIORITIES, 5)));
            row.addField(new IntValue(0));
            db.insertRow("orders", row);
            if (i % 5000 == 0)
                Logger::log("DataGen: %d orders inserted...", i);
        }
        Logger::log("DataGen: Orders generation complete.");
    }

    static void generateLineitems(NanoDB& db, int count, int orderCount) {
        Logger::log("DataGen: Generating %d lineitem records...", count);
        for (int i = 1; i <= count; i++) {
            Row row;
            row.addField(new IntValue(i));                                     // l_linenumber (PK)
            row.addField(new IntValue(randInt(1, orderCount)));                // l_orderkey
            row.addField(new IntValue(randInt(1, 200000)));                    // l_partkey
            row.addField(new IntValue(randInt(1, 10000)));                     // l_suppkey
            row.addField(new FloatValue(randFloat(1.0, 50.0)));                // l_quantity
            row.addField(new FloatValue(randFloat(100.0, 100000.0)));          // l_extendedprice
            row.addField(new FloatValue(randFloat(0.0, 0.1)));                 // l_discount
            row.addField(new FloatValue(randFloat(0.0, 0.08)));                // l_tax
            row.addField(new StringValue("N"));                                // l_returnflag
            row.addField(new StringValue(pick(SHIP_MODES, 7)));                // l_shipmode
            db.insertRow("lineitem", row);
            if (i % 10000 == 0)
                Logger::log("DataGen: %d lineitems inserted...", i);
        }
        Logger::log("DataGen: Lineitem generation complete.");
    }
};
