#pragma once

#include "frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include <stdbool.h>

/*
 * Evaluates a compile-time-constant integer expression (literals, constexpr
 * globals, arithmetic, intrinsics, and constexpr function calls). Returns true
 * and writes the folded value on success; false if the expression is not
 * statically evaluable.
 */
bool evalConstexpr(Expr *expr, Program *program, long *out);
