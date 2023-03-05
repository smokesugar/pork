#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <memory.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

#define internal static

#define LENGTH(x) (sizeof(x)/sizeof(x[0]))

typedef struct {
    void* memory;
    u64 size;
    u64 allocated;
} Arena;

Arena* new_arena(u64 size);

void* arena_push(Arena* arena, u64 size);
void* arena_push_zero(Arena* arena, u64 size);

#define arena_push_array(arena, type, count) (type*)arena_push_zero(arena, sizeof(type) * count)
#define arena_push_type(arena, type) arena_push_array(arena, type, 1)

typedef struct {
    Arena* arena;
    u64 allocated;
} Scratch;

Scratch get_scratch(Arena* conflict);
void release_scratch(Scratch* scratch);
