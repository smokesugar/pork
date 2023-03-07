#include "set.h"

enum {
    EMPTY,
    OCCUPIED,
    REMOVED 
};

internal u64 fnv1(void* memory, u64 size) {
    u64 hash = 0xcbf29ce484222325;

    for (u64 i = 0; i < size; ++i) {
        hash ^= ((u8*)memory)[i];
        hash *= 0x100000001b3;
    }

    return hash;
}

void set_insert(Set* set, i64 key) {
    u64 hash = fnv1(&key, sizeof(key));
    int i = hash % SET_CAPACITY;

    int last_grave = -1;

    for (int j = 0; j < SET_CAPACITY; ++j)
    {
        switch(set->occupancy[i])  {
            default:
                assert(false);
                break;

            case OCCUPIED:
                if (set->keys[i] == key) {
                    return;
                }
                break;

            case REMOVED:
                last_grave = i;     
                break;

            case EMPTY: {
                int index = last_grave == -1 ? i : last_grave;
                set->keys[index] = key;
                set->occupancy[index] = OCCUPIED;
                ++set->count;
                return;
            }
        }

        i = (i+1) % SET_CAPACITY;
    }

    assert(false && "set is full");
}

internal int set_find_index(Set* set, i64 key) {
    u64 hash = fnv1(&key, sizeof(key));
    int i = hash % SET_CAPACITY;

    for (int j = 0; j < SET_CAPACITY; ++j)
    {
        switch(set->occupancy[i])  {
            default:
                assert(false);
                break;

            case OCCUPIED:
                if (set->keys[i] == key) {
                    return i;
                }
                break;

            case REMOVED:
                break;

            case EMPTY:
                return -1;
        }

        i = (i+1) % SET_CAPACITY;
    }

    return -1;
}

bool set_has(Set* set, i64 key) {
    return set_find_index(set, key) != -1;
}

void set_remove(Set* set, i64 key) {
    int i = set_find_index(set, key);
    assert(i != -1 && "set does not contain value");
    set->occupancy[i] = REMOVED;
    --set->count;
}

SetIterator set_begin(Set* set) {
    int index = 0;
    while (index < SET_CAPACITY && set->occupancy[index] != OCCUPIED) {
        ++index;
    }

    return (SetIterator){
        .index = index,
        .set = set,
        .value = 0
    };
}

bool set_continue(SetIterator* it) {
    if (it->index < SET_CAPACITY) {
        it->value = it->set->keys[it->index];
        return true;
    }

    return false;
}

void set_next(SetIterator* it) {
    do {
        ++it->index;
    } while (it->index < SET_CAPACITY && it->set->occupancy[it->index] != OCCUPIED);
}
