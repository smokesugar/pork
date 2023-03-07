#pragma once

#include "base.h"

#define SET_CAPACITY (1 << 8)

typedef struct {
    int count;
    i64 keys[SET_CAPACITY];
    u8 occupancy[SET_CAPACITY];
} Set;

typedef struct {
    int index;
    Set* set;
    i64 value;
} SetIterator;

void set_insert(Set* set, i64 key);
bool set_has(Set* set, i64 key);
void set_remove(Set* set, i64 key);

SetIterator set_begin(Set* set);
bool set_continue(SetIterator* it);
void set_next(SetIterator* it);

#define foreach_set(set, it)  for (SetIterator it = set_begin(set); set_continue(&it); set_next(&it))
