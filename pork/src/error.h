#pragma once

#include "types.h"

void error_at_token(char* source, Token token, char* format, ...);
void error_on_line(char* source, int line, char* format, ...);
