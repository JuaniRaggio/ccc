#include "SemanticAnalyzer.h"
#include "frontend/semantic-analysis/ConstexprEval.h"
#include "frontend/syntactic-analysis/AccessSets.h"
#include "frontend/syntactic-analysis/AstWalk.h"
#include "frontend/syntactic-analysis/TypeInfo.h"
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
		walkStmtExprs(functions[caller]->func.body, _collectCallEdges, &graph);
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
			walkExpr(decl->constexprVar.init, _checkConstexprCall, &scope);
			break;
		case DECL_CONSTEXPR_ARRAY:
			walkExpr(decl->constexprArray.size, _checkConstexprCall, &scope);
			for (ExprList *initNode = decl->constexprArray.init; initNode != NULL; initNode = initNode->next) {
				walkExpr(initNode->expr, _checkConstexprCall, &scope);
			}
			break;
		case DECL_CONSTEXPR_FUNC:
			walkStmtExprs(decl->func.body, _checkConstexprCall, &scope);
			break;
		default:
			break;
		}
	}
	return !scope.violation;
}

static void
_checkRuntimeCall(Expr *expr, void *context) {
	ConstexprScope *scope = context;
	if (expr->kind == EXPR_CALL && _isConstexprFunction(scope->program, expr->call.name)) {
		scope->violation = true;
	}
}

static bool
_checkRuntimeCallsConstexpr(Program *program) {
	ConstexprScope scope = { .program = program, .violation = false };
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (node->decl->kind == DECL_FUNCTION) {
			walkStmtExprs(node->decl->func.body, _checkRuntimeCall, &scope);
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
_checkLoopBound(Stmt *stmt, void *context) {
	ForBoundScope *scope = context;
	if (stmt->kind == STMT_FOR) {
		scope->inductionVar = _inductionVariable(stmt->forStmt.init);
		walkExpr(stmt->forStmt.condition, _checkBoundIdentifier, scope);
		/* The step must also be compile-time constant, or the trip count is dynamic
		 * (not statically unrollable) even when the condition bound is constant. */
		walkExpr(stmt->forStmt.update, _checkBoundIdentifier, scope);
	} else if (stmt->kind == STMT_WHILE) {
		scope->inductionVar = NULL;
		walkExpr(stmt->whileStmt.condition, _checkBoundIdentifier, scope);
	}
}

static bool
_checkLoopBounds(Program *program) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (!_isFunctionDecl(decl)) {
			continue;
		}
		/* A sequential function compiles to an FSM, so data-dependent loop bounds are allowed. */
		if (decl->kind == DECL_FUNCTION && decl->func.sequential) {
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
		walkStmts(decl->func.body, _checkLoopBound, &scope);
		free(params);
		if (scope.violation) {
			return false;
		}
	}
	return true;
}

typedef struct {
	Program *program;
	bool violation;
} ParallelScope;

static bool
_statementsDepend(NameSet *readsA, NameSet *writesA, NameSet *readsB, NameSet *writesB) {
	return nameSetsIntersect(writesA, writesB) || nameSetsIntersect(writesA, readsB)
	       || nameSetsIntersect(readsA, writesB);
}

static void
_checkParallelBlock(Stmt *stmt, void *context) {
	ParallelScope *scope = context;
	if (stmt->kind != STMT_PARALLEL) {
		return;
	}
	int units = 0;
	for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
		++units;
	}
	if (units == 0) {
		return;
	}
	NameSet *reads = calloc(units, sizeof(NameSet));
	NameSet *writes = calloc(units, sizeof(NameSet));
	int index = 0;
	for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
		collectStmtAccesses(stmtNode->stmt, scope->program, &reads[index], &writes[index]);
		++index;
	}
	for (int i = 0; i < units; ++i) {
		for (int j = i + 1; j < units; ++j) {
			if (_statementsDepend(&reads[i], &writes[i], &reads[j], &writes[j])) {
				scope->violation = true;
			}
		}
	}
	for (int i = 0; i < units; ++i) {
		nameSetFree(&reads[i]);
		nameSetFree(&writes[i]);
	}
	free(reads);
	free(writes);
}

static bool
_checkParallelDependencies(Program *program) {
	ParallelScope scope = { .program = program, .violation = false };
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (_isFunctionDecl(node->decl)) {
			walkStmts(node->decl->func.body, _checkParallelBlock, &scope);
		}
	}
	return !scope.violation;
}

typedef enum { CAT_INTEGER, CAT_BOOL, CAT_VOID } TypeCategory;

static TypeCategory
_category(TypeKind type) {
	if (type == TYPE_BOOL) {
		return CAT_BOOL;
	}
	if (type == TYPE_VOID) {
		return CAT_VOID;
	}
	return CAT_INTEGER;
}

/*
 * True if a value of type rhsType (produced by rhs) may be converted to target
 * without an explicit cast: same category, and for integers either identical
 * types, a value-preserving widening, or a compile-time constant that fits.
 */
static bool
_conversionAllowed(Expr *rhs, TypeKind rhsType, TypeKind target, Program *program) {
	if (_category(rhsType) != _category(target)) {
		return false;
	}
	if (_category(target) != CAT_INTEGER || rhsType == target) {
		return true;
	}
	int64_t value;
	if (evalConstexpr(rhs, program, &value)) {
		return valueFitsType(value, target);
	}
	return typeSigned(rhsType) == typeSigned(target) && typeWidth(target) >= typeWidth(rhsType);
}

typedef struct {
	char *name;
	TypeKind type;
	bool isArray;
} Symbol;

typedef struct Scope {
	Symbol *symbols;
	int count;
	int capacity;
	struct Scope *parent;
} Scope;

static void
_scopeDeclare(Scope *scope, char *name, TypeKind type, bool isArray) {
	if (scope->count == scope->capacity) {
		scope->capacity = scope->capacity == 0 ? 8 : scope->capacity * 2;
		scope->symbols = realloc(scope->symbols, scope->capacity * sizeof(Symbol));
	}
	scope->symbols[scope->count].name = name;
	scope->symbols[scope->count].type = type;
	scope->symbols[scope->count].isArray = isArray;
	scope->count++;
}

