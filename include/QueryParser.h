#pragma once
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include "Structures.h"
#include "Schema.h"
#include "Logger.h"

// ─────────────────────────────────────────────────────────────────────────────
// Token types for the expression parser
// ─────────────────────────────────────────────────────────────────────────────
enum class TokenType {
    COLUMN, INT_LIT, FLOAT_LIT, STRING_LIT,
    OP_GT, OP_LT, OP_GTE, OP_LTE, OP_EQ, OP_NEQ,
    OP_AND, OP_OR, OP_NOT,
    OP_ADD, OP_SUB, OP_MUL, OP_MOD,
    LPAREN, RPAREN,
    UNKNOWN
};

struct Token {
    TokenType type;
    char      raw[128];   // original text

    Token() : type(TokenType::UNKNOWN) { raw[0] = '\0'; }
    Token(TokenType t, const char* r) : type(t) {
        strncpy(raw, r, 127); raw[127] = '\0';
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Tokenizer — splits a WHERE clause string into Token array
// ─────────────────────────────────────────────────────────────────────────────
static const int MAX_TOKENS = 256;

int tokenize(const char* expr, Token* tokens) {
    int  count = 0;
    int  i     = 0;
    int  len   = strlen(expr);

    while (i < len && count < MAX_TOKENS) {
        // Skip whitespace
        if (isspace(expr[i])) { i++; continue; }

        // String literal
        if (expr[i] == '"' || expr[i] == '\'') {
            char quote = expr[i++];
            char buf[128]; int bi = 0;
            while (i < len && expr[i] != quote && bi < 127) buf[bi++] = expr[i++];
            buf[bi] = '\0'; if (i < len) i++;
            tokens[count++] = Token(TokenType::STRING_LIT, buf);
            continue;
        }

        // Two-char operators
        if (i + 1 < len) {
            if (expr[i]=='>' && expr[i+1]=='=') { tokens[count++]=Token(TokenType::OP_GTE,">="); i+=2; continue; }
            if (expr[i]=='<' && expr[i+1]=='=') { tokens[count++]=Token(TokenType::OP_LTE,"<="); i+=2; continue; }
            if (expr[i]=='=' && expr[i+1]=='=') { tokens[count++]=Token(TokenType::OP_EQ,"==");  i+=2; continue; }
            if (expr[i]=='!' && expr[i+1]=='=') { tokens[count++]=Token(TokenType::OP_NEQ,"!="); i+=2; continue; }
        }

        // Single-char operators / parens
        if (expr[i]=='>') { tokens[count++]=Token(TokenType::OP_GT,">");  i++; continue; }
        if (expr[i]=='<') { tokens[count++]=Token(TokenType::OP_LT,"<");  i++; continue; }
        if (expr[i]=='(') { tokens[count++]=Token(TokenType::LPAREN,"("); i++; continue; }
        if (expr[i]==')') { tokens[count++]=Token(TokenType::RPAREN,")"); i++; continue; }
        if (expr[i]=='+') { tokens[count++]=Token(TokenType::OP_ADD,"+"); i++; continue; }
        if (expr[i]=='-') { tokens[count++]=Token(TokenType::OP_SUB,"-"); i++; continue; }
        if (expr[i]=='*') { tokens[count++]=Token(TokenType::OP_MUL,"*"); i++; continue; }
        if (expr[i]=='%') { tokens[count++]=Token(TokenType::OP_MOD,"%"); i++; continue; }

        // Number literal
        if (isdigit(expr[i]) || (expr[i]=='-' && i+1<len && isdigit(expr[i+1]))) {
            char buf[64]; int bi = 0;
            if (expr[i]=='-') buf[bi++]=expr[i++];
            bool isFloat = false;
            while (i < len && (isdigit(expr[i]) || expr[i]=='.')) {
                if (expr[i]=='.') isFloat=true;
                buf[bi++]=expr[i++];
            }
            buf[bi]='\0';
            tokens[count++]=Token(isFloat ? TokenType::FLOAT_LIT : TokenType::INT_LIT, buf);
            continue;
        }

        // Word (column name, AND, OR, NOT)
        if (isalpha(expr[i]) || expr[i]=='_') {
            char buf[128]; int bi=0;
            while (i<len && (isalnum(expr[i])||expr[i]=='_')) buf[bi++]=expr[i++];
            buf[bi]='\0';
            TokenType t = TokenType::COLUMN;
            if (strcmp(buf,"AND")==0||strcmp(buf,"and")==0) t=TokenType::OP_AND;
            else if (strcmp(buf,"OR")==0||strcmp(buf,"or")==0) t=TokenType::OP_OR;
            else if (strcmp(buf,"NOT")==0||strcmp(buf,"not")==0) t=TokenType::OP_NOT;
            tokens[count++]=Token(t, buf);
            continue;
        }

        i++; // skip unknown char
    }
    return count;
}

// ─────────────────────────────────────────────────────────────────────────────
// Operator precedence (higher = binds tighter)
// ─────────────────────────────────────────────────────────────────────────────
int precedence(TokenType t) {
    switch (t) {
        case TokenType::OP_OR:  return 1;
        case TokenType::OP_AND: return 2;
        case TokenType::OP_NOT: return 3;
        case TokenType::OP_EQ: case TokenType::OP_NEQ:
        case TokenType::OP_GT: case TokenType::OP_LT:
        case TokenType::OP_GTE: case TokenType::OP_LTE: return 4;
        case TokenType::OP_ADD: case TokenType::OP_SUB: return 5;
        case TokenType::OP_MUL: case TokenType::OP_MOD: return 6;
        default: return 0;
    }
}

bool isOperator(TokenType t) {
    return t==TokenType::OP_AND || t==TokenType::OP_OR  || t==TokenType::OP_NOT ||
           t==TokenType::OP_GT  || t==TokenType::OP_LT  || t==TokenType::OP_GTE ||
           t==TokenType::OP_LTE || t==TokenType::OP_EQ  || t==TokenType::OP_NEQ ||
           t==TokenType::OP_ADD || t==TokenType::OP_SUB || t==TokenType::OP_MUL ||
           t==TokenType::OP_MOD;
}

// ─────────────────────────────────────────────────────────────────────────────
// Infix → Postfix (Shunting-Yard) using CustomStack
// Returns postfix token array, sets postfixCount
// ─────────────────────────────────────────────────────────────────────────────
int infixToPostfix(const Token* infix, int inCount,
                   Token* postfix)
{
    CustomStack<Token> opStack;
    int outCount = 0;

    // Build human-readable infix string for logging
    char infixStr[512]; infixStr[0]='\0';
    for (int i=0; i<inCount; i++) {
        strcat(infixStr, infix[i].raw);
        if (i<inCount-1) strcat(infixStr, " ");
    }

    for (int i = 0; i < inCount; i++) {
        const Token& tok = infix[i];

        if (tok.type==TokenType::COLUMN   ||
            tok.type==TokenType::INT_LIT  ||
            tok.type==TokenType::FLOAT_LIT||
            tok.type==TokenType::STRING_LIT) {
            postfix[outCount++] = tok;
        }
        else if (tok.type==TokenType::LPAREN) {
            opStack.push(tok);
        }
        else if (tok.type==TokenType::RPAREN) {
            while (!opStack.isEmpty() && opStack.peek().type!=TokenType::LPAREN)
                postfix[outCount++] = opStack.pop();
            if (!opStack.isEmpty()) opStack.pop(); // discard LPAREN
        }
        else if (isOperator(tok.type)) {
            while (!opStack.isEmpty() &&
                   opStack.peek().type!=TokenType::LPAREN &&
                   precedence(opStack.peek().type) >= precedence(tok.type))
                postfix[outCount++] = opStack.pop();
            opStack.push(tok);
        }
    }
    while (!opStack.isEmpty()) postfix[outCount++] = opStack.pop();

    // Build postfix string for logging
    char postfixStr[512]; postfixStr[0]='\0';
    for (int i=0; i<outCount; i++) {
        strcat(postfixStr, postfix[i].raw);
        if (i<outCount-1) strcat(postfixStr, " ");
    }

    Logger::log("Parser: Infix \"%s\" converted to Postfix \"%s\"",
                infixStr, postfixStr);

    return outCount;
}

// ─────────────────────────────────────────────────────────────────────────────
// Expression Evaluator — evaluates postfix tokens against a Row
// Returns 1 (true) or 0 (false)
// ─────────────────────────────────────────────────────────────────────────────

struct EvalValue {
    double  num;
    char    str[STR_CAP];
    bool    isStr;
    bool    boolVal;

    EvalValue() : num(0), isStr(false), boolVal(false) { str[0]='\0'; }
    EvalValue(double d) : num(d), isStr(false), boolVal(false) { str[0]='\0'; }
    EvalValue(const char* s) : num(0), isStr(true), boolVal(false) {
        strncpy(str, s, STR_CAP-1); str[STR_CAP-1]='\0';
    }
    EvalValue(bool b) : num(b?1:0), isStr(false), boolVal(b) { str[0]='\0'; }
};

bool evaluatePostfix(const Token* postfix, int count,
                     const Row& row, const TableSchema& schema)
{
    CustomStack<EvalValue> evalStack;

    for (int i = 0; i < count; i++) {
        const Token& tok = postfix[i];

        if (tok.type == TokenType::INT_LIT || tok.type == TokenType::FLOAT_LIT) {
            evalStack.push(EvalValue(atof(tok.raw)));
        }
        else if (tok.type == TokenType::STRING_LIT) {
            evalStack.push(EvalValue(tok.raw));
        }
        else if (tok.type == TokenType::COLUMN) {
            int ci = schema.colIndex(tok.raw);
            if (ci < 0) { evalStack.push(EvalValue(0.0)); continue; }
            FieldValue* fv = row.fields[ci];
            if (!fv) { evalStack.push(EvalValue(0.0)); continue; }
            if (fv->type == DataType::STRING) {
                evalStack.push(EvalValue(static_cast<StringValue*>(fv)->val));
            } else {
                evalStack.push(EvalValue(fv->toDouble()));
            }
        }
        else if (isOperator(tok.type)) {
            // Binary operators need two operands
            if (evalStack.size() < 2 && tok.type != TokenType::OP_NOT) {
                // Not enough operands — push false
                evalStack.push(EvalValue(false));
                continue;
            }

            if (tok.type == TokenType::OP_NOT) {
                EvalValue a = evalStack.pop();
                evalStack.push(EvalValue(!(bool)(a.num)));
                continue;
            }

            EvalValue b = evalStack.pop();
            EvalValue a = evalStack.pop();

            // Arithmetic operators → numeric result
            if (tok.type == TokenType::OP_ADD) { evalStack.push(EvalValue(a.num+b.num)); continue; }
            if (tok.type == TokenType::OP_SUB) { evalStack.push(EvalValue(a.num-b.num)); continue; }
            if (tok.type == TokenType::OP_MUL) { evalStack.push(EvalValue(a.num*b.num)); continue; }
            if (tok.type == TokenType::OP_MOD) {
                evalStack.push(EvalValue((double)((int)a.num % (int)b.num))); continue;
            }

            // Logical operators
            if (tok.type == TokenType::OP_AND) { evalStack.push(EvalValue((bool)(a.num)&&(bool)(b.num))); continue; }
            if (tok.type == TokenType::OP_OR)  { evalStack.push(EvalValue((bool)(a.num)||(bool)(b.num))); continue; }

            // Comparison operators
            bool result = false;
            if (a.isStr || b.isStr) {
                int cmp = strcmp(a.str, b.str);
                if (tok.type==TokenType::OP_EQ)  result=(cmp==0);
                if (tok.type==TokenType::OP_NEQ) result=(cmp!=0);
                if (tok.type==TokenType::OP_GT)  result=(cmp>0);
                if (tok.type==TokenType::OP_LT)  result=(cmp<0);
                if (tok.type==TokenType::OP_GTE) result=(cmp>=0);
                if (tok.type==TokenType::OP_LTE) result=(cmp<=0);
            } else {
                if (tok.type==TokenType::OP_EQ)  result=(a.num==b.num);
                if (tok.type==TokenType::OP_NEQ) result=(a.num!=b.num);
                if (tok.type==TokenType::OP_GT)  result=(a.num>b.num);
                if (tok.type==TokenType::OP_LT)  result=(a.num<b.num);
                if (tok.type==TokenType::OP_GTE) result=(a.num>=b.num);
                if (tok.type==TokenType::OP_LTE) result=(a.num<=b.num);
            }
            evalStack.push(EvalValue(result));
        }
    }

    if (evalStack.isEmpty()) return false;
    EvalValue res = evalStack.pop();
    return (bool)(res.num) || res.boolVal;
}
