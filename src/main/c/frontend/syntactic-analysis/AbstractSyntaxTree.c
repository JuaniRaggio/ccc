#include "AbstractSyntaxTree.h"
#include <string.h>

static Logger * _logger = NULL;

void _shutdownAbstractSyntaxTreeModule() {
    if (_logger != NULL) {
        logDebugging(_logger, "Destroying module: AbstractSyntaxTree...");
        destroyLogger(_logger);
        _logger = NULL;
    }
}

ModuleDestructor initializeAbstractSyntaxTreeModule() {
    _logger = createLogger("AbstractSyntaxTree");
    return _shutdownAbstractSyntaxTreeModule;
}

void destroyExpr(Expr *expr) {
    if (expr == NULL) return;
    switch (expr->kind) {
        case EXPR_IDENTIFIER:
            free(expr->name);
            break;
        case EXPR_BINARY:
            destroyExpr(expr->binary.left);
            destroyExpr(expr->binary.right);
            break;
        case EXPR_UNARY:
            destroyExpr(expr->unary.operand);
            break;
        case EXPR_ASSIGN:
            destroyExpr(expr->assign.target);
            destroyExpr(expr->assign.value);
            break;
        case EXPR_ARRAY_ACCESS:
            destroyExpr(expr->arrayAccess.array);
            destroyExpr(expr->arrayAccess.index);
            break;
        case EXPR_CALL:
            free(expr->call.name);
            destroyExprList(expr->call.args);
            break;
        case EXPR_INTRINSIC:
            destroyExprList(expr->intrinsic.args);
            break;
        case EXPR_POST_INC:
        case EXPR_POST_DEC:
            destroyExpr(expr->postOp);
            break;
        case EXPR_INT_LITERAL:
        case EXPR_BOOL_LITERAL:
            break;
    }
    free(expr);
}

void destroyExprList(ExprList *list) {
    while (list != NULL) {
        ExprList *next = list->next;
        destroyExpr(list->expr);
        free(list);
        list = next;
    }
}

void destroyStmt(Stmt *stmt) {
    if (stmt == NULL) return;
    switch (stmt->kind) {
        case STMT_EXPR:
            destroyExpr(stmt->exprStmt);
            break;
        case STMT_LOCAL_DECL:
            free(stmt->localDecl.name);
            destroyExpr(stmt->localDecl.arraySize);
            destroyExpr(stmt->localDecl.init);
            destroyExprList(stmt->localDecl.initList);
            break;
        case STMT_IF:
            destroyExpr(stmt->ifStmt.condition);
            destroyStmt(stmt->ifStmt.thenBranch);
            destroyStmt(stmt->ifStmt.elseBranch);
            break;
        case STMT_FOR:
            destroyStmt(stmt->forStmt.init);
            destroyExpr(stmt->forStmt.condition);
            destroyExpr(stmt->forStmt.update);
            destroyStmt(stmt->forStmt.body);
            break;
        case STMT_WHILE:
            destroyExpr(stmt->whileStmt.condition);
            destroyStmt(stmt->whileStmt.body);
            break;
        case STMT_RETURN:
            destroyExpr(stmt->returnExpr);
            break;
        case STMT_COMPOUND:
            destroyStmtList(stmt->compound);
            break;
        case STMT_PARALLEL:
            destroyStmtList(stmt->parallel);
            break;
    }
    free(stmt);
}

void destroyStmtList(StmtList *list) {
    while (list != NULL) {
        StmtList *next = list->next;
        destroyStmt(list->stmt);
        free(list);
        list = next;
    }
}

void destroyParam(Param *param) {
    if (param == NULL) return;
    free(param->name);
    destroyExpr(param->arraySize);
    free(param);
}

void destroyParamList(ParamList *list) {
    while (list != NULL) {
        ParamList *next = list->next;
        destroyParam(list->param);
        free(list);
        list = next;
    }
}

void destroyDecl(Decl *decl) {
    if (decl == NULL) return;
    switch (decl->kind) {
        case DECL_FUNCTION:
        case DECL_CONSTEXPR_FUNC:
            free(decl->func.name);
            destroyParamList(decl->func.params);
            destroyStmt(decl->func.body);
            break;
        case DECL_CONSTEXPR_VAR:
            free(decl->constexprVar.name);
            destroyExpr(decl->constexprVar.init);
            break;
        case DECL_CONSTEXPR_ARRAY:
            free(decl->constexprArray.name);
            destroyExpr(decl->constexprArray.size);
            destroyExprList(decl->constexprArray.init);
            break;
    }
    free(decl);
}

void destroyDeclList(DeclList *list) {
    while (list != NULL) {
        DeclList *next = list->next;
        destroyDecl(list->decl);
        free(list);
        list = next;
    }
}

void destroyProgram(Program *program) {
    if (program == NULL) return;
    destroyDeclList(program->decls);
    free(program);
}