static TypeKind
_scopeLookup(Scope *scope, const char *name, bool *found, bool *isArray) {
	for (Scope *current = scope; current != NULL; current = current->parent) {
		for (int i = 0; i < current->count; ++i) {
			if (strcmp(current->symbols[i].name, name) == 0) {
				*found = true;
				if (isArray != NULL) {
					*isArray = current->symbols[i].isArray;
				}
				return current->symbols[i].type;
			}
		}
	}
	*found = false;
	return TYPE_INT32;
}

static bool
_isArrayName(Scope *scope, Program *program, const char *name) {
	bool found = false;
	bool isArray = false;
	_scopeLookup(scope, name, &found, &isArray);
	if (found) {
		return isArray;
	}
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (node->decl->kind == DECL_CONSTEXPR_ARRAY && strcmp(node->decl->constexprArray.name, name) == 0) {
			return true;
		}
	}
	return false;
}

static Decl *
_findFunction(Program *program, const char *name) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (_isFunctionDecl(node->decl) && strcmp(node->decl->func.name, name) == 0) {
			return node->decl;
		}
	}
	return NULL;
}

static TypeKind
_constexprGlobalType(Program *program, const char *name, bool *found) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (decl->kind == DECL_CONSTEXPR_VAR && strcmp(decl->constexprVar.name, name) == 0) {
			*found = true;
			return decl->constexprVar.type;
		}
		if (decl->kind == DECL_CONSTEXPR_ARRAY && strcmp(decl->constexprArray.name, name) == 0) {
			*found = true;
			return decl->constexprArray.type;
		}
	}
	*found = false;
	return TYPE_INT32;
}

/* The spec requires both operands of a binary op to share signedness; mixing needs an
 * explicit cast. A constant operand is exempt: it adapts to any integer type that
 * represents its value (so `uint32_t x + 1` is fine, `int32_t a + uint32_t b` is not). */
static bool
_mixedSignedness(Expr *left, TypeKind lt, Expr *right, TypeKind rt, Program *program) {
	if (_category(lt) != CAT_INTEGER || _category(rt) != CAT_INTEGER || typeSigned(lt) == typeSigned(rt)) {
		return false;
	}
	int64_t value;
	if (evalConstexpr(left, program, &value) && valueFitsType(value, rt)) {
		return false;
	}
	if (evalConstexpr(right, program, &value) && valueFitsType(value, lt)) {
		return false;
	}
	return true;
}

/* The type of an arithmetic result is the common type: an adapting constant takes the
 * other operand's type, otherwise the wider operand wins. Returning the left operand's
 * type unconditionally would both over-reject (`1 + b`) and silently narrow
 * (`int8 + int32` assigned to int8). Operands share signedness (mixing is rejected). */
static TypeKind
_commonArithmeticType(Expr *left, TypeKind lt, Expr *right, TypeKind rt, Program *program) {
	int64_t value;
	bool leftAdapts = evalConstexpr(left, program, &value) && valueFitsType(value, rt);
	bool rightAdapts = evalConstexpr(right, program, &value) && valueFitsType(value, lt);
	if (leftAdapts && !rightAdapts) {
		return rt;
	}
	if (rightAdapts && !leftAdapts) {
		return lt;
	}
	return typeWidth(lt) >= typeWidth(rt) ? lt : rt;
}

