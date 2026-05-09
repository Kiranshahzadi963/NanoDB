#pragma once
#include <cstdio>
#include <cstring>
#include <climits>
#include "Logger.h"

// ─────────────────────────────────────────────────────────────────────────────
// Graph-based Join Optimizer
//
// Tables = nodes.  Estimated join cost between two tables = edge weight.
// We find the Minimum Spanning Tree (Prim's algorithm) to determine
// the cheapest multi-way join order.
// No STL — all adjacency matrix + raw arrays.
// ─────────────────────────────────────────────────────────────────────────────

static const int MAX_GRAPH_NODES = 16;

class JoinOptimizer {
    int   nodeCount;
    char  tableNames[MAX_GRAPH_NODES][TABLE_NAME_LEN];
    int   adjMatrix[MAX_GRAPH_NODES][MAX_GRAPH_NODES];   // cost matrix

    // Prim's internal arrays
    int   key[MAX_GRAPH_NODES];      // minimum edge weight to reach this node
    bool  inMST[MAX_GRAPH_NODES];    // is this node already in MST?
    int   parent[MAX_GRAPH_NODES];   // parent in MST

    int minKeyVertex() const {
        int minVal = INT_MAX, idx = -1;
        for (int v = 0; v < nodeCount; v++)
            if (!inMST[v] && key[v] < minVal) { minVal = key[v]; idx = v; }
        return idx;
    }

public:
    JoinOptimizer() : nodeCount(0) {
        for (int i = 0; i < MAX_GRAPH_NODES; i++)
            for (int j = 0; j < MAX_GRAPH_NODES; j++)
                adjMatrix[i][j] = INT_MAX;
    }

    int addTable(const char* name) {
        strncpy(tableNames[nodeCount], name, TABLE_NAME_LEN - 1);
        tableNames[nodeCount][TABLE_NAME_LEN - 1] = '\0';
        return nodeCount++;
    }

    void addEdge(int u, int v, int cost) {
        adjMatrix[u][v] = cost;
        adjMatrix[v][u] = cost;
    }

    // Estimate join cost between two tables (rows in A * rows in B / selectivity)
    // We use a simplified model: cost = sizeA * sizeB
    void setJoinCost(const char* tableA, const char* tableB, int cost) {
        int a = -1, b = -1;
        for (int i = 0; i < nodeCount; i++) {
            if (strcmp(tableNames[i], tableA) == 0) a = i;
            if (strcmp(tableNames[i], tableB) == 0) b = i;
        }
        if (a >= 0 && b >= 0) addEdge(a, b, cost);
    }

    // Run Prim's MST — returns ordered join path as table index array
    // outPath[] will contain node indices in MST traversal order
    int computeMST(int* outPath) {
        for (int i = 0; i < nodeCount; i++) {
            key[i]    = INT_MAX;
            inMST[i]  = false;
            parent[i] = -1;
        }
        key[0] = 0;  // start from node 0

        for (int iter = 0; iter < nodeCount; iter++) {
            int u = minKeyVertex();
            if (u < 0) break;
            inMST[u] = true;

            for (int v = 0; v < nodeCount; v++) {
                if (!inMST[v] && adjMatrix[u][v] != INT_MAX &&
                    adjMatrix[u][v] < key[v]) {
                    key[v]    = adjMatrix[u][v];
                    parent[v] = u;
                }
            }
        }

        // Reconstruct path: BFS-like traversal of MST edges from root
        // Simple approach: order by parent relationships
        bool visited[MAX_GRAPH_NODES] = {};
        int  pathLen = 0;

        // Root first
        outPath[pathLen++] = 0;
        visited[0] = true;

        for (int iter = 1; iter < nodeCount; iter++) {
            for (int v = 1; v < nodeCount; v++) {
                if (!visited[v] && parent[v] >= 0 && visited[parent[v]]) {
                    outPath[pathLen++] = v;
                    visited[v] = true;
                }
            }
        }

        // Build and log the MST path
        char pathStr[512]; pathStr[0] = '\0';
        for (int i = 0; i < pathLen; i++) {
            strcat(pathStr, tableNames[outPath[i]]);
            if (i < pathLen - 1) strcat(pathStr, " -> ");
        }
        Logger::log("Optimizer: Multi-table join routed via MST: %s", pathStr);

        // Log MST edges
        for (int v = 1; v < nodeCount; v++) {
            if (parent[v] >= 0) {
                Logger::log("Optimizer: MST edge [%s -- %s] cost=%d",
                            tableNames[parent[v]], tableNames[v], adjMatrix[parent[v]][v]);
            }
        }

        return pathLen;
    }

    int getNodeCount() const { return nodeCount; }
    const char* getTableName(int idx) const { return tableNames[idx]; }
};
