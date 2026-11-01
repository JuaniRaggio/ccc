#include "AstWalk.h"

void
walkExpr(Expr *expr, ExprVisitor visit, void *context) {
	if (expr == NULL) {
		return;
	}
	visit(expr, context);
	switch (expr->kind) {
	case EXPR_BINARY:
		walkExpr(expr->binary.left, visit, context);
		walkExpr(expr->binary.right, visit, context);
		break;
	case EXPR_UNARY:
		walkExpr(expr->unary.operand, visit, context);
		break;
	case EXPR_ASSIGN:
		walkExpr(expr->assign.target, visit, context);
		walkExpr(expr->assign.value, visit, context);
		break;
	case EXPR_ARRAY_ACCESS:
		walkExpr(expr->arrayAccess.array, visit, context);
		walkExpr(expr->arrayAccess.index, visit, context);
		break;
	case EXPR_CALL:
		for (ExprList *argNode = expr->call.args; argNode != NULL; argNode = argNode->next) {
			walkExpr(argNode->expr, visit, context);
		}
		break;
	case EXPR_INTRINSIC:
		for (ExprList *argNode = expr->intrinsic.args; argNode != NULL; argNode = argNode->next) {
			walkExpr(argNode->expr, visit, context);
		}
		break;
	case EXPR_POST_INC:
	case EXPR_POST_DEC:
		walkExpr(expr->postOp, visit, context);
		break;
	default:
		break;
	}
}

void
walkStmtExprs(Stmt *stmt, ExprVisitor visit, void *context) {
	if (stmt == NULL) {
		return;
	}
	switch (stmt->kind) {
	case STMT_EXPR:
		walkExpr(stmt->exprStmt, visit, context);
		break;
	case STMT_LOCAL_DECL:
		walkExpr(stmt->localDecl.arraySize, visit, context);
		walkExpr(stmt->localDecl.init, visit, context);
		for (ExprList *initNode = stmt->localDecl.initList; initNode != NULL; initNode = initNode->next) {
			walkExpr(initNode->expr, visit, context);
		}
		break;
	case STMT_IF:
		walkExpr(stmt->ifStmt.condition, visit, context);
		walkStmtExprs(stmt->ifStmt.thenBranch, visit, context);
		walkStmtExprs(stmt->ifStmt.elseBranch, visit, context);
		break;
	case STMT_FOR:
		walkStmtExprs(stmt->forStmt.init, visit, context);
		walkExpr(stmt->forStmt.condition, visit, context);
		walkExpr(stmt->forStmt.update, visit, context);
		walkStmtExprs(stmt->forStmt.body, visit, context);
		break;
	case STMT_WHILE:
		walkExpr(stmt->whileStmt.condition, visit, context);
		walkStmtExprs(stmt->whileStmt.body, visit, context);
		break;
	case STMT_RETURN:
		walkExpr(stmt->returnExpr, visit, context);
		break;
	case STMT_COMPOUND:
		for (StmtList *stmtNode = stmt->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
			walkStmtExprs(stmtNode->stmt, visit, context);
		}
		break;
	case STMT_PARALLEL:
		for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
			walkStmtExprs(stmtNode->stmt, visit, context);
		}
		break;
	}
}

void
walkStmts(Stmt *stmt, StmtVisitor visit, void *context) {
	if (stmt == NULL) {
		return;
	}
	visit(stmt, context);
	switch (stmt->kind) {
	case STMT_IF:
		walkStmts(stmt->ifStmt.thenBranch, visit, context);
		walkStmts(stmt->ifStmt.elseBranch, visit, context);
		break;
	case STMT_FOR:
		walkStmts(stmt->forStmt.init, visit, context);
		walkStmts(stmt->forStmt.body, visit, context);
		break;
	case STMT_WHILE:
		walkStmts(stmt->whileStmt.body, visit, context);
		break;
	case STMT_COMPOUND:
		for (StmtList *stmtNode = stmt->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
			walkStmts(stmtNode->stmt, visit, context);
		}
		break;
	case STMT_PARALLEL:
		for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
			walkStmts(stmtNode->stmt, visit, context);
		}
		break;
	default:
		break;
	}
}