static TypeKind
_inferType(Expr *expr, Scope *scope, Program *program, bool *ok) {
	if (expr == NULL) {
		return TYPE_VOID;
	}
	switch (expr->kind) {
	case EXPR_INT_LITERAL:
		return TYPE_INT32;
	case EXPR_BOOL_LITERAL:
		return TYPE_BOOL;
	case EXPR_IDENTIFIER: {
		bool found = false;
		TypeKind type = _scopeLookup(scope, expr->name, &found, NULL);
		if (!found) {
			type = _constexprGlobalType(program, expr->name, &found);
		}
		if (!found) {
			logError(_logger, "Reference to undeclared name '%s'.", expr->name);
			*ok = false;
		}
		return type;
	}
	case EXPR_BINARY: {
		TypeKind left = _inferType(expr->binary.left, scope, program, ok);
		TypeKind right = _inferType(expr->binary.right, scope, program, ok);
		switch (expr->binary.op) {
		case BINOP_ADD:
		case BINOP_SUB:
		case BINOP_MUL:
			if (_category(left) != CAT_INTEGER || _category(right) != CAT_INTEGER) {
				logError(_logger, "Arithmetic operator requires integer operands.");
				*ok = false;
			} else if (_mixedSignedness(expr->binary.left, left, expr->binary.right, right, program)) {
				logError(_logger, "Mixing signed and unsigned operands requires an explicit cast.");
				*ok = false;
			}
			return _commonArithmeticType(expr->binary.left, left, expr->binary.right, right, program);
		case BINOP_LSHIFT:
		case BINOP_RSHIFT: {
			if (_category(left) != CAT_INTEGER || _category(right) != CAT_INTEGER) {
				logError(_logger, "Arithmetic operator requires integer operands.");
				*ok = false;
				return left;
			}
			int64_t count;
			if (evalConstexpr(expr->binary.right, program, &count) && (count < 0 || count >= typeWidth(left))) {
				logError(_logger, "Shift count must be non-negative and less than the operand width.");
				*ok = false;
			}
			return left;
		}
		case BINOP_LT:
		case BINOP_GT:
		case BINOP_LE:
		case BINOP_GE:
			if (_category(left) != CAT_INTEGER || _category(right) != CAT_INTEGER) {
				logError(_logger, "Relational operator requires integer operands.");
				*ok = false;
			} else if (_mixedSignedness(expr->binary.left, left, expr->binary.right, right, program)) {
				logError(_logger, "Mixing signed and unsigned operands requires an explicit cast.");
				*ok = false;
			}
			return TYPE_BOOL;
		case BINOP_EQ:
		case BINOP_NEQ:
			if (_category(left) != _category(right) || _category(left) == CAT_VOID) {
				logError(_logger, "Equality operator requires operands of the same non-void category.");
				*ok = false;
			} else if (_mixedSignedness(expr->binary.left, left, expr->binary.right, right, program)) {
				logError(_logger, "Mixing signed and unsigned operands requires an explicit cast.");
				*ok = false;
			}
			return TYPE_BOOL;
		default:
			if (_category(left) != CAT_BOOL || _category(right) != CAT_BOOL) {
				logError(_logger, "Logical operator requires boolean operands.");
				*ok = false;
			}
			return TYPE_BOOL;
		}
	}
	case EXPR_UNARY: {
		TypeKind operand = _inferType(expr->unary.operand, scope, program, ok);
		if (expr->unary.op == UNOP_NEG) {
			if (_category(operand) != CAT_INTEGER) {
				logError(_logger, "Unary minus requires an integer operand.");
				*ok = false;
			}
			return operand;
		}
		if (_category(operand) != CAT_BOOL) {
			logError(_logger, "Logical negation requires a boolean operand.");
			*ok = false;
		}
		return TYPE_BOOL;
	}
	case EXPR_ASSIGN: {
		TypeKind target = _inferType(expr->assign.target, scope, program, ok);
		TypeKind value = _inferType(expr->assign.value, scope, program, ok);
		if (!_conversionAllowed(expr->assign.value, value, target, program)) {
			logError(_logger, "Assignment between incompatible types (narrowing requires a cast).");
			*ok = false;
		}
		return target;
	}
	case EXPR_CAST: {
		TypeKind operandType = _inferType(expr->cast.operand, scope, program, ok);
		if (expr->cast.castKind == CAST_REINTERPRET && typeWidth(operandType) != typeWidth(expr->cast.type)) {
			logError(_logger, "bit_cast requires operand and target of the same width.");
			*ok = false;
		}
		return expr->cast.type;
	}
	case EXPR_ARRAY_ACCESS: {
		TypeKind element = _inferType(expr->arrayAccess.array, scope, program, ok);
		TypeKind index = _inferType(expr->arrayAccess.index, scope, program, ok);
		if (expr->arrayAccess.array->kind == EXPR_IDENTIFIER
		    && !_isArrayName(scope, program, expr->arrayAccess.array->name)) {
			logError(_logger, "Cannot index non-array '%s'.", expr->arrayAccess.array->name);
			*ok = false;
		}
		if (_category(index) != CAT_INTEGER) {
			logError(_logger, "Array index must be an integer.");
			*ok = false;
		}
		return element;
	}
	case EXPR_CALL: {
		Decl *function = _findFunction(program, expr->call.name);
		if (function == NULL) {
			logError(_logger, "Call to undeclared function '%s'.", expr->call.name);
			*ok = false;
			return TYPE_INT32;
		}
		ParamList *param = function->func.params;
		bool extraArguments = false;
		for (ExprList *argNode = expr->call.args; argNode != NULL; argNode = argNode->next) {
			TypeKind argument = _inferType(argNode->expr, scope, program, ok);
			if (param != NULL) {
				if (!_conversionAllowed(argNode->expr, argument, param->param->type, program)) {
					logError(_logger, "Argument type mismatch in call to '%s'.", expr->call.name);
					*ok = false;
				}
				param = param->next;
			} else {
				extraArguments = true;
			}
		}
		if (param != NULL || extraArguments) {
			logError(_logger, "Wrong number of arguments in call to '%s'.", expr->call.name);
			*ok = false;
		}
		return function->func.returnType;
	}
	case EXPR_INTRINSIC: {
		/* The result type follows the operands (like arithmetic), not a hardcoded int32:
		 * __abs keeps the operand type; __min/__max take the common type and, like binary
		 * ops, reject mixed signedness. */
		ExprList *args = expr->intrinsic.args;
		Expr *first = args != NULL ? args->expr : NULL;
		Expr *second = (args != NULL && args->next != NULL) ? args->next->expr : NULL;
		TypeKind firstType = _inferType(first, scope, program, ok);
		if (first != NULL && _category(firstType) != CAT_INTEGER) {
			logError(_logger, "Builtin requires integer arguments.");
			*ok = false;
		}
		if (second == NULL) {
			return firstType;
		}
		TypeKind secondType = _inferType(second, scope, program, ok);
		if (_category(secondType) != CAT_INTEGER) {
			logError(_logger, "Builtin requires integer arguments.");
			*ok = false;
		} else if (_mixedSignedness(first, firstType, second, secondType, program)) {
			logError(_logger, "Mixing signed and unsigned operands requires an explicit cast.");
			*ok = false;
		}
		return _commonArithmeticType(first, firstType, second, secondType, program);
	}
	default:
		return _inferType(expr->postOp, scope, program, ok);
	}
}

static void _typeCheckStmt(Stmt *stmt, Scope *scope, Program *program, TypeKind returnType, bool *ok);

static void
_typeCheckNested(Stmt *stmt, Scope *parent, Program *program, TypeKind returnType, bool *ok) {
	Scope child = { .symbols = NULL, .count = 0, .capacity = 0, .parent = parent };
	_typeCheckStmt(stmt, &child, program, returnType, ok);
	free(child.symbols);
}

static void
_typeCheckCondition(Expr *condition, Scope *scope, Program *program, bool *ok) {
	if (condition != NULL && _category(_inferType(condition, scope, program, ok)) != CAT_BOOL) {
		logError(_logger, "A condition must be a boolean expression.");
		*ok = false;
	}
}

