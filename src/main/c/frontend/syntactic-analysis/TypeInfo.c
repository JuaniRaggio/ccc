#include "TypeInfo.h"

int
typeWidth(TypeKind type) {
	switch (type) {
	case TYPE_INT8:
	case TYPE_UINT8:
		return 8;
	case TYPE_INT16:
	case TYPE_UINT16:
		return 16;
	case TYPE_INT64:
	case TYPE_UINT64:
		return 64;
	case TYPE_BOOL:
		return 1;
	default:
		return 32;
	}
}

bool
typeSigned(TypeKind type) {
	return type == TYPE_INT8 || type == TYPE_INT16 || type == TYPE_INT32 || type == TYPE_INT64;
}

int64_t
maskToType(int64_t value, TypeKind type) {
	int width = typeWidth(type);
	if (width >= FOLD_BITS) {
		return value;
	}
	uint64_t mask = ((uint64_t)1 << width) - 1;
	uint64_t bits = (uint64_t)value & mask;
	if (typeSigned(type) && (bits & ((uint64_t)1 << (width - 1)))) {
		bits |= ~mask;
	}
	return (int64_t)bits;
}

bool
valueFitsType(int64_t value, TypeKind type) {
	int width = typeWidth(type);
	if (width >= FOLD_BITS) {
		return true;
	}
	if (typeSigned(type)) {
		return value >= -((int64_t)1 << (width - 1)) && value < ((int64_t)1 << (width - 1));
	}
	return value >= 0 && value < ((int64_t)1 << width);
}
