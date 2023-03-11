#include "semantics.h"
#include "error.h"

void init_program(Program* program)
{
    program->type_void = program->types + (program->type_count++);
    program->type_u64  = program->types + (program->type_count++);
}

typedef struct Scope Scope;
struct Scope {
    Scope* parent;
    Variable* variables;
};

internal Variable* find_variable(Scope* scope, Token name) {
    for (Variable* v = scope->variables; v; v = v->next) {
        if (v->name.length == name.length && memcmp(v->name.memory, name.memory, name.length) == 0) {
            return v;
        }
    }

    if (scope->parent) {
        return find_variable(scope->parent, name);
    }

    return 0;
}

typedef struct {
    Arena* arena;
    char* source;
    Program* program;
} Analyzer;

internal bool process_ast(Analyzer* analyzer, Scope* scope, ASTNode* node) {
    static_assert(NUM_AST_KINDS == 17, "not all ast kinds handled");
    switch (node->kind) {
        default:
            assert(false);
            return false;

        case AST_INT_LITERAL:
            node->type = analyzer->program->type_u64;
            return true;

        case AST_VARIABLE: {
            Variable* variable = find_variable(scope, node->name);

            if (!variable) {
                error_at_token(analyzer->source, node->token, "undefined variable");
                node->type = analyzer->program->type_void;
                return false;
            }

            node->variable = variable;
            node->type = variable->type;
            
            return true;
        }

        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
        case AST_LESS:
        case AST_LEQUAL:
        case AST_EQUAL:
        case AST_NEQUAL:
        {
            bool success = true;

            success &= process_ast(analyzer, scope, node->left);
            success &= process_ast(analyzer, scope, node->right);

            if (node->left->type == node->right->type) {
                node->type = node->left->type;
            }
            else {
                error_at_token(analyzer->source, node->token, "types of operands are invalid for this operation");
                node->type = analyzer->program->type_void;
                success = false;
            }

            return success;
        }

        case AST_ASSIGN:
        {
            bool success = true;

            success &= process_ast(analyzer, scope, node->left);
            success &= process_ast(analyzer, scope, node->right);

            if (node->left->kind != AST_VARIABLE) {
                error_at_token(analyzer->source, node->left->token, "not assignable");
                success = false;
            }

            if (node->left->type == node->right->type) {
                node->type = node->left->type;
            }
            else {
                error_at_token(analyzer->source, node->token, "types of operands are invalid for this operation");
                node->type = analyzer->program->type_void;
                success = false;
            }

            return success;
        }

        case AST_BLOCK: {
            bool success = true; 

            Scope inner_scope = {
                .parent = scope,
            };

            for (ASTNode* child = node->first; child; child = child->next) {
                success &= process_ast(analyzer, &inner_scope, child);
            }

            return success;
        }

        case AST_RETURN:
            return process_ast(analyzer, scope, node->expression);

        case AST_VARIABLE_DECL: {
            if (find_variable(scope, node->name)) {
                error_at_token(analyzer->source, node->name, "variable redefinition");
                return false;
            }

            Variable* variable = arena_push_type(analyzer->arena, Variable);
            variable->name = node->name;
            variable->type = node->type;

            Variable** bucket = &scope->variables;
            while (*bucket) {
                bucket = &(*bucket)->next;
            }
            *bucket = variable;

            node->variable = variable;

            return true;
        }

        case AST_IF:
        case AST_WHILE:
        {
            bool success = true;

            success &= process_ast(analyzer, scope, node->conditional.condition);
            success &= process_ast(analyzer, scope, node->conditional.block_then);

            if (node->conditional.block_else) {
                success &= process_ast(analyzer, scope, node->conditional.block_else);
            }

            return success;
        }
    }
}

bool analyze_semantics(Arena* arena, char* source, Program* program, ASTNode* ast) {
    Analyzer analyzer = {
        .arena = arena,
        .source = source,
        .program = program
    };

    return process_ast(&analyzer, 0, ast);
}