static void
_typeCheckStmt(Stmt *stmt, Scope *scope, Program *program, TypeKind returnType, bool *ok) {
	if (stmt == NULL) {
		return;
	}
	switch (stmt->kind) {
	case STMT_EXPR:
		_inferType(stmt->exprStmt, scope, program, ok);
		break;
	case STMT_LOCAL_DECL:
		if (stmt->localDecl.arraySize != NULL
		    && _category(_inferType(stmt->localDecl.arraySize, scope, program, ok)) != CAT_INTEGER) {
			logError(_logger, "Array size must be an integer.");
			*ok = false;
		}
		if (stmt->localDecl.init != NULL) {
			TypeKind initType = _inferType(stmt->localDecl.init, scope, program, ok);
			if (!_conversionAllowed(stmt->localDecl.init, initType, stmt->localDecl.type, program)) {
				logError(_logger, "Initializer type does not match the declared type.");
				*ok = false;
			}
		}
		for (ExprList *initNode = stmt->localDecl.initList; initNode != NULL; initNode = initNode->next) {
			TypeKind elementType = _inferType(initNode->expr, scope, program, ok);
			if (!_conversionAllowed(initNode->expr, elementType, stmt->localDecl.type, program)) {
				logError(_logger, "Array initializer type does not match the declared type.");
				*ok = false;
			}
		}
		_scopeDeclare(scope, stmt->localDecl.name, stmt->localDecl.type, stmt->localDecl.arraySize != NULL);
		break;
	case STMT_IF:
		_typeCheckCondition(stmt->ifStmt.condition, scope, program, ok);
		_typeCheckNested(stmt->ifStmt.thenBranch, scope, program, returnType, ok);
		_typeCheckNested(stmt->ifStmt.elseBranch, scope, program, returnType, ok);
		break;
	case STMT_FOR: {
		Scope forScope = { .symbols = NULL, .count = 0, .capacity = 0, .parent = scope };
		_typeCheckStmt(stmt->forStmt.init, &forScope, program, returnType, ok);
		_typeCheckCondition(stmt->forStmt.condition, &forScope, program, ok);
		_inferType(stmt->forStmt.update, &forScope, program, ok);
		_typeCheckNested(stmt->forStmt.body, &forScope, program, returnType, ok);
		free(forScope.symbols);
		break;
	}
	case STMT_WHILE:
		_typeCheckCondition(stmt->whileStmt.condition, scope, program, ok);
		_typeCheckNested(stmt->whileStmt.body, scope, program, returnType, ok);
		break;
	case STMT_RETURN:
		if (stmt->returnExpr == NULL) {
			if (_category(returnType) != CAT_VOID) {
				logError(_logger, "A non-void function must return a value.");
				*ok = false;
			}
		} else {
			TypeKind actual = _inferType(stmt->returnExpr, scope, program, ok);
			if (_category(returnType) == CAT_VOID) {
				logError(_logger, "A void function cannot return a value.");
				*ok = false;
			} else if (!_conversionAllowed(stmt->returnExpr, actual, returnType, program)) {
				logError(_logger, "Return type does not match the function signature.");
				*ok = false;
			}
		}
		break;
	case STMT_COMPOUND: {
		Scope child = { .symbols = NULL, .count = 0, .capacity = 0, .parent = scope };
		for (StmtList *stmtNode = stmt->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
			_typeCheckStmt(stmtNode->stmt, &child, program, returnType, ok);
		}
		free(child.symbols);
		break;
	}
	case STMT_PARALLEL: {
		Scope child = { .symbols = NULL, .count = 0, .capacity = 0, .parent = scope };
		for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
			_typeCheckStmt(stmtNode->stmt, &child, program, returnType, ok);
		}
		free(child.symbols);
		break;
	}
	}
}

static bool
_checkTypes(Program *program) {
	bool ok = true;
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (_isFunctionDecl(decl)) {
			Scope funcScope = { .symbols = NULL, .count = 0, .capacity = 0, .parent = NULL };
			for (ParamList *paramNode = decl->func.params; paramNode != NULL; paramNode = paramNode->next) {
				if (paramNode->param->kind == PARAM_ARRAY) {
					_inferType(paramNode->param->arraySize, &funcScope, program, &ok);
				}
				_scopeDeclare(&funcScope, paramNode->param->name, paramNode->param->type,
				              paramNode->param->kind == PARAM_ARRAY);
			}
			_typeCheckStmt(decl->func.body, &funcScope, program, decl->func.returnType, &ok);
			free(funcScope.symbols);
		} else if (decl->kind == DECL_CONSTEXPR_VAR) {
			if (decl->constexprVar.init != NULL) {
				TypeKind initType = _inferType(decl->constexprVar.init, NULL, program, &ok);
				if (!_conversionAllowed(decl->constexprVar.init, initType, decl->constexprVar.type, program)) {
					logError(_logger, "Constexpr initializer overflows or mismatches the declared type.");
					ok = false;
				}
			}
		} else if (decl->kind == DECL_CONSTEXPR_ARRAY) {
			_inferType(decl->constexprArray.size, NULL, program, &ok);
			for (ExprList *initNode = decl->constexprArray.init; initNode != NULL; initNode = initNode->next) {
				TypeKind elementType = _inferType(initNode->expr, NULL, program, &ok);
				if (!_conversionAllowed(initNode->expr, elementType, decl->constexprArray.type, program)) {
					logError(_logger, "Constexpr array initializer overflows or mismatches the declared type.");
					ok = false;
				}
			}
		}
	}
	return ok;
}

typedef struct {
	Program *program;
	bool violation;
} AliasScope;

/* A reference argument as a memory slice: its base symbol plus, for an array element,
 * the index (resolved to a constant when possible). `base == NULL` means it is not a
 * plain lvalue. */
typedef struct {
	const char *base;
	bool indexed;
	bool constantIndex;
	int64_t index;
	bool unique;
} RefSlice;

static RefSlice
_refSlice(Expr *expr, Program *program) {
	RefSlice slice = { 0 };
	if (expr == NULL) {
		return slice;
	}
	if (expr->kind == EXPR_IDENTIFIER) {
		slice.base = expr->name;
	} else if (expr->kind == EXPR_ARRAY_ACCESS && expr->arrayAccess.array->kind == EXPR_IDENTIFIER) {
		slice.base = expr->arrayAccess.array->name;
		slice.indexed = true;
		slice.constantIndex = evalConstexpr(expr->arrayAccess.index, program, &slice.index);
	}
	return slice;
}

/* Two slices on the same base overlap unless both are array elements with distinct
 * constant indices (a[0] and a[1] are disjoint; a[i] and a[j] are not provably so). */
static bool
_slicesOverlap(const RefSlice *a, const RefSlice *b) {
	if (a->base == NULL || b->base == NULL || strcmp(a->base, b->base) != 0) {
		return false;
	}
	if (!a->indexed || !b->indexed) {
		return true;
	}
	if (a->constantIndex && b->constantIndex) {
		return a->index == b->index;
	}
	return true;
}

static void
_checkAliasingCall(Expr *expr, void *context) {
	AliasScope *scope = context;
	if (expr->kind != EXPR_CALL) {
		return;
	}
	Decl *function = _findFunction(scope->program, expr->call.name);
	if (function == NULL) {
		return;
	}
	int count = 0;
	for (ExprList *arg = expr->call.args; arg != NULL; arg = arg->next) {
		++count;
	}
	if (count == 0) {
		return;
	}
	RefSlice *slices = calloc(count, sizeof(*slices));
	int index = 0;
	ParamList *param = function->func.params;
	for (ExprList *arg = expr->call.args; arg != NULL && param != NULL; arg = arg->next, param = param->next, ++index) {
		if (param->param->kind == PARAM_REF) {
			slices[index] = _refSlice(arg->expr, scope->program);
			slices[index].unique = param->param->qualifier == QUALIFIER_UNIQUE;
		}
	}
	for (int i = 0; i < count; ++i) {
		for (int j = i + 1; j < count; ++j) {
			if ((slices[i].unique || slices[j].unique) && _slicesOverlap(&slices[i], &slices[j])) {
				logError(_logger, "Argument '%s' aliases a unique reference in call to '%s'.", slices[i].base,
				         expr->call.name);
				scope->violation = true;
			}
		}
	}
	free(slices);
}

