#include <stdio.h>

#include "base.h"
#include "parse.h"

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

    printf("%lld\n", eval(ast));

    return 0;
}