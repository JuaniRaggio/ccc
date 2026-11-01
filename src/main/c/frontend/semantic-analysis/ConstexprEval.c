#include "ConstexprEval.h"
#include <stdlib.h>
#include <string.h>

#define LOOP_CAP (1 << 24)

typedef struct {
	char **names;
	long *values;
	int count;
	int capacity;
} Env;

static void
_envSet(Env *env, char *name, long value) {
	for (int i = 0; i < env->count; ++i) {
		if (strcmp(env->names[i], name) == 0) {
			env->values[i] = value;
			return;
		}
	}
	if (env->count == env->capacity) {
		env->capacity = env->capacity == 0 ? 8 : env->capacity * 2;
		env->names = realloc(env->names, env->capacity * sizeof(char *));
		env->values = realloc(env->values, env->capacity * sizeof(long));
	}
	env->names[env->count] = name;
	env->values[env->count] = value;
	env->count++;
}

static bool
_envGet(Env *env, const char *name, long *out) {
	if (env == NULL) {
		return false;
	}
	for (int i = 0; i < env->count; ++i) {
		if (strcmp(env->names[i], name) == 0) {
			*out = env->values[i];
			return true;
		}
	}
	return false;
}

static Decl *
_findDecl(Program *program, const char *name, DeclKind kind) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (node->decl->kind == kind) {
			const char *declName = kind == DECL_CONSTEXPR_FUNC  ? node->decl->func.name
			                       : kind == DECL_CONSTEXPR_VAR ? node->decl->constexprVar.name
			                                                    : node->decl->constexprArray.name;
			if (strcmp(declName, name) == 0) {
				return node->decl;
			}
		}
	}
	return NULL;
}

static long _eval(Expr *expr, Program *program, Env *env, bool *ok);
static void _exec(Stmt *stmt, Program *program, Env *env, bool *ok, bool *returned, long *retval);

static long
_evalBinary(Expr *expr, Program *program, Env *env, bool *ok) {
	long left = _eval(expr->binary.left, program, env, ok);
	long right = _eval(expr->binary.right, program, env, ok);
	switch (expr->binary.op) {
	case BINOP_ADD:
		return left + right;
	case BINOP_SUB:
		return left - right;
	case BINOP_MUL:
		return left * right;
	case BINOP_LSHIFT:
		return left << right;
	case BINOP_RSHIFT:
		return left >> right;
	case BINOP_EQ:
		return left == right;
	case BINOP_NEQ:
		return left != right;
	case BINOP_LT:
		return left < right;
	case BINOP_GT:
		return left > right;
	case BINOP_LE:
		return left <= right;
	case BINOP_GE:
		return left >= right;
	case BINOP_AND:
		return left && right;
	default:
		return left || right;
	}
}

static long
_evalCall(Expr *expr, Program *program, Env *env, bool *ok) {
	Decl *function = _findDecl(program, expr->call.name, DECL_CONSTEXPR_FUNC);
	if (function == NULL) {
		*ok = false;
		return 0;
	}
	Env local = { .names = NULL, .values = NULL, .count = 0, .capacity = 0 };
	ParamList *param = function->func.params;
	ExprList *arg = expr->call.args;
	for (; param != NULL && arg != NULL; param = param->next, arg = arg->next) {
		_envSet(&local, param->param->name, _eval(arg->expr, program, env, ok));
	}
	bool returned = false;
	long retval = 0;
	_exec(function->func.body, program, &local, ok, &returned, &retval);
	free(local.names);
	free(local.values);
	return retval;
}

static long
_evalArrayAccess(Expr *expr, Program *program, Env *env, bool *ok) {
	long index = _eval(expr->arrayAccess.index, program, env, ok);
	Decl *array = expr->arrayAccess.array->kind == EXPR_IDENTIFIER
	                  ? _findDecl(program, expr->arrayAccess.array->name, DECL_CONSTEXPR_ARRAY)
	                  : NULL;
	if (array == NULL) {
		*ok = false;
		return 0;
	}
	long pos = 0;
	for (ExprList *element = array->constexprArray.init; element != NULL; element = element->next, ++pos) {
		if (pos == index) {
			return _eval(element->expr, program, NULL, ok);
		}
	}
	*ok = false;
	return 0;
}

