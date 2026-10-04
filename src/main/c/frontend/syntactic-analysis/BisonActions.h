#pragma once

#include "support/logging/Logger.h"
#include "support/type/CompilerState.h"
#include "support/type/ModuleDestructor.h"
#include "AbstractSyntaxTree.h"
#include "BisonParser.h"
#include <stdlib.h>
#include <string.h>

ModuleDestructor initializeBisonActionsModule(CompilerState *compilerState);

void        SyntaxErrorAction(const YYLTYPE *location, const char *message);

Program    *ProgramSemanticAction(DeclList *decls);

DeclList   *SingleDeclListSemanticAction(Decl *decl);
DeclList   *AppendDeclSemanticAction(DeclList *list, Decl *decl);

Decl       *FunctionDeclSemanticAction(TypeKind returnType, char *name, ParamList *params, Stmt *body);
Decl       *ConstexprFuncDeclSemanticAction(TypeKind returnType, char *name, ParamList *params, Stmt *body);
Decl       *ConstexprVarDeclSemanticAction(TypeKind type, char *name, Expr *init);
Decl       *ConstexprArrayDeclSemanticAction(TypeKind type, char *name, Expr *size, ExprList *init);

ParamList  *EmptyParamListSemanticAction(void);
ParamList  *SingleParamListSemanticAction(Param *param);
ParamList  *AppendParamSemanticAction(ParamList *list, Param *param);

Param      *ValueParamSemanticAction(TypeKind type, char *name);
Param      *ArrayParamSemanticAction(TypeKind type, Expr *size, char *name);
Param      *RefParamSemanticAction(TypeKind type, AliasQualifier qualifier, char *name);

Stmt       *CompoundStmtSemanticAction(StmtList *stmts);
Stmt       *ExprStmtSemanticAction(Expr *expr);
Stmt       *LocalDeclStmtSemanticAction(TypeKind type, char *name, Expr *init);
Stmt       *LocalArrayDeclStmtSemanticAction(TypeKind type, char *name, Expr *size, ExprList *init);
Stmt       *IfStmtSemanticAction(Expr *cond, Stmt *thenBranch, Stmt *elseBranch);
Stmt       *ForStmtSemanticAction(Stmt *init, Expr *cond, Expr *update, Stmt *body);
Stmt       *WhileStmtSemanticAction(Expr *cond, Stmt *body);
Stmt       *ReturnStmtSemanticAction(Expr *expr);
Stmt       *ParallelBlockSemanticAction(StmtList *stmts);

StmtList   *EmptyStmtListSemanticAction(void);
StmtList   *AppendStmtSemanticAction(StmtList *list, Stmt *stmt);

Expr       *IntLiteralSemanticAction(int value);
Expr       *BoolLiteralSemanticAction(int value);
Expr       *IdentifierExprSemanticAction(char *name);
Expr       *BinaryExprSemanticAction(BinaryOp op, Expr *left, Expr *right);
Expr       *UnaryExprSemanticAction(UnaryOp op, Expr *operand);
Expr       *AssignExprSemanticAction(Expr *target, AssignOp op, Expr *value);
Expr       *ArrayAccessExprSemanticAction(Expr *array, Expr *index);
Expr       *CallExprSemanticAction(char *name, ExprList *args);
Expr       *IntrinsicExprSemanticAction(IntrinsicKind kind, ExprList *args);
Expr       *PostIncExprSemanticAction(Expr *expr);
Expr       *PostDecExprSemanticAction(Expr *expr);

ExprList   *SingleExprListSemanticAction(Expr *expr);
ExprList   *AppendExprListSemanticAction(ExprList *list, Expr *expr);