static bool
_checkAliasing(Program *program) {
	AliasScope scope = { .program = program, .violation = false };
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (_isFunctionDecl(node->decl)) {
			walkStmtExprs(node->decl->func.body, _checkAliasingCall, &scope);
		}
	}
	return !scope.violation;
}

/* The sequential (FSM) backend compiles a body shaped as preamble + one top-level
 * loop + epilogue. It cannot represent nested or multiple loops, calls, parallel
 * blocks, or a return nested inside control flow, so those are rejected here instead
 * of being silently dropped by the generator. */
typedef struct {
	bool hasCall;
	bool hasParallel;
	int loopCount;
	int returnCount;
} SeqScan;

static void
_seqScanExpr(Expr *expr, void *context) {
	if (expr->kind == EXPR_CALL) {
		((SeqScan *)context)->hasCall = true;
	}
}

static void
_seqScanStmt(Stmt *stmt, void *context) {
	SeqScan *scan = context;
	if (stmt->kind == STMT_FOR || stmt->kind == STMT_WHILE) {
		++scan->loopCount;
	} else if (stmt->kind == STMT_PARALLEL) {
		scan->hasParallel = true;
	} else if (stmt->kind == STMT_RETURN) {
		++scan->returnCount;
	}
}

static bool
_sequentialBodyIsRepresentable(Stmt *body) {
	SeqScan scan = { 0 };
	walkStmts(body, _seqScanStmt, &scan);
	walkStmtExprs(body, _seqScanExpr, &scan);
	if (scan.hasCall || scan.hasParallel || scan.loopCount > 1) {
		return false;
	}
	int topLoops = 0;
	int topReturns = 0;
	for (StmtList *node = body->compound; node != NULL; node = node->next) {
		if (node->stmt->kind == STMT_FOR || node->stmt->kind == STMT_WHILE) {
			++topLoops;
		} else if (node->stmt->kind == STMT_RETURN) {
			++topReturns;
		}
	}
	/* Every loop and return must sit at the top level of the body. */
	return topLoops == scan.loopCount && topReturns == scan.returnCount;
}

/* Statements that share a cycle become concurrent nonblocking register updates, so a
 * read sees the value latched at the previous edge. If a statement reads a variable an
 * earlier statement in the same cycle wrote (RAW), the FSM miscompiles the sequential
 * reading. A local declaration becomes a register, so its name is a write. */
static void
_addDeclWrite(Stmt *stmt, void *context) {
	if (stmt->kind == STMT_LOCAL_DECL && (stmt->localDecl.init != NULL || stmt->localDecl.initList != NULL)) {
		nameSetAdd((NameSet *)context, stmt->localDecl.name);
	}
}

static void
_seqStmtAccesses(Stmt *stmt, NameSet *reads, NameSet *writes) {
	/* A sequential body has no calls (rejected earlier), so no call propagation is needed. */
	collectStmtAccesses(stmt, NULL, reads, writes);
	walkStmts(stmt, _addDeclWrite, writes);
}

static bool
_cycleGroupHasRAW(Stmt **stmts, int count) {
	if (count <= 1) {
		return false;
	}
	NameSet *reads = calloc(count, sizeof(NameSet));
	NameSet *writes = calloc(count, sizeof(NameSet));
	for (int i = 0; i < count; ++i) {
		_seqStmtAccesses(stmts[i], &reads[i], &writes[i]);
	}
	bool hazard = false;
	for (int j = 0; j < count && !hazard; ++j) {
		for (int i = 0; i < j; ++i) {
			if (nameSetsIntersect(&writes[i], &reads[j])) {
				hazard = true;
				break;
			}
		}
	}
	for (int i = 0; i < count; ++i) {
		nameSetFree(&reads[i]);
		nameSetFree(&writes[i]);
	}
	free(reads);
	free(writes);
	return hazard;
}

/* Checks each of the FSM's cycle-groups (IDLE init, loop iteration, epilogue) for a
 * read-after-write that the nonblocking emission would miscompile. */
static bool
_sequentialBodyHasHazard(Stmt *body) {
	Stmt *loop = NULL;
	int capacity = 0;
	for (StmtList *node = body->compound; node != NULL; node = node->next) {
		++capacity;
		if (loop == NULL && (node->stmt->kind == STMT_FOR || node->stmt->kind == STMT_WHILE)) {
			loop = node->stmt;
		}
	}
	Stmt *loopBody = NULL;
	if (loop != NULL) {
		loopBody = loop->kind == STMT_FOR ? loop->forStmt.body : loop->whileStmt.body;
		if (loopBody != NULL && loopBody->kind == STMT_COMPOUND) {
			for (StmtList *node = loopBody->compound; node != NULL; node = node->next) {
				++capacity;
			}
		} else if (loopBody != NULL) {
			++capacity;
		}
	}
	capacity += 2; /* for-loop init and update */

	Stmt **group = malloc(capacity * sizeof(Stmt *));
	Stmt updateStmt = { .kind = STMT_EXPR };
	bool hazard = false;

	/* Group 1 (IDLE): preamble statements plus the for-loop init. With no loop, the whole
	 * body runs in this single group. */
	int count = 0;
	for (StmtList *node = body->compound; node != NULL; node = node->next) {
		if (node->stmt == loop) {
			break;
		}
		group[count++] = node->stmt;
	}
	if (loop != NULL && loop->kind == STMT_FOR && loop->forStmt.init != NULL) {
		group[count++] = loop->forStmt.init;
	}
	hazard = _cycleGroupHasRAW(group, count);

	/* Group 2 (RUN iteration): loop body statements plus the for-loop update. */
	if (!hazard && loop != NULL) {
		count = 0;
		if (loopBody != NULL && loopBody->kind == STMT_COMPOUND) {
			for (StmtList *node = loopBody->compound; node != NULL; node = node->next) {
				group[count++] = node->stmt;
			}
		} else if (loopBody != NULL) {
			group[count++] = loopBody;
		}
		if (loop->kind == STMT_FOR && loop->forStmt.update != NULL) {
			updateStmt.exprStmt = loop->forStmt.update;
			group[count++] = &updateStmt;
		}
		hazard = _cycleGroupHasRAW(group, count);
	}

	/* Group 3 (epilogue): top-level statements after the loop, including the return. */
	if (!hazard && loop != NULL) {
		count = 0;
		bool afterLoop = false;
		for (StmtList *node = body->compound; node != NULL; node = node->next) {
			if (node->stmt == loop) {
				afterLoop = true;
				continue;
			}
			if (afterLoop) {
				group[count++] = node->stmt;
			}
		}
		hazard = _cycleGroupHasRAW(group, count);
	}

	free(group);
	return hazard;
}

