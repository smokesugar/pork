#include <ctype.h>

#include "lexer.h"

Lexer init_lexer(char* source) {
    return (Lexer) {
        .source = source,
        .pointer = source,
        .line = 1
    };
}

Token get_token(Lexer* lexer) {
    if (lexer->cache.memory) {
        Token token = lexer->cache;
        lexer->cache.memory = 0;
        return token;
    }

    while (isspace(*lexer->pointer)) {
        if (*lexer->pointer == '\n') {
            ++lexer->line;
        }
        ++lexer->pointer;
    }

    char* start = lexer->pointer++;
    int kind = *(start);
    int line = lexer->line;

    switch (*start) {
        default:
            if (isdigit(*start)) {
                while (isdigit(*lexer->pointer)) {
                    ++lexer->pointer;
                }
                kind = TOKEN_INT_LITERAL;
            }
            break;

        case '\0':
            --lexer->pointer;
            break;
    }

    return (Token) {
        .kind = kind,
        .memory = start,
        .length = (int)(lexer->pointer-start),
        .line = line
    };
}

Token peek_token(Lexer* lexer) {
    if (!lexer->cache.memory) {
        lexer->cache = get_token(lexer);
    }

    return lexer->cache;
}
