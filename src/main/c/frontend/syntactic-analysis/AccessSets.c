#include "AccessSets.h"
#include "AstWalk.h"
#include <stdlib.h>
#include <string.h>

static void
_add(NameSet *set, char *name) {
	if (set->count == set->capacity) {
		set->capacity = set->capacity == 0 ? 4 : set->capacity * 2;
		set->names = realloc(set->names, set->capacity * sizeof(*set->names));
	}
	set->names[set->count++] = name;
}

void
nameSetAdd(NameSet *set, char *name) {
	_add(set, name);
}

bool
nameSetContains(const NameSet *set, const char *name) {
	for (int i = 0; i < set->count; ++i) {
		if (strcmp(set->names[i], name) == 0) {
			return true;
		}
	}
	return false;
}

bool
nameSetsIntersect(const NameSet *a, const NameSet *b) {
	for (int i = 0; i < a->count; ++i) {
		if (nameSetContains(b, a->names[i])) {
			return true;
		}
	}
	return false;
}

void
nameSetFree(NameSet *set) {
	free(set->names);
	set->names = NULL;
	set->count = 0;
	set->capacity = 0;
}

static void _collectExpr(Expr *expr, Program *program, NameSet *reads, NameSet *writes, bool propagate);
static void _collectStmt(Stmt *stmt, Program *program, NameSet *reads, NameSet *writes, bool propagate);

static Decl *
_findFunction(Program *program, const char *name) {
	if (program == NULL) {
		return NULL;
	}
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (node->decl->kind == DECL_FUNCTION && strcmp(node->decl->func.name, name) == 0) {
			return node->decl;
		}
	}
	return NULL;
}

/* The base identifier of an lvalue argument (the only thing passable by reference). */
static char *
_lvalueBase(Expr *expr) {
	if (expr->kind == EXPR_IDENTIFIER) {
		return expr->name;
	}
	if (expr->kind == EXPR_ARRAY_ACCESS && expr->arrayAccess.array->kind == EXPR_IDENTIFIER) {
		return expr->arrayAccess.array->name;
	}
	return NULL;
}

/* Whether the callee reads/writes a given parameter in its own body (direct, one level:
 * the scan does not propagate through the callee's own calls, which avoids recursion). */
static void
_calleeParamDirection(Decl *callee, const char *param, bool *read, bool *written) {
	NameSet reads = { 0 };
	NameSet writes = { 0 };
	_collectStmt(callee->func.body, NULL, &reads, &writes, false);
	*read = nameSetContains(&reads, param);
	*written = nameSetContains(&writes, param);
	nameSetFree(&reads);
	nameSetFree(&writes);
}

static void
_collectCall(Expr *expr, Program *program, NameSet *reads, NameSet *writes, bool propagate) {
	Decl *callee = propagate ? _findFunction(program, expr->call.name) : NULL;
	ParamList *param = callee != NULL ? callee->func.params : NULL;
	for (ExprList *argNode = expr->call.args; argNode != NULL; argNode = argNode->next) {
		char *base = (param != NULL && param->param->kind == PARAM_REF) ? _lvalueBase(argNode->expr) : NULL;
		if (base != NULL) {
			/* A reference argument is read/written exactly as the callee uses the param. */
			bool read, written;
			_calleeParamDirection(callee, param->param->name, &read, &written);
			if (read) {
				_add(reads, base);
			}
			if (written) {
				_add(writes, base);
			}
			if (argNode->expr->kind == EXPR_ARRAY_ACCESS) {
				_collectExpr(argNode->expr->arrayAccess.index, program, reads, writes, propagate);
			}
		} else {
			_collectExpr(argNode->expr, program, reads, writes, propagate);
		}
		if (param != NULL) {
			param = param->next;
		}
	}
}

