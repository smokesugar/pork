#include <stdio.h>

#include "bytecode.h"
#include "error.h"

#define MAX_LABELS 1024

typedef struct {
    Bytecode* bytecode;
    int label_count;
    int label_locations[MAX_LABELS];
} Translator;

internal void emit(Translator* translator, Op op, i64 a1, i64 a2, i64 a3, int line) {
    assert(translator->bytecode->length < MAX_INSTRUCTION_COUNT);
    assert(op);

    Instruction* ins = translator->bytecode->instructions + (translator->bytecode->length++);
    ins->op = op;
    ins->a1 = a1;
    ins->a2 = a2;
    ins->a3 = a3;
    ins->label = -1;
    ins->line = line;
}

internal i64 get_reg(Translator* translator) {
    return translator->bytecode->register_count++;
}

internal int get_label(Translator* translator) {
    assert(translator->label_count < MAX_LABELS);
    return translator->label_count++;
}

internal void place_label(Translator* translator, int label) {
    assert(label < translator->label_count);
    translator->label_locations[label] = translator->bytecode->length;
}

internal i64 translate(Translator* translator, ASTNode* node) {
    static_assert(NUM_AST_KINDS == 17, "not all ast kinds handled");
    switch (node->kind)
    {
        default:
            assert(false);
            return 0;

        case AST_INT_LITERAL: {
            i64 result = get_reg(translator);
            emit(translator, OP_IMM, result, node->int_literal, 0, node->token.line);
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
            i64 left = translate(translator, node->left);
            i64 right = translate(translator, node->right);
            i64 result = get_reg(translator);

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

            emit(translator, op, result, left, right, node->token.line);
            return result;
        }

        case AST_ASSIGN: {
            i64 result = translate(translator, node->right);
            emit(translator, OP_COPY, node->left->variable->reg, result, 0, node->token.line);
            return result;
        }

        case AST_BLOCK:
            for (ASTNode* statement = node->first; statement; statement = statement->next) {
                translate(translator, statement);
            }
            return -1;
            
        case AST_RETURN: {
            i64 result = translate(translator, node->expression);
            emit(translator, OP_RET, result, 0, 0, node->token.line);
            return -1;
        }

        case AST_VARIABLE_DECL: {
            node->variable->reg = get_reg(translator);
            return -1;
        }

        case AST_IF: {
            bool has_else = node->conditional.block_else != 0;

            int label_then = get_label(translator);
            int label_else = get_label(translator);
            int label_end = 0;

            i64 condition = translate(translator, node->conditional.condition);
            emit(translator, OP_CJMP, condition, label_then, label_else, INT32_MAX);

            place_label(translator, label_then);
            translate(translator, node->conditional.block_then);

            if (has_else) {
                label_end = get_label(translator);
                emit(translator, OP_JMP, label_end, 0, 0, INT32_MAX);
            }

            place_label(translator, label_else);

            if (has_else) {
                translate(translator, node->conditional.block_else);
                place_label(translator, label_end);
            }

            return -1;
        }

        case AST_WHILE: {
            int label_start = get_label(translator);
            int label_body = get_label(translator);
            int label_end = get_label(translator);

            place_label(translator, label_start);
            i64 condition = translate(translator, node->conditional.condition);
            emit(translator, OP_CJMP, condition, label_body, label_end, INT32_MAX);

            place_label(translator, label_body);
            translate(translator, node->conditional.block_then);
            emit(translator, OP_JMP, label_start, 0, 0, INT32_MAX);

            place_label(translator, label_end);

            return -1;
        }
    }
}

Bytecode* generate_bytecode(Arena* arena, ASTNode* ast) {
    Bytecode* bytecode = arena_push_type(arena, Bytecode);

    Translator translator = {
        .bytecode = bytecode
    };

    translate(&translator, ast);

    // Remap labels to remove duplicates

    // Go through all labelled instructions and assign a new label
    for (int i = 0; i < translator.label_count; ++i)
    {
        int ins_index = translator.label_locations[i];
        if (ins_index < bytecode->length)
        {
            Instruction* ins = bytecode->instructions + ins_index;
            if (ins->label == -1) {
                assert(bytecode->label_count < MAX_LABEL_COUNT);
                ins->label = bytecode->label_count++;
                bytecode->label_locations[ins->label] = ins_index;
            }
        }
    }

    // End label
    assert(bytecode->label_count < MAX_LABEL_COUNT);
    bytecode->label_locations[bytecode->label_count++] = bytecode->length;

    // Set translator label locations to the new label index
    for (int i = 0; i < translator.label_count; ++i)
    {
        int ins_index = translator.label_locations[i];
        if (ins_index < bytecode->length)
        {
            Instruction* ins = bytecode->instructions + ins_index;
            translator.label_locations[i] = ins->label;
        }
        else {
            translator.label_locations[i] = bytecode->label_count-1;
        }
    }

    // Remap each instruction to point to new labels
    for (int i = 0; i < bytecode->length; ++i) {
        Instruction* ins = bytecode->instructions + i;
        
        switch (ins->op) {
            case OP_JMP:
                ins->a1 = translator.label_locations[ins->a1];
                break;
            case OP_CJMP:
                ins->a2 = translator.label_locations[ins->a2];
                ins->a3 = translator.label_locations[ins->a3];
                break;
        }
    }

    return bytecode;
}