static bool
_isSequentialFunction(Program *program, const char *name) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (node->decl->kind == DECL_FUNCTION && node->decl->func.sequential
		    && strcmp(node->decl->func.name, name) == 0) {
			return true;
		}
	}
	return false;
}

typedef struct {
	Program *program;
	bool violation;
} SeqCallScope;

static void
_seqCallCheck(Expr *expr, void *context) {
	SeqCallScope *scope = context;
	if (expr->kind == EXPR_CALL && _isSequentialFunction(scope->program, expr->call.name)) {
		scope->violation = true;
	}
}

static bool
_checkSequential(Program *program) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (!_isFunctionDecl(decl)) {
			continue;
		}
		/* A sequential module exposes a clk/start/done handshake, not a dataflow port,
		 * so it cannot be instantiated by the combinational call path. */
		SeqCallScope callScope = { .program = program, .violation = false };
		walkStmtExprs(decl->func.body, _seqCallCheck, &callScope);
		if (callScope.violation) {
			logError(_logger, "A __sequential function cannot be called; it exposes a start/done handshake.");
			return false;
		}
		if (decl->kind != DECL_FUNCTION || !decl->func.sequential) {
			continue;
		}
		if (!_sequentialBodyIsRepresentable(decl->func.body)) {
			logError(_logger,
			         "A __sequential function admits one top-level loop, no nested loops, calls or parallel blocks.");
			return false;
		}
		if (_sequentialBodyHasHazard(decl->func.body)) {
			logError(_logger, "A __sequential cycle cannot read a value written earlier in the same cycle; split "
			                  "the dependency across iterations.");
			return false;
		}
	}
	return true;
}

/* A non-void function becomes combinational logic (or an FSM) driving `out`; if some
 * path leaves `out` unassigned, synthesis infers a latch. A path definitely returns
 * when it ends in `return expr`, or in an if/else whose both branches do, or in a block
 * containing such a statement. Loops do not count: their body may never execute. */
static bool
_definitelyReturns(Stmt *stmt) {
	if (stmt == NULL) {
		return false;
	}
	switch (stmt->kind) {
	case STMT_RETURN:
		return stmt->returnExpr != NULL;
	case STMT_IF:
		return stmt->ifStmt.elseBranch != NULL && _definitelyReturns(stmt->ifStmt.thenBranch)
		       && _definitelyReturns(stmt->ifStmt.elseBranch);
	case STMT_COMPOUND:
		for (StmtList *node = stmt->compound; node != NULL; node = node->next) {
			if (_definitelyReturns(node->stmt)) {
				return true;
			}
		}
		return false;
	default:
		return false;
	}
}

static bool
_checkReturns(Program *program) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (decl->kind != DECL_FUNCTION || decl->func.returnType == TYPE_VOID) {
			continue;
		}
		if (!_definitelyReturns(decl->func.body)) {
			return false;
		}
	}
	return true;
}

/* Every constexpr declaration must fold to a constant at compile time; otherwise the
 * generator would emit a non-constant (or malformed) localparam instead of a value. */
static bool
_checkConstexprEvaluable(Program *program) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		int64_t value;
		if (decl->kind == DECL_CONSTEXPR_VAR) {
			if (!evalConstexpr(decl->constexprVar.init, program, &value)) {
				return false;
			}
		} else if (decl->kind == DECL_CONSTEXPR_ARRAY) {
			if (!evalConstexpr(decl->constexprArray.size, program, &value)) {
				return false;
			}
			for (ExprList *element = decl->constexprArray.init; element != NULL; element = element->next) {
				if (!evalConstexpr(element->expr, program, &value)) {
					return false;
				}
			}
		}
	}
	return true;
}

static bool
_calleeWritesRef(Decl *callee, const char *param) {
	NameSet reads = { 0 };
	NameSet writes = { 0 };
	collectStmtAccesses(callee->func.body, NULL, &reads, &writes);
	bool written = nameSetContains(&writes, param);
	nameSetFree(&reads);
	nameSetFree(&writes);
	return written;
}

typedef struct {
	Program *program;
	RefSlice *slices;
	int count;
	int capacity;
} DrivenScope;

/* Collects the slices driven by a call's reference output (the callee writes the param). */
static void
_collectDrivenSlice(Expr *expr, void *context) {
	DrivenScope *scope = context;
	if (expr->kind != EXPR_CALL) {
		return;
	}
	Decl *callee = _findFunction(scope->program, expr->call.name);
	if (callee == NULL) {
		return;
	}
	ParamList *param = callee->func.params;
	for (ExprList *arg = expr->call.args; arg != NULL && param != NULL; arg = arg->next, param = param->next) {
		if (param->param->kind != PARAM_REF || !_calleeWritesRef(callee, param->param->name)) {
			continue;
		}
		RefSlice slice = _refSlice(arg->expr, scope->program);
		if (slice.base == NULL) {
			continue;
		}
		if (scope->count == scope->capacity) {
			scope->capacity = scope->capacity == 0 ? 4 : scope->capacity * 2;
			scope->slices = realloc(scope->slices, scope->capacity * sizeof(RefSlice));
		}
		scope->slices[scope->count++] = slice;
	}
}

/* In a combinational module a name assigned procedurally (always_comb) must not also be
 * driven by a call's reference output (a module instance), nor may two call outputs drive
 * overlapping storage; otherwise the signal has multiple drivers. Distinct constant array
 * elements (a[0] and a[1]) are disjoint and allowed. */
