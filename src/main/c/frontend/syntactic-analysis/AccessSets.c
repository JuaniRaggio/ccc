#include "AccessSets.h"
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

static void
_collectExpr(Expr *expr, NameSet *reads, NameSet *writes) {
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
			_collectExpr(target->arrayAccess.index, reads, writes);
		} else {
			_collectExpr(target, reads, writes);
		}
		_collectExpr(expr->assign.value, reads, writes);
		break;
	}
	case EXPR_POST_INC:
	case EXPR_POST_DEC:
		if (expr->postOp->kind == EXPR_IDENTIFIER) {
			_add(reads, expr->postOp->name);
			_add(writes, expr->postOp->name);
		} else {
			_collectExpr(expr->postOp, reads, writes);
		}
		break;
	case EXPR_BINARY:
		_collectExpr(expr->binary.left, reads, writes);
		_collectExpr(expr->binary.right, reads, writes);
		break;
	case EXPR_UNARY:
		_collectExpr(expr->unary.operand, reads, writes);
		break;
	case EXPR_CAST:
		_collectExpr(expr->cast.operand, reads, writes);
		break;
	case EXPR_ARRAY_ACCESS:
		_collectExpr(expr->arrayAccess.array, reads, writes);
		_collectExpr(expr->arrayAccess.index, reads, writes);
		break;
	case EXPR_CALL:
		for (ExprList *argNode = expr->call.args; argNode != NULL; argNode = argNode->next) {
			_collectExpr(argNode->expr, reads, writes);
		}
		break;
	case EXPR_INTRINSIC:
		for (ExprList *argNode = expr->intrinsic.args; argNode != NULL; argNode = argNode->next) {
			_collectExpr(argNode->expr, reads, writes);
		}
		break;
	default:
		break;
	}
}

void
collectStmtAccesses(Stmt *stmt, NameSet *reads, NameSet *writes) {
	if (stmt == NULL) {
		return;
	}
	switch (stmt->kind) {
	case STMT_EXPR:
		_collectExpr(stmt->exprStmt, reads, writes);
		break;
	case STMT_LOCAL_DECL:
		_collectExpr(stmt->localDecl.arraySize, reads, writes);
		_collectExpr(stmt->localDecl.init, reads, writes);
		for (ExprList *initNode = stmt->localDecl.initList; initNode != NULL; initNode = initNode->next) {
			_collectExpr(initNode->expr, reads, writes);
		}
		break;
	case STMT_IF:
		_collectExpr(stmt->ifStmt.condition, reads, writes);
		collectStmtAccesses(stmt->ifStmt.thenBranch, reads, writes);
		collectStmtAccesses(stmt->ifStmt.elseBranch, reads, writes);
		break;
	case STMT_FOR:
		collectStmtAccesses(stmt->forStmt.init, reads, writes);
		_collectExpr(stmt->forStmt.condition, reads, writes);
		_collectExpr(stmt->forStmt.update, reads, writes);
		collectStmtAccesses(stmt->forStmt.body, reads, writes);
		break;
	case STMT_WHILE:
		_collectExpr(stmt->whileStmt.condition, reads, writes);
		collectStmtAccesses(stmt->whileStmt.body, reads, writes);
		break;
	case STMT_RETURN:
		_collectExpr(stmt->returnExpr, reads, writes);
		break;
	case STMT_COMPOUND:
		for (StmtList *stmtNode = stmt->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
			collectStmtAccesses(stmtNode->stmt, reads, writes);
		}
		break;
	case STMT_PARALLEL:
		for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
			collectStmtAccesses(stmtNode->stmt, reads, writes);
		}
		break;
	}
}
