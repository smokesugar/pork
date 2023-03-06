#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "error.h"

internal int find_line_length(char* line_memory) {
    int line_length = 0;

    while (line_memory[line_length] != '\n' && line_memory[line_length] != '\0') {
        ++line_length;
    }

    return line_length;
}

void error_at_token(char* source, Token token, char* format, ...) {
    char* line_memory = token.memory;
    while (*line_memory != '\n' && line_memory != source) {
        --line_memory;
    }
    
    while (isspace(*line_memory)) {
        ++line_memory;
    }

    char prefix[512];
    int prefix_length = sprintf_s(prefix, sizeof(prefix), "Line %d: Error: ", token.line);

    int arrow_offset = prefix_length + (int)(token.memory-line_memory);

    printf("%s%.*s\n", prefix, find_line_length(line_memory), line_memory);
    printf("%*s^ ", arrow_offset, "");

    va_list argument_list;
    va_start(argument_list, format);
    vprintf(format, argument_list);
    va_end(argument_list);

    printf("\n");
}

void error_on_line(char* source, int line, char* format, ...) {
    char* line_memory = source;
    for (int i = 1; i < line; ++i) {
        while (*line_memory != '\n' && *line_memory != '\0') {
            ++line_memory;
        }
        assert(*line_memory == '\n' && "line out of bounds");
        ++line_memory;
    }

    while (isspace(*line_memory)) {
        ++line_memory;
    }

    printf("Line %d: ", line);

    va_list argument_list;
    va_start(argument_list, format);
    vprintf(format, argument_list);
    va_end(argument_list);

    printf(": '%.*s'\n", find_line_length(line_memory), line_memory);
}