static bool
_checkSingleDriver(Program *program) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (decl->kind != DECL_FUNCTION || decl->func.sequential) {
			continue;
		}
		NameSet reads = { 0 };
		NameSet procedural = { 0 };
		collectStmtAccesses(decl->func.body, NULL, &reads, &procedural);
		walkStmts(decl->func.body, _addDeclWrite, &procedural);

		DrivenScope driven = { .program = program };
		walkStmtExprs(decl->func.body, _collectDrivenSlice, &driven);

		bool conflict = false;
		for (int i = 0; i < driven.count && !conflict; ++i) {
			if (nameSetContains(&procedural, driven.slices[i].base)) {
				conflict = true;
				break;
			}
			for (int j = i + 1; j < driven.count; ++j) {
				if (_slicesOverlap(&driven.slices[i], &driven.slices[j])) {
					conflict = true;
					break;
				}
			}
		}
		nameSetFree(&reads);
		nameSetFree(&procedural);
		free(driven.slices);
		if (conflict) {
			return false;
		}
	}
	return true;
}

/* Definite assignment: reading a local before it is assigned on every path infers a latch
 * (combinational) or yields X. The pass threads the set of definitely-assigned names
 * through the control flow; a read of a declared local not yet in that set is rejected. */
static void
_nameSetCopy(const NameSet *source, NameSet *target) {
	for (int i = 0; i < source->count; ++i) {
		nameSetAdd(target, source->names[i]);
	}
}

static void
_collectLocalName(Stmt *stmt, void *context) {
	if (stmt->kind == STMT_LOCAL_DECL) {
		nameSetAdd((NameSet *)context, stmt->localDecl.name);
	}
}

typedef struct {
	Program *program;
	const NameSet *locals;
	bool ok;
} DefAssignScope;

static void _daNode(Stmt *stmt, DefAssignScope *scope, NameSet *assigned);

/* Checks the reads of a leaf statement (or wrapped expression) against the assigned set,
 * then marks its writes assigned. */
static void
_daAccesses(Stmt *stmt, DefAssignScope *scope, NameSet *assigned) {
	NameSet reads = { 0 };
	NameSet writes = { 0 };
	collectStmtAccesses(stmt, scope->program, &reads, &writes);
	for (int i = 0; i < reads.count; ++i) {
		if (nameSetContains(scope->locals, reads.names[i]) && !nameSetContains(assigned, reads.names[i])) {
			scope->ok = false;
		}
	}
	for (int i = 0; i < writes.count; ++i) {
		if (!nameSetContains(assigned, writes.names[i])) {
			nameSetAdd(assigned, writes.names[i]);
		}
	}
	nameSetFree(&reads);
	nameSetFree(&writes);
}

static void
_daExpr(Expr *expr, DefAssignScope *scope, NameSet *assigned) {
	Stmt wrapped = { .kind = STMT_EXPR };
	wrapped.exprStmt = expr;
	_daAccesses(&wrapped, scope, assigned);
}

static void
_daNode(Stmt *stmt, DefAssignScope *scope, NameSet *assigned) {
	if (stmt == NULL) {
		return;
	}
	switch (stmt->kind) {
	case STMT_COMPOUND:
		for (StmtList *node = stmt->compound; node != NULL; node = node->next) {
			_daNode(node->stmt, scope, assigned);
		}
		break;
	case STMT_IF: {
		_daExpr(stmt->ifStmt.condition, scope, assigned);
		NameSet thenSet = { 0 };
		NameSet elseSet = { 0 };
		_nameSetCopy(assigned, &thenSet);
		_nameSetCopy(assigned, &elseSet);
		_daNode(stmt->ifStmt.thenBranch, scope, &thenSet);
		_daNode(stmt->ifStmt.elseBranch, scope, &elseSet);
		/* A name is assigned after the if only if both branches assign it. */
		if (stmt->ifStmt.elseBranch != NULL) {
			for (int i = 0; i < thenSet.count; ++i) {
				if (nameSetContains(&elseSet, thenSet.names[i]) && !nameSetContains(assigned, thenSet.names[i])) {
					nameSetAdd(assigned, thenSet.names[i]);
				}
			}
		}
		nameSetFree(&thenSet);
		nameSetFree(&elseSet);
		break;
	}
	case STMT_FOR: {
		/* The init runs once; the body/update may not run, so their assignments do not persist. */
		_daNode(stmt->forStmt.init, scope, assigned);
		_daExpr(stmt->forStmt.condition, scope, assigned);
		NameSet bodySet = { 0 };
		_nameSetCopy(assigned, &bodySet);
		_daNode(stmt->forStmt.body, scope, &bodySet);
		_daExpr(stmt->forStmt.update, scope, &bodySet);
		nameSetFree(&bodySet);
		break;
	}
	case STMT_WHILE: {
		_daExpr(stmt->whileStmt.condition, scope, assigned);
		NameSet bodySet = { 0 };
		_nameSetCopy(assigned, &bodySet);
		_daNode(stmt->whileStmt.body, scope, &bodySet);
		nameSetFree(&bodySet);
		break;
	}
	case STMT_PARALLEL: {
		/* Units are concurrent: none sees another's write, but all writes land afterwards. */
		for (StmtList *node = stmt->parallel; node != NULL; node = node->next) {
			NameSet unitSet = { 0 };
			_nameSetCopy(assigned, &unitSet);
			_daNode(node->stmt, scope, &unitSet);
			nameSetFree(&unitSet);
		}
		NameSet reads = { 0 };
		NameSet writes = { 0 };
		collectStmtAccesses(stmt, scope->program, &reads, &writes);
		for (int i = 0; i < writes.count; ++i) {
			if (!nameSetContains(assigned, writes.names[i])) {
				nameSetAdd(assigned, writes.names[i]);
			}
		}
		nameSetFree(&reads);
		nameSetFree(&writes);
		break;
	}
	default: /* STMT_EXPR, STMT_RETURN, STMT_LOCAL_DECL */
		_daAccesses(stmt, scope, assigned);
		if (stmt->kind == STMT_LOCAL_DECL && (stmt->localDecl.init != NULL || stmt->localDecl.initList != NULL)) {
			nameSetAdd(assigned, stmt->localDecl.name);
		}
		break;
	}
}

