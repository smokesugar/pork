#pragma once

#include "base.h"

enum {
    TOKEN_EOF = 0,

    TOKEN_INT_LITERAL = 256,
    TOKEN_IDENTIFIER,

    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG_EQUAL,

    TOKEN_RETURN,
    TOKEN_LET
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
    AST_VARIABLE,

    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,

    AST_LESS,
    AST_LEQUAL,
    AST_EQUAL,
    AST_NEQUAL,

    AST_ASSIGN,

    AST_BLOCK,
    AST_RETURN,
    AST_VARIABLE_DECL,

    NUM_AST_KINDS,
} ASTKind;

typedef struct Variable Variable;
struct Variable {
    Variable* next;
    Token name;
    i64 reg;
};

typedef struct ASTNode ASTNode;
struct ASTNode {
    ASTKind kind;
    ASTNode* next;
    Token token;

    union {
        u64 int_literal;
        struct {
            ASTNode* left;           
            ASTNode* right;
        };
        ASTNode* first;
        ASTNode* expression;
        Token name;
        Variable* variable;
    };
};

#define MAX_INSTRUCTION_COUNT (1 << 13)

typedef enum {
    OP_INVALID,

    OP_IMM,
    OP_COPY,

    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,

    OP_LESS,
    OP_LEQUAL,
    OP_EQUAL,
    OP_NEQUAL,

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
