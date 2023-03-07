#include <stdio.h>

#include "base.h"
#include "parse.h"
#include "bytecode.h"
#include "set.h"

static Arena* scratch_arenas[2];

Scratch get_scratch(Arena* conflict)
{
    for (int i = 0; i < LENGTH(scratch_arenas); ++i)
    {
        Arena* arena = scratch_arenas[i];
        if (conflict != arena)
        {
            return (Scratch) {
                .arena = arena,
                .allocated = arena->allocated
            };
        }
    }

    assert(false);
    return (Scratch){0};
}

void release_scratch(Scratch* scratch) {
    assert(scratch->allocated <= scratch->arena->allocated);
    scratch->arena->allocated = scratch->allocated;
}

internal i64 eval(ASTNode* node) {
    switch (node->kind) {
        default:
            assert(false);
            return 0;
        case AST_INT_LITERAL:
            return node->int_literal;
        case AST_ADD:
            return eval(node->left) + eval(node->right);
        case AST_SUB:
            return eval(node->left) - eval(node->right);
        case AST_MUL:
            return eval(node->left) * eval(node->right);
        case AST_DIV:
            return eval(node->left) / eval(node->right);
    }
}

int main() {
    for (int i = 0; i < LENGTH(scratch_arenas); ++i) {
        scratch_arenas[i] = new_arena(5 * 1024 * 1024);
    }

    Arena* arena = new_arena(5 * 1024 * 1024);

    char* source_path = "examples/test.pork";

    FILE* file;
    if(fopen_s(&file, source_path, "r")) {
        printf("Failed to open '%s'\n", source_path);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    size_t file_length = ftell(file);
    rewind(file);

    char* source = arena_push(arena, file_length + 1);
    size_t source_length = fread(source, 1, file_length, file);
    source[source_length] = '\0';

    ASTNode* ast = parse(arena, source);
    if (!ast) return 1;

    Bytecode* bytecode = generate_bytecode(arena, ast);
    BasicBlock* cfg = analyze_control_flow(arena, source, bytecode);
    if (!cfg) return 1;
    analyze_data_flow(cfg, bytecode);

    for (BasicBlock* block = cfg; block; block = block->next) {
        printf("Block %d:\n", block->index);
        foreach_set(&block->live_out, x) {
            printf("  %lld\n", x.value);
        }
    }

    i64 regs[1024] = {0};

    for (int i = 0; i < bytecode->length;)
    {
        Instruction* ins = bytecode->instructions + i;

        static_assert(NUM_OPS == 14, "not all ops handled");
        switch (ins->op) {
            default:
                assert(false);
                break;

            case OP_IMM:
                regs[ins->a1] = ins->a2;
                break;
            case OP_COPY:
                regs[ins->a1] = regs[ins->a2];
                break;

            case OP_ADD:
                regs[ins->a1] = regs[ins->a2] + regs[ins->a3];
                break;
            case OP_SUB:
                regs[ins->a1] = regs[ins->a2] - regs[ins->a3];
                break;
            case OP_MUL:
                regs[ins->a1] = regs[ins->a2] * regs[ins->a3];
                break;
            case OP_DIV:
                regs[ins->a1] = regs[ins->a2] / regs[ins->a3];
                break;
            case OP_LESS:
                regs[ins->a1] = regs[ins->a2] < regs[ins->a3];
                break;
            case OP_LEQUAL:
                regs[ins->a1] = regs[ins->a2] <= regs[ins->a3];
                break;
            case OP_EQUAL:
                regs[ins->a1] = regs[ins->a2] == regs[ins->a3];
                break;
            case OP_NEQUAL:
                regs[ins->a1] = regs[ins->a2] != regs[ins->a3];
                break;

            case OP_RET:
                printf("Returned with %lld.\n", regs[ins->a1]);
                return 0;

            case OP_JMP: {
                i64 label = ins->a1;
                i = bytecode->label_locations[label];
                continue;
            }

            case OP_CJMP: {
                i64 label = regs[ins->a1] ? ins->a2 : ins->a3;
                i = bytecode->label_locations[label];
                continue;
            }
        }

        ++i;
    }

    printf("No return.\n");
    return 1;
}