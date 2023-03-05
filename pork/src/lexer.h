#pragma once

#include "types.h"

typedef struct {
    char* source;
    char* pointer;
    int line;
    Token cache;
} Lexer;

Lexer init_lexer(char* source);

Token get_token(Lexer* lexer);
Token peek_token(Lexer* lexer);
