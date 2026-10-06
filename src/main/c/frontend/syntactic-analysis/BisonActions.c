#include "BisonActions.h"

/* MODULE INTERNAL STATE */

static CompilerState *_compilerState = NULL;
static Logger *_logger = NULL;

/** Shutdown module's internal state. */
void
_shutdownBisonActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: BisonActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_compilerState = NULL;
}

ModuleDestructor
initializeBisonActionsModule(CompilerState *compilerState) {
	_compilerState = compilerState;
	_logger = createLogger("BisonActions");
	return _shutdownBisonActionsModule;
}

/* IMPORTED FUNCTIONS */

/* PRIVATE FUNCTIONS */

static void _logSyntacticAnalyzerAction(const char *functionName);

/**
 * Logs a syntactic-analyzer action in DEBUGGING level.
 */
static void
_logSyntacticAnalyzerAction(const char *functionName) {
	logDebugging(_logger, "%s", functionName);
}

/* PUBLIC FUNCTIONS */

void
SyntaxErrorAction(const YYLTYPE *location, const char *message) {
	logError(_logger, "Syntax error at line %d: %s.", location->first_line, message);
}

/* Program and top-level declarations. */

Program *
ProgramSemanticAction(DeclList *decls) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Program *program = calloc(1, sizeof(Program));
	program->decls = decls;
	_compilerState->abstractSyntaxtTree = program;
	return program;
}

DeclList *
SingleDeclListSemanticAction(Decl *decl) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	DeclList *list = calloc(1, sizeof(DeclList));
	list->decl = decl;
	return list;
}

DeclList *
AppendDeclSemanticAction(DeclList *list, Decl *decl) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	DeclList *node = SingleDeclListSemanticAction(decl);
	DeclList *last = list;
	while (last->next != NULL) {
		last = last->next;
	}
	last->next = node;
	return list;
}

static Decl *
_functionDecl(DeclKind kind, TypeKind returnType, char *name, ParamList *params, Stmt *body) {
	Decl *decl = calloc(1, sizeof(Decl));
	decl->kind = kind;
	decl->func.returnType = returnType;
	decl->func.name = name;
	decl->func.params = params;
	decl->func.body = body;
	return decl;
}

Decl *
FunctionDeclSemanticAction(TypeKind returnType, char *name, ParamList *params, Stmt *body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return _functionDecl(DECL_FUNCTION, returnType, name, params, body);
}

Decl *
ConstexprFuncDeclSemanticAction(TypeKind returnType, char *name, ParamList *params, Stmt *body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return _functionDecl(DECL_CONSTEXPR_FUNC, returnType, name, params, body);
}

Decl *
ConstexprVarDeclSemanticAction(TypeKind type, char *name, Expr *init) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Decl *decl = calloc(1, sizeof(Decl));
	decl->kind = DECL_CONSTEXPR_VAR;
	decl->constexprVar.type = type;
	decl->constexprVar.name = name;
	decl->constexprVar.init = init;
	return decl;
}

Decl *
ConstexprArrayDeclSemanticAction(TypeKind type, char *name, Expr *size, ExprList *init) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Decl *decl = calloc(1, sizeof(Decl));
	decl->kind = DECL_CONSTEXPR_ARRAY;
	decl->constexprArray.type = type;
	decl->constexprArray.name = name;
	decl->constexprArray.size = size;
	decl->constexprArray.init = init;
	return decl;
}

/* Parameters. */

ParamList *
EmptyParamListSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return NULL;
}

ParamList *
SingleParamListSemanticAction(Param *param) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ParamList *list = calloc(1, sizeof(ParamList));
	list->param = param;
	return list;
}

ParamList *
AppendParamSemanticAction(ParamList *list, Param *param) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ParamList *node = SingleParamListSemanticAction(param);
	ParamList *last = list;
	while (last->next != NULL) {
		last = last->next;
	}
	last->next = node;
	return list;
}

Param *
ValueParamSemanticAction(TypeKind type, char *name) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Param *param = calloc(1, sizeof(Param));
	param->kind = PARAM_VALUE;
	param->type = type;
	param->name = name;
	return param;
}

Param *
ArrayParamSemanticAction(TypeKind type, Expr *size, char *name) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Param *param = calloc(1, sizeof(Param));
	param->kind = PARAM_ARRAY;
	param->type = type;
	param->name = name;
	param->arraySize = size;
	return param;
}

Param *
RefParamSemanticAction(TypeKind type, AliasQualifier qualifier, char *name) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Param *param = calloc(1, sizeof(Param));
	param->kind = PARAM_REF;
	param->type = type;
	param->name = name;
	param->qualifier = qualifier;
	return param;
}

/* Statements. */

static Stmt *
_stmt(StmtKind kind) {
	Stmt *stmt = calloc(1, sizeof(Stmt));
	stmt->kind = kind;
	return stmt;
}

Stmt *
CompoundStmtSemanticAction(StmtList *stmts) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Stmt *stmt = _stmt(STMT_COMPOUND);
	stmt->compound = stmts;
	return stmt;
}

Stmt *
ExprStmtSemanticAction(Expr *expr) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Stmt *stmt = _stmt(STMT_EXPR);
	stmt->exprStmt = expr;
	return stmt;
}

