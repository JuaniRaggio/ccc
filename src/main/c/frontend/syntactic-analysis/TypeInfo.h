#pragma once

#include "AbstractSyntaxTree.h"
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

/* Number of bits in the integer used for compile-time folding. */
#define FOLD_BITS ((int)(CHAR_BIT * sizeof(int64_t)))

/* Bit width of an integer type (8/16/32/64); 1 for bool. */
int typeWidth(TypeKind type);

/* True for the signed integer types (intN). */
bool typeSigned(TypeKind type);

/* Truncates/extends a value to the type's width and signedness. */
int64_t maskToType(int64_t value, TypeKind type);

/* True if the value is representable in the type without overflow. */
bool valueFitsType(int64_t value, TypeKind type);
