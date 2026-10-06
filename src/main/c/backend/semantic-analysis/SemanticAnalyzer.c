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
}

static void
_walkStmtExprs(Stmt *stmt, ExprVisitor visit, void *context) {
	if (stmt == NULL) {
		return;
	}
}

typedef struct {
	Decl **funcs;
	int count;
	bool *adjRow;
} CallEdgeContext;

static void
_collectCallEdges(Expr *expr, void *context) {
	CallEdgeContext *ctx = context;
	if (expr->kind != EXPR_CALL) {
		return;
	}
}

static bool
_hasCycle(const bool *adjacency, int n, int node, int *color) {
	color[node] = 1;
	for (int next = 0; next < n; ++next) {
		if (!adjacency[node * n + next]) {
			continue;
		}
		if (color[next] == 1) {
			return true;
		}
		if (color[next] == 0 && _hasCycle(adjacency, n, next, color)) {
			return true;
		}
	}
	color[node] = 2;
	return false;
}

static bool
_checkNoRecursion(Program *program) {
	int count = 0;
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (_isFunctionDecl(node->decl)) {
			++count;
		}
	}
	if (count == 0) {
		return true;
	}
	Decl **funcs = calloc(count, sizeof(Decl *));
	int index = 0;
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (_isFunctionDecl(node->decl)) {
			funcs[index++] = node->decl;
		}
	}
	bool *adjacency = calloc((size_t)count * count, sizeof(bool));
	for (int i = 0; i < count; ++i) {
		CallEdgeContext ctx = { .funcs = funcs, .count = count, .adjRow = &adjacency[i * count] };
		_walkStmtExprs(funcs[i]->func.body, _collectCallEdges, &ctx);
	}
	int *color = calloc(count, sizeof(int));
	bool cyclic = false;
	for (int i = 0; i < count && !cyclic; ++i) {
		if (color[i] == 0 && _hasCycle(adjacency, count, i, color)) {
			cyclic = true;
		}
	}
	free(color);
	free(adjacency);
	free(funcs);
	return !cyclic;
}

static bool
_checkConstexprRefParams(Program *program) {
	return true;
}

typedef struct {
	Program *program;
	bool violation;
} ConstexprCallContext;

static void
_checkConstexprCall(Expr *expr, void *context) {
	ConstexprCallContext *ctx = context;
	if (expr->kind == EXPR_CALL && !_isConstexprFunction(ctx->program, expr->call.name)) {
		ctx->violation = true;
	}
}

static bool
_checkConstexprCalls(Program *program) {
	ConstexprCallContext ctx = { .program = program, .violation = false };
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		switch (decl->kind) {
		case DECL_CONSTEXPR_VAR:
			_walkExpr(decl->constexprVar.init, _checkConstexprCall, &ctx);
			break;
		case DECL_CONSTEXPR_ARRAY:
			_walkExpr(decl->constexprArray.size, _checkConstexprCall, &ctx);
			for (ExprList *e = decl->constexprArray.init; e != NULL; e = e->next) {
				_walkExpr(e->expr, _checkConstexprCall, &ctx);
			}
			break;
		case DECL_CONSTEXPR_FUNC:
			_walkStmtExprs(decl->func.body, _checkConstexprCall, &ctx);
			break;
		default:
			break;
		}
	}
	return !ctx.violation;
}

typedef struct {
	Program *program;
	bool inConstexprFunc;
	char **params;
	int nParams;
	const char *inductionVar;
	bool violation;
} ForBoundContext;

static void
_checkBoundIdentifier(Expr *expr, void *context) {
}

static const char *
_inductionVariable(Stmt *init) {
	return NULL;
}

static void
_checkForBoundsStmt(Stmt *stmt, ForBoundContext *ctx) {
	if (stmt == NULL) {
		return;
	}
}

static bool
_checkForBounds(Program *program) {
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

static bool
_checkParallelStmt(Stmt *stmt) {
	if (stmt == NULL) {
		return true;
	}
}

static bool
_checkParallelDependencies(Program *program) {
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
