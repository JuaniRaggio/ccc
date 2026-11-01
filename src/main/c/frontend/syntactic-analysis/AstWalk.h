#pragma once

#include "AbstractSyntaxTree.h"

typedef void (*ExprVisitor)(Expr *expr, void *context);
typedef void (*StmtVisitor)(Stmt *stmt, void *context);

/* Visits expr and every sub-expression (pre-order). */
void walkExpr(Expr *expr, ExprVisitor visit, void *context);

/* Visits every expression reachable from stmt and its sub-statements. */
void walkStmtExprs(Stmt *stmt, ExprVisitor visit, void *context);

/* Visits stmt and every sub-statement (pre-order). */
void walkStmts(Stmt *stmt, StmtVisitor visit, void *context);
