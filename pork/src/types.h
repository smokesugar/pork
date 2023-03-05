#pragma once

#include "base.h"

enum {
    TOKEN_EOF = 0,
    TOKEN_INT_LITERAL = 256,
};

typedef struct {
    int kind;
    char* memory;
    int length;
    int line;
} Token;
