#pragma once

#include "types.h"

void init_program(Program* program);

bool analyze_semantics(Arena* arena, char* source, Program* program, ASTNode* ast);
