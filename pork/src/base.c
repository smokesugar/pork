#include <stdlib.h>

#include "base.h"

Arena* new_arena(u64 size) {
    Arena* arena = malloc(sizeof(Arena) + size);
    arena->memory = arena + 1;
    arena->size = size;
    arena->allocated = 0;
    return arena;
}

void* arena_push(Arena* arena, u64 size) {
    size = (size + 7) & ~7;
    assert(arena->size-arena->allocated >= size && "arena out of memory");

    u64 offset = arena->allocated;
    arena->allocated += size;

    return size ? (u8*)arena->memory + offset : 0;
}

void* arena_push_zero(Arena* arena, u64 size) {
    void* memory = arena_push(arena, size);
    memset(memory, 0, size);
    return memory;
}
