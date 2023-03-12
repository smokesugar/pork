#include <ctype.h>
#include <string.h>

#include "lexer.h"

Lexer init_lexer(char* source) {
    return (Lexer) {
        .pointer = source,
        .line = 1
    };
}

internal bool is_identifier(char c) {
    return isalnum(c) || c == '_';
}

internal int check_keyword(char* start, char* pointer, char* keyword, int kind) {
    size_t length = pointer-start;

    if (length == strlen(keyword) && memcmp(start, keyword, length) == 0) {
        return kind;
    }

    return TOKEN_IDENTIFIER;
}

internal int identifier_kind(char* start, char* pointer) {
    switch(start[0]) {
        case 'r':
            return check_keyword(start, pointer, "return", TOKEN_RETURN);
        case 'u':
            switch (start[1]) {
                case '6':
                    return check_keyword(start, pointer, "u64", TOKEN_U64);
                case '3':
                    return check_keyword(start, pointer, "u32", TOKEN_U32);
                case '1':
                    return check_keyword(start, pointer, "u16", TOKEN_U16);
                case '8':
                    return check_keyword(start, pointer, "u8", TOKEN_U8);
            }
            break;
        case 'i':
            switch (start[1]) {
                case 'f':
                    return check_keyword(start, pointer, "if", TOKEN_IF);
                case '6':
                    return check_keyword(start, pointer, "i64", TOKEN_I64);
                case '3':
                    return check_keyword(start, pointer, "i32", TOKEN_I32);
                case '1':
                    return check_keyword(start, pointer, "i16", TOKEN_I16);
                case '8':
                    return check_keyword(start, pointer, "i8", TOKEN_I8);
            }
            break;
        case 'e':
            return check_keyword(start, pointer, "else", TOKEN_ELSE);
        case 'w':
            return check_keyword(start, pointer, "while", TOKEN_WHILE);
    }
    
    return TOKEN_IDENTIFIER;
}

internal bool match(Lexer* lexer, char c) {
    if (*lexer->pointer == c) {
        ++lexer->pointer;
        return true;
    }

    return false;
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
            else if(is_identifier(*start)) {
                while (is_identifier(*lexer->pointer)) {
                    ++lexer->pointer;
                }
                kind = identifier_kind(start, lexer->pointer);
            }
            break;

        case '\0':
            --lexer->pointer;
            break;

        case '<':
            if (match(lexer, '='))
                kind = TOKEN_LESS_EQUAL;
            break;
        case '>':
            if (match(lexer, '='))
                kind = TOKEN_GREATER_EQUAL;
            break;
        case '=':
            if (match(lexer, '='))
                kind = TOKEN_EQUAL_EQUAL;
            break;
        case '!':
            if (match(lexer, '='))
                kind = TOKEN_BANG_EQUAL;
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

void jump_to_token(Lexer* lexer, Token token) {
    lexer->cache.memory = 0;
    lexer->pointer = token.memory;
    lexer->line = token.line;
}