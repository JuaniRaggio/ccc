#include "SemanticAnalyzer.h"
#include <string.h>

static Logger *_logger = NULL;

void
_shutdownSemanticAnalysisModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: SemanticAnalyzer...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor
initializeSemanticAnalysisModule(void) {
	_logger = createLogger("SemanticAnalyzer");
	return _shutdownSemanticAnalysisModule;
}

typedef void (*ExprVisitor)(Expr *expr, void *context);
typedef void (*StmtVisitor)(Stmt *stmt, void *context);

static bool
_isFunctionDecl(const Decl *decl) {
	return decl->kind == DECL_FUNCTION || decl->kind == DECL_CONSTEXPR_FUNC;
}

static bool
_nameInList(char **names, int count, const char *name) {
	for (int i = 0; i < count; ++i) {
		if (strcmp(names[i], name) == 0) {
			return true;
		}
	}
	return false;
}

static bool
_isConstexprGlobal(Program *program, const char *name) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (decl->kind == DECL_CONSTEXPR_VAR && strcmp(decl->constexprVar.name, name) == 0) {
			return true;
		}
		if (decl->kind == DECL_CONSTEXPR_ARRAY && strcmp(decl->constexprArray.name, name) == 0) {
			return true;
		}
	}
	return false;
}

static bool
_isConstexprFunction(Program *program, const char *name) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (decl->kind == DECL_CONSTEXPR_FUNC && strcmp(decl->func.name, name) == 0) {
			return true;
		}
	}
	return false;
}

static void
_walkExpr(Expr *expr, ExprVisitor visit, void *context) {
	if (expr == NULL) {
		return;
	}
	visit(expr, context);
	switch (expr->kind) {
	case EXPR_BINARY:
		_walkExpr(expr->binary.left, visit, context);
		_walkExpr(expr->binary.right, visit, context);
		break;
	case EXPR_UNARY:
		_walkExpr(expr->unary.operand, visit, context);
		break;
	case EXPR_ASSIGN:
		_walkExpr(expr->assign.target, visit, context);
		_walkExpr(expr->assign.value, visit, context);
		break;
	case EXPR_ARRAY_ACCESS:
		_walkExpr(expr->arrayAccess.array, visit, context);
		_walkExpr(expr->arrayAccess.index, visit, context);
		break;
	case EXPR_CALL:
		for (ExprList *argNode = expr->call.args; argNode != NULL; argNode = argNode->next) {
			_walkExpr(argNode->expr, visit, context);
		}
		break;
	case EXPR_INTRINSIC:
		for (ExprList *argNode = expr->intrinsic.args; argNode != NULL; argNode = argNode->next) {
			_walkExpr(argNode->expr, visit, context);
		}
		break;
	case EXPR_POST_INC:
	case EXPR_POST_DEC:
		_walkExpr(expr->postOp, visit, context);
		break;
	default:
		break;
	}
}

static void
_walkStmtExprs(Stmt *stmt, ExprVisitor visit, void *context) {
	if (stmt == NULL) {
		return;
	}
	switch (stmt->kind) {
	case STMT_EXPR:
		_walkExpr(stmt->exprStmt, visit, context);
		break;
	case STMT_LOCAL_DECL:
		_walkExpr(stmt->localDecl.arraySize, visit, context);
		_walkExpr(stmt->localDecl.init, visit, context);
		for (ExprList *initNode = stmt->localDecl.initList; initNode != NULL; initNode = initNode->next) {
			_walkExpr(initNode->expr, visit, context);
		}
		break;
	case STMT_IF:
		_walkExpr(stmt->ifStmt.condition, visit, context);
		_walkStmtExprs(stmt->ifStmt.thenBranch, visit, context);
		_walkStmtExprs(stmt->ifStmt.elseBranch, visit, context);
		break;
	case STMT_FOR:
		_walkStmtExprs(stmt->forStmt.init, visit, context);
		_walkExpr(stmt->forStmt.condition, visit, context);
		_walkExpr(stmt->forStmt.update, visit, context);
		_walkStmtExprs(stmt->forStmt.body, visit, context);
		break;
	case STMT_WHILE:
		_walkExpr(stmt->whileStmt.condition, visit, context);
		_walkStmtExprs(stmt->whileStmt.body, visit, context);
		break;
	case STMT_RETURN:
		_walkExpr(stmt->returnExpr, visit, context);
		break;
	case STMT_COMPOUND:
		for (StmtList *stmtNode = stmt->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
			_walkStmtExprs(stmtNode->stmt, visit, context);
		}
		break;
	case STMT_PARALLEL:
		for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
			_walkStmtExprs(stmtNode->stmt, visit, context);
		}
		break;
	}
}

