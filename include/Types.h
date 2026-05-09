#pragma once
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// DataType enum — every column in NanoDB has one of these types
// ─────────────────────────────────────────────────────────────────────────────
enum class DataType { INT, FLOAT, STRING };

// ─────────────────────────────────────────────────────────────────────────────
// FieldValue — polymorphic base class for a single cell value.
// Virtual destructor + compare() allow type-safe dispatch without STL.
// ─────────────────────────────────────────────────────────────────────────────
class FieldValue {
public:
    DataType type;
    explicit FieldValue(DataType t) : type(t) {}
    virtual ~FieldValue() {}

    // Returns <0, 0, >0 like strcmp / qsort comparator
    virtual int compare(const FieldValue* other) const = 0;

    // Human-readable print
    virtual void print() const = 0;

    // Deep copy — caller owns the result
    virtual FieldValue* clone() const = 0;

    // Serialize into buf (returns bytes written)
    virtual int serialize(char* buf) const = 0;

    // Arithmetic helpers (used by expression evaluator)
    virtual double toDouble() const = 0;

    // Operator overloads — delegate to compare()
    bool operator==(const FieldValue& o) const { return compare(&o) == 0; }
    bool operator!=(const FieldValue& o) const { return compare(&o) != 0; }
    bool operator< (const FieldValue& o) const { return compare(&o) <  0; }
    bool operator> (const FieldValue& o) const { return compare(&o) >  0; }
    bool operator<=(const FieldValue& o) const { return compare(&o) <= 0; }
    bool operator>=(const FieldValue& o) const { return compare(&o) >= 0; }
};

// ─────────────────────────────────────────────────────────────────────────────
// IntValue
// ─────────────────────────────────────────────────────────────────────────────
class IntValue : public FieldValue {
public:
    int val;
    explicit IntValue(int v = 0) : FieldValue(DataType::INT), val(v) {}

    int compare(const FieldValue* o) const override {
        const IntValue* ov = static_cast<const IntValue*>(o);
        return val - ov->val;
    }
    void print() const override { printf("%d", val); }
    FieldValue* clone() const override { return new IntValue(val); }
    double toDouble() const override { return (double)val; }

    int serialize(char* buf) const override {
        memcpy(buf, &val, sizeof(int));
        return sizeof(int);
    }
    static IntValue* deserialize(const char* buf) {
        int v; memcpy(&v, buf, sizeof(int));
        return new IntValue(v);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// FloatValue
// ─────────────────────────────────────────────────────────────────────────────
class FloatValue : public FieldValue {
public:
    double val;
    explicit FloatValue(double v = 0.0) : FieldValue(DataType::FLOAT), val(v) {}

    int compare(const FieldValue* o) const override {
        const FloatValue* ov = static_cast<const FloatValue*>(o);
        if (val < ov->val) return -1;
        if (val > ov->val) return  1;
        return 0;
    }
    void print() const override { printf("%.2f", val); }
    FieldValue* clone() const override { return new FloatValue(val); }
    double toDouble() const override { return val; }

    int serialize(char* buf) const override {
        memcpy(buf, &val, sizeof(double));
        return sizeof(double);
    }
    static FloatValue* deserialize(const char* buf) {
        double v; memcpy(&v, buf, sizeof(double));
        return new FloatValue(v);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// StringValue  (fixed 64-byte internal storage)
// ─────────────────────────────────────────────────────────────────────────────
static const int STR_CAP = 64;
class StringValue : public FieldValue {
public:
    char val[STR_CAP];
    explicit StringValue(const char* s = "") : FieldValue(DataType::STRING) {
        strncpy(val, s, STR_CAP - 1);
        val[STR_CAP - 1] = '\0';
    }

    int compare(const FieldValue* o) const override {
        const StringValue* ov = static_cast<const StringValue*>(o);
        return strcmp(val, ov->val);
    }
    void print() const override { printf("\"%s\"", val); }
    FieldValue* clone() const override { return new StringValue(val); }
    double toDouble() const override { return 0.0; }

    int serialize(char* buf) const override {
        memcpy(buf, val, STR_CAP);
        return STR_CAP;
    }
    static StringValue* deserialize(const char* buf) {
        return new StringValue(buf);   // buf must be STR_CAP bytes
    }
};