internal BasicBlock* new_basic_block(Arena* arena, int start) {
    BasicBlock* block = arena_push_type(arena, BasicBlock);
    block->start = start;
    block->end = start;
    block->first_line = INT32_MAX;
    return block;
}

internal void mark_reachable(BasicBlock* block) {
    if (!block->reachable)
    {
        block->reachable = true;
        for (int i = 0; i < block->successor_count; ++i) {
            mark_reachable(block->successors[i]);
        }
    }
}

BasicBlock* analyze_control_flow(Arena* arena, char* source, Bytecode* bytecode) {
    BasicBlock* root = new_basic_block(arena, 0);
    BasicBlock* current = root;

    bool start_new_block = false;

    BasicBlock end_block = {0};
    BasicBlock* labelled_blocks[MAX_LABELS] = {0};
    labelled_blocks[bytecode->label_count-1] = &end_block;

    int index_counter = 1;
    for (int i = 0; i < bytecode->length; ++i) {
        Instruction* ins = bytecode->instructions + i;

        if (ins->label != -1 || start_new_block) {
            start_new_block = false;
            current = current->next = new_basic_block(arena, i);
            current->index = index_counter++;

            if (ins->label != -1) {
                labelled_blocks[ins->label] = current;
            }
        }

        ++current->end;

        if (ins->op != OP_CJMP && ins->op != OP_JMP) {
            current->has_user_code = true;
            current->first_line = ins->line < current->first_line ? ins->line : current->first_line;
        }

        switch (ins->op) {
            case OP_JMP:
            case OP_CJMP:
            case OP_RET:
                start_new_block = true;
                break;
        }
    }

    for (BasicBlock* block = root; block; block = block->next) {
        if (block->end == block->start)
            continue;

        Instruction* ins = bytecode->instructions + (block->end-1);
        switch (ins->op) {
            default:
                block->successors[0] = block->next ? block->next : &end_block;
                ++block->successor_count;
                break;

            case OP_RET:
                break;

            case OP_JMP:
                block->successors[0] = labelled_blocks[ins->a1];
                ++block->successor_count;
                break;

            case OP_CJMP:
                block->successors[0] = labelled_blocks[ins->a2];
                ++block->successor_count;
                if (labelled_blocks[ins->a3] != block->successors[0]) {
                    block->successors[1] = labelled_blocks[ins->a3];
                    ++block->successor_count;
                }
                else {
                    printf("Successors both same.\n");
                }
                break;
        }
    }

    mark_reachable(root);

    bool success = true;

    if (end_block.reachable) {
        printf("Not all control paths return.\n");
        success = false;
    }

    for (BasicBlock* block = root; block; block = block->next) {
        if (block->has_user_code && !block->reachable) {
            error_on_line(source, block->first_line, "Unreachable code");
            success = false;
        }

        for (int i = block->successor_count - 1; i >= 0; --i) {
            if (block->successors[i] == &end_block) {
                block->successors[i] = block->successors[--block->successor_count];
            }
            else {
                ++block->successors[i]->predecessor_count;
            }
        }
    }

    if (!success) {
        return 0;
    }

    for (BasicBlock* block = root; block; block = block->next) {
        block->predecessors = arena_push_array(arena, BasicBlock*, block->predecessor_count);
        block->predecessor_count = 0;
    }

    for (BasicBlock* block = root; block; block = block->next) {
        for (int i = 0; i < block->successor_count; ++i) {
            BasicBlock* successor = block->successors[i];
            successor->predecessors[successor->predecessor_count++] = block;
        }
    }

    return root;
}
