#include "vm.h"
#include <stdio.h>

#define VM_REGISTER_COUNT 8

i64 vm_execute(Bytecode* bytecode) {
    i64 regs[VM_REGISTER_COUNT] = {0};

    for (int i = 0; i < bytecode->length;)
    {
        Instruction* ins = bytecode->instructions + i;

        static_assert(NUM_OPS == 16, "not all ops handled");
        switch (ins->op) {
            default:
                assert(false);
                break;

            case OP_NOOP:
                break;

            case OP_IMM:
                regs[ins->a1] = ins->a2;
                break;
            case OP_COPY:
                regs[ins->a1] = regs[ins->a2];
                break;
            case OP_CAST:
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
                return regs[ins->a1];

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
    return 0;
}