static void
_walkStmts(Stmt *stmt, StmtVisitor visit, void *context) {
	if (stmt == NULL) {
		return;
	}
	visit(stmt, context);
	switch (stmt->kind) {
	case STMT_IF:
		_walkStmts(stmt->ifStmt.thenBranch, visit, context);
		_walkStmts(stmt->ifStmt.elseBranch, visit, context);
		break;
	case STMT_FOR:
		_walkStmts(stmt->forStmt.init, visit, context);
		_walkStmts(stmt->forStmt.body, visit, context);
		break;
	case STMT_WHILE:
		_walkStmts(stmt->whileStmt.body, visit, context);
		break;
	case STMT_COMPOUND:
		for (StmtList *stmtNode = stmt->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
			_walkStmts(stmtNode->stmt, visit, context);
		}
		break;
	case STMT_PARALLEL:
		for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
			_walkStmts(stmtNode->stmt, visit, context);
		}
		break;
	default:
		break;
	}
}

typedef struct {
	Decl **functions;
	int functionCount;
	bool *adjacencyRow;
} CallGraphBuilder;

static void
_collectCallEdges(Expr *expr, void *context) {
	CallGraphBuilder *graph = context;
	if (expr->kind != EXPR_CALL) {
		return;
	}
	for (int callee = 0; callee < graph->functionCount; ++callee) {
		if (strcmp(graph->functions[callee]->func.name, expr->call.name) == 0) {
			graph->adjacencyRow[callee] = true;
		}
	}
}

static bool
_hasCycle(const bool *adjacency, int functionCount, int current, int *color) {
	color[current] = 1;
	for (int next = 0; next < functionCount; ++next) {
		if (!adjacency[current * functionCount + next]) {
			continue;
		}
		if (color[next] == 1) {
			return true;
		}
		if (color[next] == 0 && _hasCycle(adjacency, functionCount, next, color)) {
			return true;
		}
	}
	color[current] = 2;
	return false;
}

static bool
_checkNoRecursion(Program *program) {
	int functionCount = 0;
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (_isFunctionDecl(node->decl)) {
			++functionCount;
		}
	}
	if (functionCount == 0) {
		return true;
	}
	Decl **functions = calloc(functionCount, sizeof(Decl *));
	int stored = 0;
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (_isFunctionDecl(node->decl)) {
			functions[stored++] = node->decl;
		}
	}
	bool *adjacency = calloc((size_t)functionCount * functionCount, sizeof(bool));
	for (int caller = 0; caller < functionCount; ++caller) {
		CallGraphBuilder graph = {
			.functions = functions,
			.functionCount = functionCount,
			.adjacencyRow = &adjacency[caller * functionCount],
		};
		_walkStmtExprs(functions[caller]->func.body, _collectCallEdges, &graph);
	}
	int *color = calloc(functionCount, sizeof(int));
	bool cyclic = false;
	for (int caller = 0; caller < functionCount && !cyclic; ++caller) {
		if (color[caller] == 0 && _hasCycle(adjacency, functionCount, caller, color)) {
			cyclic = true;
		}
	}
	free(color);
	free(adjacency);
	free(functions);
	return !cyclic;
}

static bool
_checkConstexprRefParams(Program *program) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (decl->kind != DECL_CONSTEXPR_FUNC) {
			continue;
		}
		for (ParamList *paramNode = decl->func.params; paramNode != NULL; paramNode = paramNode->next) {
			if (paramNode->param->kind == PARAM_REF) {
				return false;
			}
		}
	}
	return true;
}

typedef struct {
	Program *program;
	bool violation;
} ConstexprScope;

static void
_checkConstexprCall(Expr *expr, void *context) {
	ConstexprScope *scope = context;
	if (expr->kind == EXPR_CALL && !_isConstexprFunction(scope->program, expr->call.name)) {
		scope->violation = true;
	}
}

static bool
_checkConstexprCalls(Program *program) {
	ConstexprScope scope = { .program = program, .violation = false };
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		switch (decl->kind) {
		case DECL_CONSTEXPR_VAR:
			_walkExpr(decl->constexprVar.init, _checkConstexprCall, &scope);
			break;
		case DECL_CONSTEXPR_ARRAY:
			_walkExpr(decl->constexprArray.size, _checkConstexprCall, &scope);
			for (ExprList *initNode = decl->constexprArray.init; initNode != NULL; initNode = initNode->next) {
				_walkExpr(initNode->expr, _checkConstexprCall, &scope);
			}
			break;
		case DECL_CONSTEXPR_FUNC:
			_walkStmtExprs(decl->func.body, _checkConstexprCall, &scope);
			break;
		default:
			break;
		}
	}
	return !scope.violation;
}

typedef struct {
	Program *program;
	bool inConstexprFunc;
	char **params;
	int paramCount;
	const char *inductionVar;
	bool violation;
} ForBoundScope;

