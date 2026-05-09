#pragma once
#include <cstdio>
#include <cstdlib>
#include "Logger.h"

// ─────────────────────────────────────────────────────────────────────────────
// AVL Tree — self-balancing BST, guarantees O(log N) search/insert/delete.
// Stores integer keys → row page/slot pairs.
// No STL used anywhere.
// ─────────────────────────────────────────────────────────────────────────────

struct AVLNode {
    int      key;
    int      pageId;   // which disk page this row lives on
    int      slot;     // which slot within that page
    int      height;
    AVLNode* left;
    AVLNode* right;

    AVLNode(int k, int pg, int sl)
        : key(k), pageId(pg), slot(sl), height(1), left(nullptr), right(nullptr) {}
};

class AVLTree {
    AVLNode* root;
    int      nodeCount;

    // ── Helper utilities ─────────────────────────────────────────────────────
    int height(AVLNode* n) const { return n ? n->height : 0; }

    void updateHeight(AVLNode* n) {
        if (!n) return;
        int lh = height(n->left), rh = height(n->right);
        n->height = 1 + (lh > rh ? lh : rh);
    }

    int balanceFactor(AVLNode* n) const {
        return n ? height(n->left) - height(n->right) : 0;
    }

    // ── Rotations ────────────────────────────────────────────────────────────
    AVLNode* rotateRight(AVLNode* y) {
        AVLNode* x  = y->left;
        AVLNode* T2 = x->right;
        x->right = y;
        y->left  = T2;
        updateHeight(y);
        updateHeight(x);
        return x;
    }

    AVLNode* rotateLeft(AVLNode* x) {
        AVLNode* y  = x->right;
        AVLNode* T2 = y->left;
        y->left  = x;
        x->right = T2;
        updateHeight(x);
        updateHeight(y);
        return y;
    }

    // ── Balance node after insert/delete ────────────────────────────────────
    AVLNode* balance(AVLNode* n) {
        updateHeight(n);
        int bf = balanceFactor(n);

        // Left-Left
        if (bf > 1 && balanceFactor(n->left) >= 0)
            return rotateRight(n);
        // Left-Right
        if (bf > 1 && balanceFactor(n->left) < 0) {
            n->left = rotateLeft(n->left);
            return rotateRight(n);
        }
        // Right-Right
        if (bf < -1 && balanceFactor(n->right) <= 0)
            return rotateLeft(n);
        // Right-Left
        if (bf < -1 && balanceFactor(n->right) > 0) {
            n->right = rotateRight(n->right);
            return rotateLeft(n);
        }
        return n;
    }

    // ── Recursive insert ─────────────────────────────────────────────────────
    AVLNode* insert(AVLNode* n, int key, int pageId, int slot) {
        if (!n) { nodeCount++; return new AVLNode(key, pageId, slot); }
        if (key < n->key)       n->left  = insert(n->left,  key, pageId, slot);
        else if (key > n->key)  n->right = insert(n->right, key, pageId, slot);
        else { n->pageId = pageId; n->slot = slot; return n; } // update duplicate
        return balance(n);
    }

    // ── Find min node (used in delete) ──────────────────────────────────────
    AVLNode* minNode(AVLNode* n) {
        while (n->left) n = n->left;
        return n;
    }

    // ── Recursive delete ─────────────────────────────────────────────────────
    AVLNode* remove(AVLNode* n, int key) {
        if (!n) return nullptr;
        if      (key < n->key) n->left  = remove(n->left,  key);
        else if (key > n->key) n->right = remove(n->right, key);
        else {
            if (!n->left || !n->right) {
                AVLNode* child = n->left ? n->left : n->right;
                delete n;
                nodeCount--;
                return child;
            }
            AVLNode* successor = minNode(n->right);
            n->key    = successor->key;
            n->pageId = successor->pageId;
            n->slot   = successor->slot;
            n->right  = remove(n->right, successor->key);
        }
        return balance(n);
    }

    // ── Recursive search ─────────────────────────────────────────────────────
    AVLNode* search(AVLNode* n, int key) const {
        if (!n)           return nullptr;
        if (key == n->key) return n;
        if (key < n->key)  return search(n->left,  key);
        return search(n->right, key);
    }

    // ── Free all nodes ───────────────────────────────────────────────────────
    void destroy(AVLNode* n) {
        if (!n) return;
        destroy(n->left);
        destroy(n->right);
        delete n;
    }

    // ── In-order traversal for debugging ────────────────────────────────────
    void inorder(AVLNode* n) const {
        if (!n) return;
        inorder(n->left);
        printf("  key=%d -> page=%d slot=%d\n", n->key, n->pageId, n->slot);
        inorder(n->right);
    }

public:
    AVLTree() : root(nullptr), nodeCount(0) {}
    ~AVLTree() { destroy(root); }

    void insert(int key, int pageId, int slot) {
        root = insert(root, key, pageId, slot);
    }

    void remove(int key) {
        root = remove(root, key);
    }

    // Returns nullptr if not found, else the node
    AVLNode* search(int key) const {
        return search(root, key);
    }

    int  size()         const { return nodeCount; }
    int  treeHeight()   const { return height(root); }
    void printInorder() const { inorder(root); }
};
