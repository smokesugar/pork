#include <stdlib.h>

#include "parse.h"
#include "lexer.h"
#include "error.h"

typedef struct {
    char* source;
    Arena* arena;
    Lexer* lexer;
} Parser; 

internal ASTNode* new_node(Parser* parser, ASTKind kind, Token token) {
    ASTNode* node = arena_push_type(parser->arena, ASTNode);
    node->kind = kind;
    node->token = token;
    return node;
}

internal ASTNode* parse_primary(Parser* parser)
{
    Token token = peek_token(parser->lexer);
    switch (token.kind)
    {
        case TOKEN_INT_LITERAL: {
            get_token(parser->lexer);
            ASTNode* node = new_node(parser, AST_INT_LITERAL, token);
            node->int_literal = strtoull(token.memory, 0, 10);
            return node;
        }
    }

    error_at_token(parser->source, token, "expected an expression");
    return 0;
}

internal int binary_precedence(Token op) {
    switch (op.kind) {
        default:
            return 0;
        case '*':
        case '/':
            return 20;
        case '+':
        case '-':
            return 10;
    }
}

internal ASTKind binary_ast_kind(Token op) {
    switch (op.kind) {
        default:
            assert(false);
            return 0;
        case '*':
            return AST_MUL;
        case '/':
            return AST_DIV;
        case '+':
            return AST_ADD;
        case '-':
            return AST_SUB;
    }
}

internal ASTNode* parse_binary(Parser* parser, int caller_precedence) {
    ASTNode* left = parse_primary(parser);
    if (!left) return 0;

    while (binary_precedence(peek_token(parser->lexer)) > caller_precedence) {
        Token op = get_token(parser->lexer);
        
        ASTNode* right = parse_binary(parser, binary_precedence(op));
        if (!right) return 0;

        ASTNode* binary = new_node(parser, binary_ast_kind(op), op);
        binary->left = left;
        binary->right = right;

        left = binary;
    }

    return left;
}

internal ASTNode* parse_expression(Parser* parser) {
    return parse_binary(parser, 0);
}

ASTNode* parse(Arena* arena, char* source) {
    Lexer lexer = init_lexer(source);

    Parser parser = {
        .source = source,
        .arena = arena,
        .lexer = &lexer
    };

    return parse_expression(&parser);
}