static void
_checkBoundIdentifier(Expr *expr, void *context) {
	ForBoundScope *scope = context;
	if (expr->kind != EXPR_IDENTIFIER) {
		return;
	}
	if (scope->inductionVar != NULL && strcmp(expr->name, scope->inductionVar) == 0) {
		return;
	}
	if (_isConstexprGlobal(scope->program, expr->name)) {
		return;
	}
	if (scope->inConstexprFunc && _nameInList(scope->params, scope->paramCount, expr->name)) {
		return;
	}
	scope->violation = true;
}

static const char *
_inductionVariable(Stmt *init) {
	if (init == NULL) {
		return NULL;
	}
	if (init->kind == STMT_LOCAL_DECL) {
		return init->localDecl.name;
	}
	if (init->kind == STMT_EXPR && init->exprStmt != NULL && init->exprStmt->kind == EXPR_ASSIGN
	    && init->exprStmt->assign.target != NULL && init->exprStmt->assign.target->kind == EXPR_IDENTIFIER) {
		return init->exprStmt->assign.target->name;
	}
	return NULL;
}

static void
_checkForBound(Stmt *stmt, void *context) {
	ForBoundScope *scope = context;
	if (stmt->kind != STMT_FOR) {
		return;
	}
	scope->inductionVar = _inductionVariable(stmt->forStmt.init);
	_walkExpr(stmt->forStmt.condition, _checkBoundIdentifier, scope);
}

static bool
_checkForBounds(Program *program) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (!_isFunctionDecl(decl)) {
			continue;
		}
		int paramCount = 0;
		for (ParamList *paramNode = decl->func.params; paramNode != NULL; paramNode = paramNode->next) {
			++paramCount;
		}
		char **params = paramCount > 0 ? calloc(paramCount, sizeof(char *)) : NULL;
		int stored = 0;
		for (ParamList *paramNode = decl->func.params; paramNode != NULL; paramNode = paramNode->next) {
			params[stored++] = paramNode->param->name;
		}
		ForBoundScope scope = {
			.program = program,
			.inConstexprFunc = decl->kind == DECL_CONSTEXPR_FUNC,
			.params = params,
			.paramCount = paramCount,
			.inductionVar = NULL,
			.violation = false,
		};
		_walkStmts(decl->func.body, _checkForBound, &scope);
		free(params);
		if (scope.violation) {
			return false;
		}
	}
	return true;
}

typedef struct {
	char **names;
	int count;
	int capacity;
} NameList;

static void
_collectAssignTargets(Expr *expr, void *context) {
	NameList *list = context;
	if (expr->kind != EXPR_ASSIGN || expr->assign.target == NULL || expr->assign.target->kind != EXPR_IDENTIFIER) {
		return;
	}
	if (list->count == list->capacity) {
		list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
		list->names = realloc(list->names, list->capacity * sizeof(char *));
	}
	list->names[list->count++] = expr->assign.target->name;
}

static bool
_hasDuplicateName(NameList *list) {
	for (int i = 0; i < list->count; ++i) {
		for (int j = i + 1; j < list->count; ++j) {
			if (strcmp(list->names[i], list->names[j]) == 0) {
				return true;
			}
		}
	}
	return false;
}

typedef struct {
	bool violation;
} ParallelScope;

static void
_checkParallelBlock(Stmt *stmt, void *context) {
	ParallelScope *scope = context;
	if (stmt->kind != STMT_PARALLEL) {
		return;
	}
	NameList targets = { .names = NULL, .count = 0, .capacity = 0 };
	for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
		_walkStmtExprs(stmtNode->stmt, _collectAssignTargets, &targets);
	}
	if (_hasDuplicateName(&targets)) {
		scope->violation = true;
	}
	free(targets.names);
}

static bool
_checkParallelDependencies(Program *program) {
	ParallelScope scope = { .violation = false };
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (_isFunctionDecl(node->decl)) {
			_walkStmts(node->decl->func.body, _checkParallelBlock, &scope);
		}
	}
	return !scope.violation;
}

bool
executeSemanticAnalysis(CompilerState *compilerState) {
	logDebugging(_logger, "Analyzing semantics...");
	Program *program = compilerState->abstractSyntaxtTree;
	if (program == NULL) {
		return true;
	}
	if (!_checkNoRecursion(program)) {
		logError(_logger, "Recursion is not allowed.");
		return false;
	}
	if (!_checkConstexprRefParams(program)) {
		logError(_logger, "A constexpr function cannot take reference parameters.");
		return false;
	}
	if (!_checkConstexprCalls(program)) {
		logError(_logger, "A constexpr context can only call constexpr functions.");
		return false;
	}
	if (!_checkForBounds(program)) {
		logError(_logger, "The bound of a for loop must be a compile-time constant.");
		return false;
	}
	if (!_checkParallelDependencies(program)) {
		logError(_logger, "A parallel block cannot write the same variable more than once.");
		return false;
	}
	logDebugging(_logger, "Semantic analysis succeeded.");
	return true;
}
