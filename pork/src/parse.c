#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "lexer.h"
#include "error.h"

typedef struct {
    char* source;
    Arena* arena;
    Lexer* lexer;
    Program* program;
} Parser; 

internal ASTNode* new_node(Parser* parser, ASTKind kind, Token token) {
    ASTNode* node = arena_push_type(parser->arena, ASTNode);
    node->kind = kind;
    node->token = token;
    return node;
}

internal bool match(Parser* parser, int kind, char* description) {
    Token token = peek_token(parser->lexer);

    if (token.kind == kind) {
        get_token(parser->lexer);
        return true;
    }

    error_at_token(parser->source, token, "expected %s", description);
    return false;
}

#define CONSUME(kind, description) if (!match(parser, kind, description)) { return 0; }

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

        case TOKEN_IDENTIFIER: {
            get_token(parser->lexer);
            ASTNode* node = new_node(parser, AST_VARIABLE, token);
            node->name = token;
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
        case '<':
        case '>':
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER_EQUAL:
            return 7;
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_BANG_EQUAL:
            return 5;
    }
}

internal ASTKind binary_ast_kind(Token op, bool* swap) {
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
        case '<':
            return AST_LESS;
        case '>':
            *swap = true;
            return AST_LESS;
        case TOKEN_LESS_EQUAL:
            return AST_LEQUAL;
        case TOKEN_GREATER_EQUAL:
            *swap = true;
            return AST_LEQUAL;
        case TOKEN_EQUAL_EQUAL:
            return AST_EQUAL;
        case TOKEN_BANG_EQUAL:
            return AST_NEQUAL;
    }
}

internal ASTNode* parse_binary(Parser* parser, int caller_precedence) {
    ASTNode* left = parse_primary(parser);
    if (!left) return 0;

    while (binary_precedence(peek_token(parser->lexer)) > caller_precedence) {
        Token op = get_token(parser->lexer);
        
        ASTNode* right = parse_binary(parser, binary_precedence(op));
        if (!right) return 0;

        bool swap = false;
        ASTNode* binary = new_node(parser, binary_ast_kind(op, &swap), op);
        binary->left = swap ? right : left;
        binary->right = swap ? left : right;

        left = binary;
    }

    return left;
}

internal ASTNode* parse_assign(Parser* parser) {
    ASTNode* left = parse_binary(parser, 0);
    if (!left) return 0;

    if (peek_token(parser->lexer).kind == '=') {
        Token equal_token = get_token(parser->lexer);

        ASTNode* right = parse_assign(parser);
        if (!right) return 0;

        ASTNode* assign = new_node(parser, AST_ASSIGN, equal_token);
        assign->left = left;
        assign->right = right;

        left = assign;
    }

    return left;
}

internal ASTNode* parse_expression(Parser* parser) {
    return parse_assign(parser);
}

internal ASTNode* parse_statement(Parser* parser);

internal ASTNode* parse_block(Parser* parser) {
    Token lbrace_token = peek_token(parser->lexer);
    CONSUME('{', "{");

    ASTNode head = {0};
    ASTNode* cur = &head;

    while (peek_token(parser->lexer).kind != '}' && peek_token(parser->lexer).kind != TOKEN_EOF) {
        ASTNode* statement = parse_statement(parser);
        if (!statement) return 0;

        cur->next = statement;
        while (cur->next) {
            cur = cur->next;
        }
    }

    CONSUME('}', "}");
    ASTNode* block = new_node(parser, AST_BLOCK, lbrace_token);
    block->first = head.next;

    return block;
}

/*
internal bool token_is(Token token, char* string) {
    return token.length == strlen(string) && memcmp(token.memory, string, token.length) == 0;
}
*/

internal Type* find_type(Parser* parser, Token type_name) {
    assert(parser->program->type_void && "program structure not initialized");

    if (type_name.kind == TOKEN_U64) {
        return parser->program->type_u64;
    }

    error_at_token(parser->source, type_name, "unrecognized format");
    return parser->program->type_void;
}

internal ASTNode* parse_statement(Parser* parser) {
    Token token = peek_token(parser->lexer);

    switch (token.kind) {
        default: {
            ASTNode* expression = parse_expression(parser);
            if (!expression) return 0;
            CONSUME(';', ";");
            return expression;
        } 

        case '{':
            return parse_block(parser);

        case TOKEN_RETURN: {
            get_token(parser->lexer);
            ASTNode* expression = parse_expression(parser);
            if (!expression) return 0;
            CONSUME(';', ";");
            ASTNode* ret = new_node(parser, AST_RETURN, token);
            ret->expression = expression;
            return ret;
        }

        case TOKEN_U64:
        {
            get_token(parser->lexer);
            Token name = peek_token(parser->lexer);
            CONSUME(TOKEN_IDENTIFIER, "an identifier");

            ASTNode* assign = 0;
            if (peek_token(parser->lexer).kind == '=') {
                jump_to_token(parser->lexer, name);
                assign = parse_assign(parser);
                if (!assign) return 0;
            }

            CONSUME(';', ";");
            
            ASTNode* decl = new_node(parser, AST_VARIABLE_DECL, token);
            decl->name = name;
            decl->next = assign;
            decl->type = find_type(parser, token);

            return decl;
        }

        case TOKEN_IF: {
            get_token(parser->lexer);

            ASTNode* condition = parse_expression(parser);
            if (!condition) return 0;

            ASTNode* block_then = parse_block(parser);
            if (!block_then) return 0;

            ASTNode* block_else = 0;
            if (peek_token(parser->lexer).kind == TOKEN_ELSE) {
                get_token(parser->lexer);
                block_else = parse_block(parser);
                if (!block_else) return 0;
            }

            ASTNode* if_statement = new_node(parser, AST_IF, token);
            if_statement->conditional.condition = condition;
            if_statement->conditional.block_then = block_then;
            if_statement->conditional.block_else = block_else;

            return if_statement;
        }

        case TOKEN_WHILE: {
            get_token(parser->lexer);

            ASTNode* condition = parse_expression(parser);
            if (!condition) return 0;

            ASTNode* body = parse_block(parser);
            if (!body) return 0;

            ASTNode* while_statement = new_node(parser, AST_WHILE, token);
            while_statement->conditional.condition = condition;
            while_statement->conditional.block_then = body;

            return while_statement;
        }
    }
}

ASTNode* parse(Arena* arena, char* source, Program* program) {
    Lexer lexer = init_lexer(source);

    Parser parser = {
        .source = source,
        .arena = arena,
        .lexer = &lexer,
        .program = program
    };

    return parse_block(&parser);
}