Stmt *
LocalDeclStmtSemanticAction(TypeKind type, char *name, Expr *init) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Stmt *stmt = _stmt(STMT_LOCAL_DECL);
	stmt->localDecl.type = type;
	stmt->localDecl.name = name;
	stmt->localDecl.init = init;
	return stmt;
}

Stmt *
LocalArrayDeclStmtSemanticAction(TypeKind type, char *name, Expr *size, ExprList *init) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Stmt *stmt = _stmt(STMT_LOCAL_DECL);
	stmt->localDecl.type = type;
	stmt->localDecl.name = name;
	stmt->localDecl.arraySize = size;
	stmt->localDecl.initList = init;
	return stmt;
}

Stmt *
IfStmtSemanticAction(Expr *cond, Stmt *thenBranch, Stmt *elseBranch) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Stmt *stmt = _stmt(STMT_IF);
	stmt->ifStmt.condition = cond;
	stmt->ifStmt.thenBranch = thenBranch;
	stmt->ifStmt.elseBranch = elseBranch;
	return stmt;
}

Stmt *
ForStmtSemanticAction(Stmt *init, Expr *cond, Expr *update, Stmt *body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Stmt *stmt = _stmt(STMT_FOR);
	stmt->forStmt.init = init;
	stmt->forStmt.condition = cond;
	stmt->forStmt.update = update;
	stmt->forStmt.body = body;
	return stmt;
}

Stmt *
WhileStmtSemanticAction(Expr *cond, Stmt *body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Stmt *stmt = _stmt(STMT_WHILE);
	stmt->whileStmt.condition = cond;
	stmt->whileStmt.body = body;
	return stmt;
}

Stmt *
ReturnStmtSemanticAction(Expr *expr) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Stmt *stmt = _stmt(STMT_RETURN);
	stmt->returnExpr = expr;
	return stmt;
}

Stmt *
ParallelBlockSemanticAction(StmtList *stmts) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Stmt *stmt = _stmt(STMT_PARALLEL);
	stmt->parallel = stmts;
	return stmt;
}

StmtList *
EmptyStmtListSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return NULL;
}

StmtList *
AppendStmtSemanticAction(StmtList *list, Stmt *stmt) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	StmtList *node = calloc(1, sizeof(StmtList));
	node->stmt = stmt;
	if (list == NULL) {
		return node;
	}
	StmtList *last = list;
	while (last->next != NULL) {
		last = last->next;
	}
	last->next = node;
	return list;
}

/* Expressions. */

static Expr *
_expr(ExprKind kind) {
	Expr *expr = calloc(1, sizeof(Expr));
	expr->kind = kind;
	return expr;
}

Expr *
IntLiteralSemanticAction(int value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expr *expr = _expr(EXPR_INT_LITERAL);
	expr->intValue = value;
	return expr;
}

Expr *
BoolLiteralSemanticAction(int value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expr *expr = _expr(EXPR_BOOL_LITERAL);
	expr->boolValue = value;
	return expr;
}

Expr *
IdentifierExprSemanticAction(char *name) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expr *expr = _expr(EXPR_IDENTIFIER);
	expr->name = name;
	return expr;
}

Expr *
BinaryExprSemanticAction(BinaryOp op, Expr *left, Expr *right) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expr *expr = _expr(EXPR_BINARY);
	expr->binary.op = op;
	expr->binary.left = left;
	expr->binary.right = right;
	return expr;
}

Expr *
UnaryExprSemanticAction(UnaryOp op, Expr *operand) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expr *expr = _expr(EXPR_UNARY);
	expr->unary.op = op;
	expr->unary.operand = operand;
	return expr;
}

Expr *
AssignExprSemanticAction(Expr *target, AssignOp op, Expr *value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expr *expr = _expr(EXPR_ASSIGN);
	expr->assign.target = target;
	expr->assign.op = op;
	expr->assign.value = value;
	return expr;
}

Expr *
ArrayAccessExprSemanticAction(Expr *array, Expr *index) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expr *expr = _expr(EXPR_ARRAY_ACCESS);
	expr->arrayAccess.array = array;
	expr->arrayAccess.index = index;
	return expr;
}

Expr *
CallExprSemanticAction(char *name, ExprList *args) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expr *expr = _expr(EXPR_CALL);
	expr->call.name = name;
	expr->call.args = args;
	return expr;
}

Expr *
IntrinsicExprSemanticAction(IntrinsicKind kind, ExprList *args) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expr *expr = _expr(EXPR_INTRINSIC);
	expr->intrinsic.kind = kind;
	expr->intrinsic.args = args;
	return expr;
}

Expr *
PostIncExprSemanticAction(Expr *operand) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expr *expr = _expr(EXPR_POST_INC);
	expr->postOp = operand;
	return expr;
}

Expr *
PostDecExprSemanticAction(Expr *operand) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expr *expr = _expr(EXPR_POST_DEC);
	expr->postOp = operand;
	return expr;
}

ExprList *
SingleExprListSemanticAction(Expr *expr) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ExprList *list = calloc(1, sizeof(ExprList));
	list->expr = expr;
	return list;
}

ExprList *
AppendExprListSemanticAction(ExprList *list, Expr *expr) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ExprList *node = SingleExprListSemanticAction(expr);
	ExprList *last = list;
	while (last->next != NULL) {
		last = last->next;
	}
	last->next = node;
	return list;
}
