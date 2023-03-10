#include <stdio.h>

#include "bytecode.h"
#include "error.h"

#define MAX_LABELS 1024

typedef struct {
    Bytecode* bytecode;
    int label_count;
    int label_locations[MAX_LABELS];
    Program* program;
} Translator;

internal void emit(Translator* translator, Op op, Type* type, i64 a1, i64 a2, i64 a3, int line) {
    assert(translator->bytecode->length < MAX_INSTRUCTION_COUNT);
    assert(op);

    OpType op_type = type ? type->op_type : OP_TYPE_NONE;

    Instruction* ins = translator->bytecode->instructions + (translator->bytecode->length++);
    ins->op = op;
    ins->type = op_type;
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
    static_assert(NUM_AST_KINDS == 18, "not all ast kinds handled");
    switch (node->kind)
    {
        default:
            assert(false);
            return 0;

        case AST_INT_LITERAL: {
            i64 result = get_reg(translator);
            emit(translator, OP_IMM, node->type, result, node->int_literal, 0, node->token.line);
            return result;
        }

        case AST_VARIABLE: {
            return node->variable->reg;
        }

        case AST_CAST: {
            i64 input = translate(translator, node->expression);
            i64 result = get_reg(translator);
            emit(translator, OP_CAST, node->type, result, input, node->expression->type->op_type, node->token.line);
            return result;
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

            emit(translator, op, node->type, result, left, right, node->token.line);
            return result;
        }

        case AST_ASSIGN: {
            i64 result = translate(translator, node->right);
            emit(translator, OP_COPY, node->type, node->left->variable->reg, result, 0, node->token.line);
            return result;
        }

        case AST_BLOCK:
            for (ASTNode* statement = node->first; statement; statement = statement->next) {
                translate(translator, statement);
            }
            return -1;
            
        case AST_RETURN: {
            i64 result = translate(translator, node->expression);
            emit(translator, OP_RET, node->type, result, 0, 0, node->token.line);
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
            emit(translator, OP_CJMP, 0, condition, label_then, label_else, INT32_MAX);

            place_label(translator, label_then);
            translate(translator, node->conditional.block_then);

            if (has_else) {
                label_end = get_label(translator);
                emit(translator, OP_JMP, 0, label_end, 0, 0, INT32_MAX);
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
            emit(translator, OP_CJMP, 0, condition, label_body, label_end, INT32_MAX);

            place_label(translator, label_body);
            translate(translator, node->conditional.block_then);
            emit(translator, OP_JMP, 0, label_start, 0, 0, INT32_MAX);

            place_label(translator, label_end);

            return -1;
        }
    }
}

Bytecode* generate_bytecode(Arena* arena, ASTFunction* ast_function) {
    Bytecode* bytecode = arena_push_type(arena, Bytecode);

    Translator translator = {
        .bytecode = bytecode
    };

    translate(&translator, ast_function->body);

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
        if (block->end == block->start) {
            block->successors[0] = block->next ? block->next : &end_block;
            ++block->successor_count;
            continue;
        }

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

void analyze_data_flow(BasicBlock* graph, Bytecode* bytecode) {
    for (BasicBlock* b = graph; b; b = b->next)
    {
        for (int i = b->start; i < b->end; ++i)
        {
            Instruction* ins = bytecode->instructions + i;

            #define DEFINES(ai) \
                    set_insert(&b->var_kill, ins->ai)

            #define USES(ai) \
                    if (!set_has(&b->var_kill, ins->ai)) \
                        set_insert(&b->ue_var, ins->ai);

            static_assert(NUM_OPS == 16, "not all ops handled");
            switch (ins->op)
            {
                default:
                    assert(false);
                    break;

                case OP_NOOP:
                case OP_JMP:
                    break;

                case OP_IMM: // Define a1
                    DEFINES(a1);
                    break;

                case OP_COPY: // Define a1, use a2
                case OP_CAST:
                    USES(a2);
                    DEFINES(a1);
                    break;

                case OP_ADD:
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                case OP_LESS:
                case OP_LEQUAL:
                case OP_EQUAL:
                case OP_NEQUAL: // Define a1, use a2 and a3
                    USES(a2);
                    USES(a3);
                    DEFINES(a1);
                    break;

                case OP_RET:
                case OP_CJMP: // Use a1
                    USES(a1);
                    break;
            }

            #undef USES
            #undef DEFINES
        }
    }

    for (;;) {
        bool changed = false;

        for (BasicBlock* n = graph; n; n = n->next) {
            int initial_size = n->live_out.count;

            for (int i = 0; i < n->successor_count; ++i) {
                BasicBlock* m = n->successors[i];
                
                foreach_set(&m->ue_var, x) {
                    set_insert(&n->live_out, x.value);
                }

                foreach_set(&m->live_out, x) {
                    if (!set_has(&m->var_kill, x.value))
                        set_insert(&n->live_out, x.value);
                }
            }

            if (n->live_out.count != initial_size)
                changed = true;
        }

        if (!changed)
            break;
    }

    /*
    Set uinitialized_variables = {0};

    foreach_set(&graph->ue_var, x) {
        set_insert(&uinitialized_variables, x.value);
    }

    foreach_set(&graph->live_out, x) {
        if (!set_has(&graph->var_kill, x.value))
            set_insert(&uinitialized_variables, x.value);
    }
    */
}

typedef struct AdjacencyNode AdjacencyNode;
struct AdjacencyNode {
    bool active;
    i64 var;
    AdjacencyNode* next;
};

internal bool check_interference(i64 register_count, u8* matrix, i64 a, i64 b) {
    i64 row = b > a ? b : a;
    i64 column = b > a ? a : b;
    i64 bit_index = row * register_count + column;
    u8 value = (matrix[bit_index/8] >> (bit_index % 8)) & 1;
    return value;
}

internal AdjacencyNode* get_adjacency_node(Arena* arena, AdjacencyNode** free_list) {
    if (!*free_list) {
        return arena_push_type(arena, AdjacencyNode);
    }
    else {
        AdjacencyNode* front = *free_list;
        *free_list = (*free_list)->next;
        memset(front, 0, sizeof(*front));
        return front;
    }
}

internal void add_interference(Arena* arena, AdjacencyNode** free_list, i64 register_count, u8* matrix, AdjacencyNode** adjacency_lists, i64 a, i64 b) {
    if (check_interference(register_count, matrix, a, b)) {
        return;
    }

    i64 row = b > a ? b : a;
    i64 column = b > a ? a : b;
    i64 bit_index = row * register_count + column;
    u8* byte = matrix + (bit_index/8);
    *byte |= 1 << (bit_index % 8);

    AdjacencyNode* node_a = get_adjacency_node(arena, free_list);
    node_a->active = true;
    node_a->var = b;
    node_a->next = adjacency_lists[a];
    adjacency_lists[a] = node_a;

    AdjacencyNode* node_b = get_adjacency_node(arena, free_list);
    node_b->active = true;
    node_b->var = a;
    node_b->next = adjacency_lists[b];
    adjacency_lists[b] = node_b;
}

internal u64 calculate_bit_matrix_size(i64 register_count) {
    return register_count * register_count / 8 + (register_count % 8 != 0);
}

internal void clear_interference(i64 register_count, u8* matrix, AdjacencyNode** adjacency_lists, AdjacencyNode** free_list)
{
    memset(matrix, 0, calculate_bit_matrix_size(register_count));

    for (i64 i = 0; i < register_count; ++i) {
        if (adjacency_lists[i]) {
            AdjacencyNode* last = adjacency_lists[i];
            while (last->next) {
                last = last->next;
            }
            last->next = *free_list;
            *free_list = adjacency_lists[i];
            adjacency_lists[i] = 0;
        }
    }
}

internal AdjacencyNode* find_edge(AdjacencyNode* list, i64 var) {
    while(list) {
        if (list->var == var) {
            return list;
        }

        list = list->next;
    }

    assert(false);
    return 0;
}

internal u32 count_active_interferences(AdjacencyNode* list) {
    u32 count = 0;
    while (list) {
        if (list->active) {
            ++count;
        }
        list = list->next;
    }
    return count;
}

internal i64 get_lr(i64* mapping, i64 reg) {
    if (mapping[reg] == reg) {
        return reg;
    }

    i64 actual = get_lr(mapping, mapping[reg]);
    mapping[reg] = actual;

    return actual;
}

void allocate_registers(BasicBlock* graph, Bytecode* bytecode, u32 register_count) {
    Scratch scratch = get_scratch(0);

    u8* adjacency_matrix = arena_push_zero(scratch.arena, calculate_bit_matrix_size(bytecode->register_count));

    AdjacencyNode** adjacency_lists = arena_push_array(scratch.arena, AdjacencyNode*, bytecode->register_count);
    AdjacencyNode* adjacency_node_free_list = 0;

    int copy_instruction_count = 0;
    Instruction** copy_instructions = arena_push_array(scratch.arena, Instruction*, bytecode->length);

    i64* lrs = arena_push_array(scratch.arena, i64, bytecode->register_count);
    for (i64 i = 0; i < bytecode->register_count; ++i) {
        lrs[i] = i;
    }

    for (;;) {
        bool any_coalesced = false;

        clear_interference(bytecode->register_count, adjacency_matrix, adjacency_lists, &adjacency_node_free_list);
        copy_instruction_count = 0;

        for (BasicBlock* b = graph; b; b = b->next) {
            Set live_now = b->live_out;

            #define DEFINES(ai, is_copy) \
                        if (set_has(&live_now, get_lr(lrs, ins->ai))) \
                            set_remove(&live_now, get_lr(lrs, ins->ai)); \
                        foreach_set(&live_now, other) { \
                            if (!(is_copy) || get_lr(lrs, other.value) != get_lr(lrs, ins->a2)) { \
                                add_interference(scratch.arena, &adjacency_node_free_list, bytecode->register_count, adjacency_matrix, adjacency_lists, get_lr(lrs, other.value), get_lr(lrs, ins->ai)); \
                            } \
                        }

            #define USES(ai) set_insert(&live_now, ins->ai)

            for (int i = b->end-1; i >= b->start; --i) {
                Instruction* ins = bytecode->instructions + i;

                static_assert(NUM_OPS == 16, "not all ops handled");
                switch (ins->op)
                {
                    default:
                        assert(false);
                        break;

                    case OP_NOOP:
                    case OP_JMP:
                        break;

                    case OP_IMM: // Define a1
                        DEFINES(a1, false);
                        break;

                    case OP_COPY: // Copies require special treatement as they can be coalesced.
                        DEFINES(a1, true);
                        USES(a2);
                        copy_instructions[copy_instruction_count++] = ins;
                        break;
                       
                    case OP_CAST:
                        DEFINES(a1, false);
                        USES(a2);
                        break;

                    case OP_ADD:
                    case OP_SUB:
                    case OP_MUL:
                    case OP_DIV:
                    case OP_LESS:
                    case OP_LEQUAL:
                    case OP_EQUAL:
                    case OP_NEQUAL: // Define a1, use a2 and a3
                        DEFINES(a1, false);
                        USES(a2);
                        USES(a3);
                        break;

                    case OP_RET:
                    case OP_CJMP: // Use a1
                        USES(a1);
                        break;
                }
            }

            #undef USES
            #undef DEFINES
        }

        for (int i = copy_instruction_count-1; i >= 0; --i)
        {
            Instruction* copy = copy_instructions[i];
            assert(copy->op == OP_COPY);

            i64 lr1 = get_lr(lrs, copy->a1);
            i64 lr2 = get_lr(lrs, copy->a2);

            if (lr1 == lr2) {
                copy->op = OP_NOOP;
                copy_instructions[i] = copy_instructions[--copy_instruction_count];
            }
            else if (!check_interference(bytecode->register_count, adjacency_matrix, lr1, lr2))
            {
                //printf("Coalesced %lld and %lld\n", lr1, lr2);
                lrs[lr2] = lr1;
                any_coalesced = true;
            }
        }

        if (!any_coalesced)
            break;
    }

    Set live_ranges_to_select = {0};
    for (i64 i = 0; i < bytecode->register_count; ++i) {
        set_insert(&live_ranges_to_select, get_lr(lrs, i));
    }

    i64* colors = arena_push_array(scratch.arena, i64, bytecode->register_count);
    for (i64 i = 0; i < bytecode->register_count; ++i) {
        colors[i] = -1;
    }

    int select_count = 0;
    i64* select_stack = arena_push_array(scratch.arena, i64, bytecode->register_count);

    for (;;) {
        bool any_removed = false;

        foreach_set(&live_ranges_to_select, lr_it)
        {
            i64 lr = lr_it.value;

            u32 num_interferences = count_active_interferences(adjacency_lists[lr]);
            if (num_interferences < register_count)
            {
                select_stack[select_count++] = lr;
                set_remove(&live_ranges_to_select, lr);
                
                for (AdjacencyNode* edge = adjacency_lists[lr]; edge; edge = edge->next) {
                    edge->active = false;
                    find_edge(adjacency_lists[edge->var], lr)->active = false;
                }

                any_removed = true;
            }
        }

        if (!any_removed)
            break;
    }

    assert(live_ranges_to_select.count == 0);

    // TODO: Select non-trivially colorable LRs based on spill metrics

    while (select_count > 0) {
        i64 lr = select_stack[--select_count];

        bool occupied_colors[128] = {0};
        assert(register_count <= LENGTH(occupied_colors));

        for (AdjacencyNode* edge = adjacency_lists[lr]; edge; edge = edge->next) {
            if (edge->active) {
                i64 edge_color = colors[edge->var];
                assert(edge_color != -1);
                occupied_colors[colors[edge->var]] = true;
            }

            find_edge(adjacency_lists[edge->var], lr)->active = true;
        }
        
        for (u32 i = 0; i < register_count; ++i) {
            if (!occupied_colors[i]) {
                colors[lr] = i;
                break;
            }
        }

        assert(colors[lr] != -1);
    }

    /*
    for (i64 i = 0; i < bytecode->register_count; ++i) {
        printf("%lld -> %lld\n", i, colors[get_lr(lrs, i)]);
    }
    */

    for (int i = 0; i < bytecode->length; ++i) {
        Instruction* ins = bytecode->instructions + i;

        #define REMAP(var) colors[get_lr(lrs, var)]
        
        static_assert(NUM_OPS == 16, "not all ops handled");
        switch (ins->op)
        {
            default:
                assert(false);
                break;

            case OP_NOOP:
            case OP_JMP:
                break;

            case OP_IMM:
            case OP_CJMP:
            case OP_RET:
                ins->a1 = REMAP(ins->a1);
                break;

            case OP_COPY:
            case OP_CAST:
                ins->a1 = REMAP(ins->a1);
                ins->a2 = REMAP(ins->a2);
                break;

            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV:
            case OP_LESS:
            case OP_LEQUAL:
            case OP_EQUAL:
            case OP_NEQUAL:
                ins->a1 = REMAP(ins->a1);
                ins->a2 = REMAP(ins->a2);
                ins->a3 = REMAP(ins->a3);
                break;
        }

        #undef REMAP
    }

    bytecode->register_count = register_count;
    release_scratch(&scratch);
}
