#pragma once

#include "types.h"

void init_program(Program* program);

bool analyze_semantics(Arena* arena, char* source, Program* program, ASTFunction* ast_function);

bool type_is_integral(Program* program, Type* type);
bool type_is_signed_integral(Program* program, Type* type);
Type* get_signed_integral_type(Program* program, Type* type);
Type* get_unsigned_integral_type(Program* program, Type* type);
