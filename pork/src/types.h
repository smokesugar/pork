#pragma once

#include "base.h"

enum {
    TOKEN_EOF = 0,
    TOKEN_INT_LITERAL = 256,
};

typedef struct {
    int kind;
    char* memory;
    int length;
    int line;
} Token;

typedef enum {
    AST_UNINITIALIZED,

    AST_INT_LITERAL,

    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,

    NUM_AST_KINDS,
} ASTKind;

typedef struct ASTNode ASTNode;
struct ASTNode {
    ASTKind kind;
    Token token;

    union {
        u64 int_literal;
        struct {
            ASTNode* left;           
            ASTNode* right;
        };
    };
};

#define MAX_INSTRUCTION_COUNT (1 << 13)

typedef enum {
    OP_INVALID,

    OP_IMM,

    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,

    OP_RET,

    NUM_OPS
} Op;

typedef struct {
    Op op;
    i64 a1;
    i64 a2;
    i64 a3;
} Instruction;

typedef struct {
    int length;
    Instruction instructions[MAX_INSTRUCTION_COUNT];
    i64 register_count;
} Bytecode;