static void
_collectExpr(Expr *expr, Program *program, NameSet *reads, NameSet *writes, bool propagate) {
	if (expr == NULL) {
		return;
	}
	switch (expr->kind) {
	case EXPR_IDENTIFIER:
		_add(reads, expr->name);
		break;
	case EXPR_ASSIGN: {
		Expr *target = expr->assign.target;
		bool compound = expr->assign.op != ASSIGN_PLAIN;
		if (target->kind == EXPR_IDENTIFIER) {
			_add(writes, target->name);
			if (compound) {
				_add(reads, target->name);
			}
		} else if (target->kind == EXPR_ARRAY_ACCESS && target->arrayAccess.array->kind == EXPR_IDENTIFIER) {
			_add(writes, target->arrayAccess.array->name);
			if (compound) {
				_add(reads, target->arrayAccess.array->name);
			}
			_collectExpr(target->arrayAccess.index, program, reads, writes, propagate);
		} else {
			_collectExpr(target, program, reads, writes, propagate);
		}
		_collectExpr(expr->assign.value, program, reads, writes, propagate);
		break;
	}
	case EXPR_POST_INC:
	case EXPR_POST_DEC:
		if (expr->postOp->kind == EXPR_IDENTIFIER) {
			_add(reads, expr->postOp->name);
			_add(writes, expr->postOp->name);
		} else {
			_collectExpr(expr->postOp, program, reads, writes, propagate);
		}
		break;
	case EXPR_BINARY:
		_collectExpr(expr->binary.left, program, reads, writes, propagate);
		_collectExpr(expr->binary.right, program, reads, writes, propagate);
		break;
	case EXPR_UNARY:
		_collectExpr(expr->unary.operand, program, reads, writes, propagate);
		break;
	case EXPR_CAST:
		_collectExpr(expr->cast.operand, program, reads, writes, propagate);
		break;
	case EXPR_ARRAY_ACCESS:
		_collectExpr(expr->arrayAccess.array, program, reads, writes, propagate);
		_collectExpr(expr->arrayAccess.index, program, reads, writes, propagate);
		break;
	case EXPR_CALL:
		_collectCall(expr, program, reads, writes, propagate);
		break;
	case EXPR_INTRINSIC:
		for (ExprList *argNode = expr->intrinsic.args; argNode != NULL; argNode = argNode->next) {
			_collectExpr(argNode->expr, program, reads, writes, propagate);
		}
		break;
	default:
		break;
	}
}

static void
_collectStmt(Stmt *stmt, Program *program, NameSet *reads, NameSet *writes, bool propagate) {
	if (stmt == NULL) {
		return;
	}
	switch (stmt->kind) {
	case STMT_EXPR:
		_collectExpr(stmt->exprStmt, program, reads, writes, propagate);
		break;
	case STMT_LOCAL_DECL:
		_collectExpr(stmt->localDecl.arraySize, program, reads, writes, propagate);
		_collectExpr(stmt->localDecl.init, program, reads, writes, propagate);
		for (ExprList *initNode = stmt->localDecl.initList; initNode != NULL; initNode = initNode->next) {
			_collectExpr(initNode->expr, program, reads, writes, propagate);
		}
		break;
	case STMT_IF:
		_collectExpr(stmt->ifStmt.condition, program, reads, writes, propagate);
		_collectStmt(stmt->ifStmt.thenBranch, program, reads, writes, propagate);
		_collectStmt(stmt->ifStmt.elseBranch, program, reads, writes, propagate);
		break;
	case STMT_FOR:
		_collectStmt(stmt->forStmt.init, program, reads, writes, propagate);
		_collectExpr(stmt->forStmt.condition, program, reads, writes, propagate);
		_collectExpr(stmt->forStmt.update, program, reads, writes, propagate);
		_collectStmt(stmt->forStmt.body, program, reads, writes, propagate);
		break;
	case STMT_WHILE:
		_collectExpr(stmt->whileStmt.condition, program, reads, writes, propagate);
		_collectStmt(stmt->whileStmt.body, program, reads, writes, propagate);
		break;
	case STMT_RETURN:
		_collectExpr(stmt->returnExpr, program, reads, writes, propagate);
		break;
	case STMT_COMPOUND:
		for (StmtList *stmtNode = stmt->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
			_collectStmt(stmtNode->stmt, program, reads, writes, propagate);
		}
		break;
	case STMT_PARALLEL:
		for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
			_collectStmt(stmtNode->stmt, program, reads, writes, propagate);
		}
		break;
	}
}

void
collectStmtAccesses(Stmt *stmt, Program *program, NameSet *reads, NameSet *writes) {
	_collectStmt(stmt, program, reads, writes, true);
}

typedef struct {
	Program *program;
	NameSet *driven;
} CallDrivenContext;

static void
_collectCallDriven(Expr *expr, void *context) {
	if (expr->kind != EXPR_CALL) {
		return;
	}
	CallDrivenContext *ctx = context;
	Decl *callee = _findFunction(ctx->program, expr->call.name);
	if (callee == NULL) {
		return;
	}
	ParamList *param = callee->func.params;
	for (ExprList *argNode = expr->call.args; argNode != NULL && param != NULL;
	     argNode = argNode->next, param = param->next) {
		if (param->param->kind != PARAM_REF) {
			continue;
		}
		bool read, written;
		_calleeParamDirection(callee, param->param->name, &read, &written);
		char *base = _lvalueBase(argNode->expr);
		if (written && base != NULL) {
			_add(ctx->driven, base);
		}
	}
}

void
collectCallRefWrites(Stmt *stmt, Program *program, NameSet *driven) {
	CallDrivenContext ctx = { .program = program, .driven = driven };
	walkStmtExprs(stmt, _collectCallDriven, &ctx);
}