static long
_eval(Expr *expr, Program *program, Env *env, bool *ok) {
	if (expr == NULL || !*ok) {
		*ok = false;
		return 0;
	}
	switch (expr->kind) {
	case EXPR_INT_LITERAL:
		return expr->intValue;
	case EXPR_BOOL_LITERAL:
		return expr->boolValue ? 1 : 0;
	case EXPR_IDENTIFIER: {
		long value;
		if (_envGet(env, expr->name, &value)) {
			return value;
		}
		Decl *var = _findDecl(program, expr->name, DECL_CONSTEXPR_VAR);
		if (var != NULL) {
			return _eval(var->constexprVar.init, program, NULL, ok);
		}
		*ok = false;
		return 0;
	}
	case EXPR_BINARY:
		return _evalBinary(expr, program, env, ok);
	case EXPR_UNARY: {
		long operand = _eval(expr->unary.operand, program, env, ok);
		return expr->unary.op == UNOP_NEG ? -operand : !operand;
	}
	case EXPR_ASSIGN: {
		long value = _eval(expr->assign.value, program, env, ok);
		if (expr->assign.target->kind == EXPR_IDENTIFIER) {
			long current = 0;
			if (expr->assign.op != ASSIGN_PLAIN) {
				_envGet(env, expr->assign.target->name, &current);
			}
			if (expr->assign.op == ASSIGN_ADD) {
				value = current + value;
			} else if (expr->assign.op == ASSIGN_SUB) {
				value = current - value;
			} else if (expr->assign.op == ASSIGN_MUL) {
				value = current * value;
			}
			_envSet(env, expr->assign.target->name, value);
		}
		return value;
	}
	case EXPR_ARRAY_ACCESS:
		return _evalArrayAccess(expr, program, env, ok);
	case EXPR_CALL:
		return _evalCall(expr, program, env, ok);
	case EXPR_INTRINSIC: {
		long first = _eval(expr->intrinsic.args->expr, program, env, ok);
		if (expr->intrinsic.kind == INTRINSIC_ABS) {
			return first < 0 ? -first : first;
		}
		long second = _eval(expr->intrinsic.args->next->expr, program, env, ok);
		if (expr->intrinsic.kind == INTRINSIC_MIN) {
			return first < second ? first : second;
		}
		return first > second ? first : second;
	}
	case EXPR_POST_INC:
	case EXPR_POST_DEC: {
		long current = 0;
		_envGet(env, expr->postOp->name, &current);
		_envSet(env, expr->postOp->name, current + (expr->kind == EXPR_POST_INC ? 1 : -1));
		return current;
	}
	default:
		*ok = false;
		return 0;
	}
}

static void
_exec(Stmt *stmt, Program *program, Env *env, bool *ok, bool *returned, long *retval) {
	if (stmt == NULL || *returned || !*ok) {
		return;
	}
	switch (stmt->kind) {
	case STMT_EXPR:
		_eval(stmt->exprStmt, program, env, ok);
		break;
	case STMT_LOCAL_DECL:
		if (stmt->localDecl.init != NULL) {
			_envSet(env, stmt->localDecl.name, _eval(stmt->localDecl.init, program, env, ok));
		}
		break;
	case STMT_IF:
		if (_eval(stmt->ifStmt.condition, program, env, ok)) {
			_exec(stmt->ifStmt.thenBranch, program, env, ok, returned, retval);
		} else {
			_exec(stmt->ifStmt.elseBranch, program, env, ok, returned, retval);
		}
		break;
	case STMT_FOR:
		_exec(stmt->forStmt.init, program, env, ok, returned, retval);
		for (int guard = 0; guard < LOOP_CAP && *ok && !*returned; ++guard) {
			if (!_eval(stmt->forStmt.condition, program, env, ok)) {
				break;
			}
			_exec(stmt->forStmt.body, program, env, ok, returned, retval);
			_eval(stmt->forStmt.update, program, env, ok);
		}
		break;
	case STMT_WHILE:
		for (int guard = 0; guard < LOOP_CAP && *ok && !*returned; ++guard) {
			if (!_eval(stmt->whileStmt.condition, program, env, ok)) {
				break;
			}
			_exec(stmt->whileStmt.body, program, env, ok, returned, retval);
		}
		break;
	case STMT_RETURN:
		*returned = true;
		*retval = stmt->returnExpr != NULL ? _eval(stmt->returnExpr, program, env, ok) : 0;
		break;
	case STMT_COMPOUND:
		for (StmtList *node = stmt->compound; node != NULL; node = node->next) {
			_exec(node->stmt, program, env, ok, returned, retval);
			if (*returned || !*ok) {
				break;
			}
		}
		break;
	default:
		break;
	}
}

bool
evalConstexpr(Expr *expr, Program *program, long *out) {
	bool ok = true;
	long value = _eval(expr, program, NULL, &ok);
	if (ok) {
		*out = value;
	}
	return ok;
}
