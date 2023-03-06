#include "bytecode.h"

internal void emit(Bytecode* bytecode, Op op, i64 a1, i64 a2, i64 a3) {
    assert(bytecode->length < MAX_INSTRUCTION_COUNT);
    assert(op);

    Instruction* ins = bytecode->instructions + (bytecode->length++);
    ins->op = op;
    ins->a1 = a1;
    ins->a2 = a2;
    ins->a3 = a3;
}

internal i64 get_reg(Bytecode* bytecode) {
    return bytecode->register_count++;
}

internal i64 translate(Bytecode* bytecode, ASTNode* node) {
    static_assert(NUM_AST_KINDS == 15, "not all ast kinds handled");
    switch (node->kind)
    {
        default:
            assert(false);
            return 0;

        case AST_INT_LITERAL: {
            i64 result = get_reg(bytecode);
            emit(bytecode, OP_IMM, result, node->int_literal, 0);
            return result;
        }

        case AST_VARIABLE: {
            return node->variable->reg;
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
            i64 left = translate(bytecode, node->left);
            i64 right = translate(bytecode, node->right);
            i64 result = get_reg(bytecode);

            Op op = 0;

            switch (node->kind) {
                default:
                    assert(false);
                    break;
                case AST_ADD:
                    op = OP_ADD;
                    break;
                case AST_SUB:
                    op = OP_SUB;
                    break;
                case AST_MUL:
                    op = OP_MUL;
                    break;
                case AST_DIV:
                    op = OP_DIV;
                    break;
                case AST_LESS:
                    op = OP_LESS;
                    break;
                case AST_LEQUAL:
                    op = OP_LEQUAL;
                    break;
                case AST_EQUAL:
                    op = OP_EQUAL;
                    break;
                case AST_NEQUAL:
                    op = OP_NEQUAL;
                    break;
            }

            emit(bytecode, op, result, left, right);
            return result;
        }

        case AST_ASSIGN: {
            i64 result = translate(bytecode, node->right);
            emit(bytecode, OP_COPY, node->left->variable->reg, result, 0);
            return result;
        }

        case AST_BLOCK:
            for (ASTNode* statement = node->first; statement; statement = statement->next) {
                translate(bytecode, statement);
            }
            return -1;
            
        case AST_RETURN: {
            i64 result = translate(bytecode, node->expression);
            emit(bytecode, OP_RET, result, 0, 0);
            return -1;
        }

        case AST_VARIABLE_DECL: {
            node->variable->reg = get_reg(bytecode);
            return -1;
        }
    }
}

Bytecode* generate_bytecode(Arena* arena, ASTNode* ast) {
    Bytecode* bytecode = arena_push_type(arena, Bytecode);
    translate(bytecode, ast);
    return bytecode;
}