static bool
_checkDefiniteAssignment(Program *program) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (decl->kind != DECL_FUNCTION || decl->func.sequential) {
			continue;
		}
		NameSet locals = { 0 };
		walkStmts(decl->func.body, _collectLocalName, &locals);
		NameSet assigned = { 0 };
		for (ParamList *param = decl->func.params; param != NULL; param = param->next) {
			nameSetAdd(&assigned, param->param->name);
		}
		DefAssignScope scope = { .program = program, .locals = &locals, .ok = true };
		_daNode(decl->func.body, &scope, &assigned);
		nameSetFree(&locals);
		nameSetFree(&assigned);
		if (!scope.ok) {
			return false;
		}
	}
	return true;
}

/* A combinational function emits each return as an unguarded `out = expr;`, so an early
 * return does not short-circuit the statements after it (the last assignment wins). To
 * keep that sound, returns are only allowed in tail position: nothing may execute after
 * one. The if/else form (a return per branch, nothing after) is the supported idiom. */
static bool
_hasReturn(Stmt *stmt) {
	if (stmt == NULL) {
		return false;
	}
	switch (stmt->kind) {
	case STMT_RETURN:
		return true;
	case STMT_IF:
		return _hasReturn(stmt->ifStmt.thenBranch) || _hasReturn(stmt->ifStmt.elseBranch);
	case STMT_FOR:
		return _hasReturn(stmt->forStmt.body);
	case STMT_WHILE:
		return _hasReturn(stmt->whileStmt.body);
	case STMT_COMPOUND:
		for (StmtList *node = stmt->compound; node != NULL; node = node->next) {
			if (_hasReturn(node->stmt)) {
				return true;
			}
		}
		return false;
	case STMT_PARALLEL:
		for (StmtList *node = stmt->parallel; node != NULL; node = node->next) {
			if (_hasReturn(node->stmt)) {
				return true;
			}
		}
		return false;
	default:
		return false;
	}
}

static bool
_tailReturnsOk(Stmt *stmt) {
	if (stmt == NULL) {
		return true;
	}
	switch (stmt->kind) {
	case STMT_RETURN:
		return true;
	case STMT_IF:
		return _tailReturnsOk(stmt->ifStmt.thenBranch) && _tailReturnsOk(stmt->ifStmt.elseBranch);
	case STMT_COMPOUND:
		for (StmtList *node = stmt->compound; node != NULL; node = node->next) {
			if (node->next == NULL) {
				return _tailReturnsOk(node->stmt);
			}
			if (_hasReturn(node->stmt)) {
				return false;
			}
		}
		return true;
	case STMT_FOR:
		return !_hasReturn(stmt->forStmt.body);
	case STMT_WHILE:
		return !_hasReturn(stmt->whileStmt.body);
	case STMT_PARALLEL:
		return !_hasReturn(stmt);
	default:
		return true;
	}
}

static bool
_checkTailReturns(Program *program) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (decl->kind != DECL_FUNCTION || decl->func.sequential) {
			continue;
		}
		if (!_tailReturnsOk(decl->func.body)) {
			return false;
		}
	}
	return true;
}

/* A reference read and written in a combinational function becomes an inout port driven
 * by always_comb that reads its own output: a combinational loop (unsynthesizable). Such
 * a read-modify needs a register, i.e. a __sequential function. */
static bool
_checkCombinationalRefs(Program *program) {
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (decl->kind != DECL_FUNCTION || decl->func.sequential) {
			continue;
		}
		NameSet reads = { 0 };
		NameSet writes = { 0 };
		collectStmtAccesses(decl->func.body, program, &reads, &writes);
		bool violation = false;
		for (ParamList *param = decl->func.params; param != NULL; param = param->next) {
			if ((param->param->kind == PARAM_REF || param->param->kind == PARAM_ARRAY)
			    && nameSetContains(&reads, param->param->name) && nameSetContains(&writes, param->param->name)) {
				violation = true;
				break;
			}
		}
		nameSetFree(&reads);
		nameSetFree(&writes);
		if (violation) {
			return false;
		}
	}
	return true;
}

bool
executeSemanticAnalysis(CompilerState *compilerState) {
	logDebugging(_logger, "Analyzing semantics...");
	Program *program = compilerState->abstractSyntaxTree;
	if (program == NULL) {
		return true;
	}
	if (!_checkTypes(program)) {
		logError(_logger, "The program has name-resolution or type errors.");
		return false;
	}
	if (!_checkReturns(program)) {
		logError(_logger, "A non-void function must return a value on every path.");
		return false;
	}
	if (!_checkTailReturns(program)) {
		logError(_logger,
		         "A return must be in tail position (no statements after it); use if/else with a return per branch.");
		return false;
	}
	if (!_checkNoRecursion(program)) {
		logError(_logger, "Recursion is not allowed.");
		return false;
	}
	if (!_checkConstexprRefParams(program)) {
		logError(_logger, "A constexpr function cannot take reference parameters.");
		return false;
	}
	if (!_checkRuntimeCallsConstexpr(program)) {
		logError(_logger, "A constexpr function can only be called from a constexpr context.");
		return false;
	}
	if (!_checkConstexprCalls(program)) {
		logError(_logger, "A constexpr context can only call constexpr functions.");
		return false;
	}
	if (!_checkConstexprEvaluable(program)) {
		logError(_logger, "A constexpr declaration must evaluate to a compile-time constant.");
		return false;
	}
	if (!_checkLoopBounds(program)) {
		logError(_logger, "The bound of a for or while loop must be a compile-time constant.");
		return false;
	}
	if (!_checkParallelDependencies(program)) {
		logError(_logger, "A parallel block cannot write the same variable more than once.");
		return false;
	}
	if (!_checkAliasing(program)) {
		logError(_logger, "A unique reference cannot alias another argument in the same call.");
		return false;
	}
	if (!_checkSequential(program)) {
		return false;
	}
	if (!_checkSingleDriver(program)) {
		logError(_logger, "A signal cannot be driven by both an assignment and a call, or by two calls.");
		return false;
	}
	if (!_checkCombinationalRefs(program)) {
		logError(_logger,
		         "A read-write reference or array requires a __sequential function (it forms a combinational loop).");
		return false;
	}
	if (!_checkDefiniteAssignment(program)) {
		logError(_logger, "A local variable may be read before it is assigned on every path (infers a latch).");
		return false;
	}
	logDebugging(_logger, "Semantic analysis succeeded.");
	return true;
}
