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
    TOKEN_LET,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
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
    AST_IF,
    AST_WHILE,

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
        struct {
            ASTNode* condition;
            ASTNode* block_then;
            ASTNode* block_else;
        } conditional;
    };
};

#define MAX_INSTRUCTION_COUNT (1 << 13)
#define MAX_LABEL_COUNT (1 << 10)

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
    OP_JMP,
    OP_CJMP,

    NUM_OPS
} Op;

typedef struct {
    Op op;
    i64 a1;
    i64 a2;
    i64 a3;
    int label;
    int line;
} Instruction;

typedef struct {
    int length;
    Instruction instructions[MAX_INSTRUCTION_COUNT];

    int label_count;
    int label_locations[MAX_LABEL_COUNT];

    i64 register_count;
} Bytecode;

typedef struct BasicBlock BasicBlock;
struct BasicBlock {
    BasicBlock* next;
    bool has_user_code;
    bool reachable;
    int first_line;

    int start;
    int end;

    int successor_count;
    BasicBlock** successors;
};
