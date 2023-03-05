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
