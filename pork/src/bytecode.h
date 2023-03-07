#pragma once

#include "types.h"

Bytecode* generate_bytecode(Arena* arena, ASTNode* ast);

BasicBlock* analyze_control_flow(Arena* arena, char* source, Bytecode* bytecode);

void analyze_data_flow(BasicBlock* graph, Bytecode* bytecode);