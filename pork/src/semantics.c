#include "semantics.h"
#include "error.h"

internal Type* new_type(Program* program, u64 size, OpType op_type) {
    assert(program->type_count < MAX_TYPE_COUNT);
    Type* type = program->types + (program->type_count++);
    type->op_type = op_type;
    type->size = size;
    return type;
}

void init_program(Program* program)
{
    program->type_void = new_type(program, 0, OP_TYPE_NONE);
    program->type_integer_literal = new_type(program, 0, OP_I64);

    program->type_u64  = new_type(program, 8, OP_U64);
    program->type_u32  = new_type(program, 4, OP_U32);
    program->type_u16  = new_type(program, 2, OP_U16);
    program->type_u8   = new_type(program, 1, OP_U8);

    program->type_i64  = new_type(program, 8, OP_I64);
    program->type_i32  = new_type(program, 4, OP_I32);
    program->type_i16  = new_type(program, 2, OP_I16);
    program->type_i8   = new_type(program, 1, OP_I8);
}

bool type_is_integral(Program* program, Type* type) {
    for (int i = 0; i < LENGTH(program->integer_types); ++i) {
        if (type == program->integer_types[i]) {
            return true;
        }
    }

    return false;
}

bool type_is_signed_integral(Program* program, Type* type) {
    for (int i = 0; i < 4; ++i) {
        if (type == program->integer_types[i]) {
            return false;
        }
    }

    for (int i = 4; i < 8; ++i) {
        if (type == program->integer_types[i]) {
            return true;
        }
    }

    assert(false && "not an integral type");
    return false;
}

Type* get_signed_integral_type(Program* program, Type* type) {
    for (int i = 0; i < 4; ++i) {
        if (type == program->integer_types[i]) {
            return program->integer_types[i + 4];
        }
    }

    for (int i = 4; i < 8; ++i) {
        if (type == program->integer_types[i]) {
            return type;
        }
    }

    assert(false && "not an integral type");
    return false;
}

Type* get_unsigned_integral_type(Program* program, Type* type) {
    for (int i = 0; i < 4; ++i) {
        if (type == program->integer_types[i]) {
            return type;
        }
    }

    for (int i = 4; i < 8; ++i) {
        if (type == program->integer_types[i]) {
            return program->integer_types[i - 4];
        }
    }

    assert(false && "not an integral type");
    return false;
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
    ASTFunction* ast_function;
} Analyzer;

internal ASTNode* clone_node(Arena* arena, ASTNode* node) {
    ASTNode* clone = arena_push_type(arena, ASTNode);
    memcpy(clone, node, sizeof(*node));
    return clone;
}

internal void set_subtree_integer_type(ASTNode* node, Type* type) {
    node->type = type;

    static_assert(NUM_AST_KINDS == 18, "not all ast kinds handled");
    switch (node->kind) {
        default:
            assert(false);
            break;

        case AST_INT_LITERAL:
            break;

        case AST_VARIABLE: // These nodes should not appear in an integer-literal-typed sub-tree.
        case AST_CAST:
        case AST_ASSIGN:
        case AST_BLOCK:
        case AST_RETURN:
        case AST_VARIABLE_DECL:
        case AST_IF:
        case AST_WHILE:
            assert(false);
            break;

        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
        case AST_LESS:
        case AST_LEQUAL:
        case AST_EQUAL:
        case AST_NEQUAL:
            set_subtree_integer_type(node->left, type);
            set_subtree_integer_type(node->right, type);
            break;
    }
}

internal void implicit_cast(Analyzer* analyzer, ASTNode* node, Type* type) {
    if (node->type != type) {
        if (node->type == analyzer->program->type_integer_literal) {
            set_subtree_integer_type(node, type);
        }
        else {
            ASTNode* clone = clone_node(analyzer->arena, node);
            node->kind = AST_CAST;
            node->type = type;
            node->expression = clone;
        }
    }
}

internal bool can_coerce_type(Program* program, Type* type, Type* wanted_type) {
    bool both_integral_and_wanted_is_larger = type_is_integral(program, wanted_type) && type_is_integral(program, type) && wanted_type->size >= type->size;
    bool wanted_integral_and_type_integer_literal = type_is_integral(program, wanted_type) && type == program->type_integer_literal;
    return both_integral_and_wanted_is_larger || wanted_integral_and_type_integer_literal;
}

internal bool process_ast(Analyzer* analyzer, Scope* scope, ASTNode* node) {
    Program* program = analyzer->program;

    static_assert(NUM_AST_KINDS == 18, "not all ast kinds handled");
    switch (node->kind) {
        default:
            assert(false);
            return false;

        case AST_INT_LITERAL:
            node->type = program->type_integer_literal;
            return true;

        case AST_VARIABLE: {
            Variable* variable = find_variable(scope, node->name);

            if (!variable) {
                error_at_token(analyzer->source, node->token, "undefined variable");
                node->type = program->type_void;
                return false;
            }

            node->variable = variable;
            node->type = variable->type;
            
            return true;
        }

        case AST_CAST:
            return process_ast(analyzer, scope, node->expression);

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
                bool left_is_integral = type_is_integral(program, node->left->type);
                bool right_is_integral = type_is_integral(program, node->right->type);

                if (left_is_integral && right_is_integral) // Implicitly cast the nodes to a common type.
                {
                    bool left_is_signed = type_is_signed_integral(program, node->left->type);
                    bool right_is_signed = type_is_signed_integral(program, node->right->type);

                    bool casted_type_is_signed = left_is_signed || right_is_signed;
                    Type* casted_type = node->left->type->size > node->right->type->size ? node->left->type : node->right->type;
                    casted_type = casted_type_is_signed ? get_signed_integral_type(program, casted_type) : get_unsigned_integral_type(program, casted_type);

                    implicit_cast(analyzer, node->left, casted_type);
                    implicit_cast(analyzer, node->right, casted_type);

                    node->type = casted_type;
                }
                else if(left_is_integral && node->right->type == program->type_integer_literal) {
                    set_subtree_integer_type(node->right, node->left->type);
                    node->type = node->left->type;
                }
                else if(right_is_integral && node->left->type == program->type_integer_literal) {
                    set_subtree_integer_type(node->left, node->right->type);
                    node->type = node->right->type;
                }
                else {
                    error_at_token(analyzer->source, node->token, "types of operands are invalid for this operation");
                    node->type = program->type_void;
                    success = false;
                }
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
                if (can_coerce_type(program, node->right->type, node->left->type)) {
                    implicit_cast(analyzer, node->right, node->left->type);
                    node->type = node->left->type;
                }
                else {
                    error_at_token(analyzer->source, node->token, "types of operands are invalid for this operation");
                    node->type = program->type_void;
                    success = false;
                }
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

        case AST_RETURN: {
            bool success = process_ast(analyzer, scope, node->expression);

            if (node->expression->type != analyzer->ast_function->return_type) {
                if (can_coerce_type(program, node->expression->type, analyzer->ast_function->return_type)) {
                    implicit_cast(analyzer, node->expression, analyzer->ast_function->return_type);
                    node->type = analyzer->ast_function->return_type;
                }
                else {
                    error_at_token(analyzer->source, node->token, "return type does not match the function signature");
                    success = false;
                    node->type = program->type_void;
                }
            }

            return success;
        }

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

bool analyze_semantics(Arena* arena, char* source, Program* program, ASTFunction* ast_function) {
    Analyzer analyzer = {
        .arena = arena,
        .source = source,
        .program = program,
        .ast_function = ast_function
    };

    ast_function->return_type = program->type_i32;

    return process_ast(&analyzer, 0, ast_function->body);
}
