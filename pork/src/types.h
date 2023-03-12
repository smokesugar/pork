#pragma once

#include "base.h"
#include "set.h"

enum {
    TOKEN_EOF = 0,

    TOKEN_INT_LITERAL = 256,
    TOKEN_IDENTIFIER,

    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG_EQUAL,

    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,

    TOKEN_U64,
    TOKEN_U32,
    TOKEN_U16,
    TOKEN_U8,

    TOKEN_I64,
    TOKEN_I32,
    TOKEN_I16,
    TOKEN_I8,
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

    AST_CAST,

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

typedef enum {
    OP_TYPE_NONE,

    OP_U64, 
    OP_U32, 
    OP_U16, 
    OP_U8, 

    OP_I64, 
    OP_I32, 
    OP_I16, 
    OP_I8, 

    NUM_OP_TYPES
} OpType;

typedef struct Type Type;
struct Type {
    OpType op_type;
    u64 size;
};

typedef struct Variable Variable;
struct Variable {
    Variable* next;
    Token name;
    i64 reg;
    Type* type;
};

#define MAX_TYPE_COUNT 1024

typedef struct {
    u32 type_count;
    Type types[MAX_TYPE_COUNT];

    Type* type_void;
    Type* type_integer_literal;

    union {
        struct {
            Type* type_u64;
            Type* type_u32;
            Type* type_u16;
            Type* type_u8;

            Type* type_i64;
            Type* type_i32;
            Type* type_i16;
            Type* type_i8;
        };

        Type* integer_types[8];
    };
} Program;

typedef struct ASTNode ASTNode;
struct ASTNode {
    ASTKind kind;
    ASTNode* next;
    Token token;
    Type* type;

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

typedef struct {
    Type* return_type;
    ASTNode* body;
} ASTFunction;

#define MAX_INSTRUCTION_COUNT (1 << 13)
#define MAX_LABEL_COUNT (1 << 10)

typedef enum {
    OP_INVALID,
    OP_NOOP,

    OP_IMM,
    OP_COPY,
    OP_CAST,

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
    OpType type;
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
    int index;
    BasicBlock* next;

    bool has_user_code;
    bool reachable;
    int first_line;

    Set ue_var;
    Set var_kill;
    Set live_out;

    int start;
    int end;

    int successor_count;
    BasicBlock* successors[2];

    int predecessor_count;
    BasicBlock** predecessors;
};

ASTNode* new_ast_node(Arena* arena, ASTKind kind, Token token);
