#include "Generator.h"
#include <string.h>

static Logger *_logger = NULL;
static Program *_program = NULL;
static Expr **_callNodes = NULL;
static int _callCount = 0;

static Decl *
_findFunctionDecl(const char *name) {
	for (DeclList *node = _program->decls; node != NULL; node = node->next) {
		if (node->decl->kind == DECL_FUNCTION && strcmp(node->decl->func.name, name) == 0) {
			return node->decl;
		}
	}
	return NULL;
}

static int
_callIndex(Expr *call) {
	for (int i = 0; i < _callCount; ++i) {
		if (_callNodes[i] == call) {
			return i;
		}
	}
	return -1;
}

void
_shutdownGeneratorModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Generator...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor
initializeGeneratorModule(void) {
	_logger = createLogger("Generator");
	return _shutdownGeneratorModule;
}

static int
_bitWidth(TypeKind type) {
	switch (type) {
	case TYPE_INT8:
	case TYPE_UINT8:
		return 8;
	case TYPE_INT16:
	case TYPE_UINT16:
		return 16;
	case TYPE_INT64:
	case TYPE_UINT64:
		return 64;
	default:
		return 32;
	}
}

static bool
_isSigned(TypeKind type) {
	return type == TYPE_INT8 || type == TYPE_INT16 || type == TYPE_INT32 || type == TYPE_INT64;
}

static void
_emitType(TypeKind type) {
	if (type == TYPE_BOOL) {
		fprintf(stdout, "logic");
		return;
	}
	fprintf(stdout, "logic %s[%d:0]", _isSigned(type) ? "signed " : "", _bitWidth(type) - 1);
}

static void _emitExpr(Expr *expr);

static void
_emitIntrinsic(Expr *expr) {
	Expr *first = expr->intrinsic.args->expr;
	if (expr->intrinsic.kind == INTRINSIC_ABS) {
		fprintf(stdout, "(");
		_emitExpr(first);
		fprintf(stdout, " < 0 ? -");
		_emitExpr(first);
		fprintf(stdout, " : ");
		_emitExpr(first);
		fprintf(stdout, ")");
		return;
	}
	Expr *second = expr->intrinsic.args->next->expr;
	const char *op = expr->intrinsic.kind == INTRINSIC_MIN ? "<" : ">";
	fprintf(stdout, "(");
	_emitExpr(first);
	fprintf(stdout, " %s ", op);
	_emitExpr(second);
	fprintf(stdout, " ? ");
	_emitExpr(first);
	fprintf(stdout, " : ");
	_emitExpr(second);
	fprintf(stdout, ")");
}

static const char *
_binaryOp(BinaryOp op) {
	switch (op) {
	case BINOP_ADD:
		return "+";
	case BINOP_SUB:
		return "-";
	case BINOP_MUL:
		return "*";
	case BINOP_LSHIFT:
		return "<<";
	case BINOP_RSHIFT:
		return ">>";
	case BINOP_EQ:
		return "==";
	case BINOP_NEQ:
		return "!=";
	case BINOP_LT:
		return "<";
	case BINOP_GT:
		return ">";
	case BINOP_LE:
		return "<=";
	case BINOP_GE:
		return ">=";
	case BINOP_AND:
		return "&&";
	default:
		return "||";
	}
}

static void
_emitExpr(Expr *expr) {
	if (expr == NULL) {
		return;
	}
	switch (expr->kind) {
	case EXPR_INT_LITERAL:
		fprintf(stdout, "%d", expr->intValue);
		break;
	case EXPR_BOOL_LITERAL:
		fprintf(stdout, "1'b%d", expr->boolValue ? 1 : 0);
		break;
	case EXPR_IDENTIFIER:
		fprintf(stdout, "%s", expr->name);
		break;
	case EXPR_BINARY:
		fprintf(stdout, "(");
		_emitExpr(expr->binary.left);
		fprintf(stdout, " %s ", _binaryOp(expr->binary.op));
		_emitExpr(expr->binary.right);
		fprintf(stdout, ")");
		break;
	case EXPR_UNARY:
		fprintf(stdout, "%s", expr->unary.op == UNOP_NEG ? "-" : "!");
		_emitExpr(expr->unary.operand);
		break;
	case EXPR_ARRAY_ACCESS:
		_emitExpr(expr->arrayAccess.array);
		fprintf(stdout, "[");
		_emitExpr(expr->arrayAccess.index);
		fprintf(stdout, "]");
		break;
	case EXPR_CALL:
		fprintf(stdout, "__call%d", _callIndex(expr));
		break;
	case EXPR_INTRINSIC:
		_emitIntrinsic(expr);
		break;
	default:
		_emitExpr(expr->postOp);
		break;
	}
}

static void
_emitIndent(int indent) {
	for (int i = 0; i < indent; ++i) {
		fprintf(stdout, " ");
	}
}

static const char *
_compoundOp(AssignOp op) {
	switch (op) {
	case ASSIGN_ADD:
		return "+";
	case ASSIGN_SUB:
		return "-";
	case ASSIGN_MUL:
		return "*";
	default:
		return NULL;
	}
}

static void
_emitAssign(Expr *assign) {
	_emitExpr(assign->assign.target);
	fprintf(stdout, " = ");
	const char *op = _compoundOp(assign->assign.op);
	if (op != NULL) {
		_emitExpr(assign->assign.target);
		fprintf(stdout, " %s ", op);
	}
	_emitExpr(assign->assign.value);
}

static void
_emitStep(Expr *expr) {
	if (expr->kind == EXPR_POST_INC || expr->kind == EXPR_POST_DEC) {
		_emitExpr(expr->postOp);
		fprintf(stdout, " = ");
		_emitExpr(expr->postOp);
		fprintf(stdout, " %s 1", expr->kind == EXPR_POST_INC ? "+" : "-");
	} else if (expr->kind == EXPR_ASSIGN) {
		_emitAssign(expr);
	} else {
		_emitExpr(expr);
	}
}

static void _emitStmt(Stmt *stmt, int indent);

static void
_emitBranch(Stmt *stmt, int indent) {
	if (stmt != NULL && stmt->kind == STMT_COMPOUND) {
		_emitStmt(stmt, indent);
	} else {
		_emitStmt(stmt, indent + 2);
	}
}

static void
_emitStmt(Stmt *stmt, int indent) {
	if (stmt == NULL) {
		return;
	}
	switch (stmt->kind) {
	case STMT_EXPR:
		_emitIndent(indent);
		_emitStep(stmt->exprStmt);
		fprintf(stdout, ";\n");
		break;
	case STMT_LOCAL_DECL:
		if (stmt->localDecl.init != NULL) {
			_emitIndent(indent);
			fprintf(stdout, "%s = ", stmt->localDecl.name);
			_emitExpr(stmt->localDecl.init);
			fprintf(stdout, ";\n");
		}
		int index = 0;
		for (ExprList *initNode = stmt->localDecl.initList; initNode != NULL; initNode = initNode->next) {
			_emitIndent(indent);
			fprintf(stdout, "%s[%d] = ", stmt->localDecl.name, index++);
			_emitExpr(initNode->expr);
			fprintf(stdout, ";\n");
		}
		break;
	case STMT_IF:
		_emitIndent(indent);
		fprintf(stdout, "if (");
		_emitExpr(stmt->ifStmt.condition);
		fprintf(stdout, ")\n");
		_emitBranch(stmt->ifStmt.thenBranch, indent);
		if (stmt->ifStmt.elseBranch != NULL) {
			_emitIndent(indent);
			fprintf(stdout, "else\n");
			_emitBranch(stmt->ifStmt.elseBranch, indent);
		}
		break;
	case STMT_FOR:
		_emitIndent(indent);
		fprintf(stdout, "for (");
		if (stmt->forStmt.init != NULL && stmt->forStmt.init->kind == STMT_LOCAL_DECL) {
			fprintf(stdout, "%s = ", stmt->forStmt.init->localDecl.name);
			_emitExpr(stmt->forStmt.init->localDecl.init);
		} else if (stmt->forStmt.init != NULL) {
			_emitStep(stmt->forStmt.init->exprStmt);
		}
		fprintf(stdout, "; ");
		_emitExpr(stmt->forStmt.condition);
		fprintf(stdout, "; ");
		_emitStep(stmt->forStmt.update);
		fprintf(stdout, ")\n");
		_emitBranch(stmt->forStmt.body, indent);
		break;
	case STMT_WHILE:
		_emitIndent(indent);
		fprintf(stdout, "while (");
		_emitExpr(stmt->whileStmt.condition);
		fprintf(stdout, ")\n");
		_emitBranch(stmt->whileStmt.body, indent);
		break;
	case STMT_RETURN:
		if (stmt->returnExpr != NULL) {
			_emitIndent(indent);
			fprintf(stdout, "out = ");
			_emitExpr(stmt->returnExpr);
			fprintf(stdout, ";\n");
		}
		break;
	case STMT_COMPOUND:
		_emitIndent(indent);
		fprintf(stdout, "begin\n");
		for (StmtList *stmtNode = stmt->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
			_emitStmt(stmtNode->stmt, indent + 2);
		}
		_emitIndent(indent);
		fprintf(stdout, "end\n");
		break;
	case STMT_PARALLEL:
		for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
			_emitStmt(stmtNode->stmt, indent);
		}
		break;
	}
}

static void
_emitLocalDecls(Stmt *stmt) {
	if (stmt == NULL) {
		return;
	}
	switch (stmt->kind) {
	case STMT_LOCAL_DECL:
		fprintf(stdout, "  ");
		_emitType(stmt->localDecl.type);
		fprintf(stdout, " %s", stmt->localDecl.name);
		if (stmt->localDecl.arraySize != NULL) {
			fprintf(stdout, " [");
			_emitExpr(stmt->localDecl.arraySize);
			fprintf(stdout, "]");
		}
		fprintf(stdout, ";\n");
		break;
	case STMT_IF:
		_emitLocalDecls(stmt->ifStmt.thenBranch);
		_emitLocalDecls(stmt->ifStmt.elseBranch);
		break;
	case STMT_FOR:
		_emitLocalDecls(stmt->forStmt.init);
		_emitLocalDecls(stmt->forStmt.body);
		break;
	case STMT_WHILE:
		_emitLocalDecls(stmt->whileStmt.body);
		break;
	case STMT_COMPOUND:
		for (StmtList *stmtNode = stmt->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
			_emitLocalDecls(stmtNode->stmt);
		}
		break;
	case STMT_PARALLEL:
		for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
			_emitLocalDecls(stmtNode->stmt);
		}
		break;
	default:
		break;
	}
}

static bool
_bodyHasPlainStatement(Stmt *body) {
	for (StmtList *stmtNode = body->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
		if (stmtNode->stmt->kind != STMT_PARALLEL) {
			return true;
		}
	}
	return false;
}

static void
_emitPort(Param *param, bool last) {
	const char *direction = param->kind == PARAM_REF ? "output" : "input ";
	fprintf(stdout, "  %s ", direction);
	_emitType(param->type);
	fprintf(stdout, " %s", param->name);
	if (param->kind == PARAM_ARRAY) {
		fprintf(stdout, " [");
		_emitExpr(param->arraySize);
		fprintf(stdout, "]");
	}
	fprintf(stdout, "%s\n", last ? "" : ",");
}

typedef struct {
	Expr **items;
	int count;
	int capacity;
} CallList;

static void
_callListAdd(CallList *list, Expr *call) {
	if (list->count == list->capacity) {
		list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
		list->items = realloc(list->items, list->capacity * sizeof(Expr *));
	}
	list->items[list->count++] = call;
}

static void
_collectCallsExpr(Expr *expr, CallList *list) {
	if (expr == NULL) {
		return;
	}
	switch (expr->kind) {
	case EXPR_BINARY:
		_collectCallsExpr(expr->binary.left, list);
		_collectCallsExpr(expr->binary.right, list);
		break;
	case EXPR_UNARY:
		_collectCallsExpr(expr->unary.operand, list);
		break;
	case EXPR_ASSIGN:
		_collectCallsExpr(expr->assign.target, list);
		_collectCallsExpr(expr->assign.value, list);
		break;
	case EXPR_ARRAY_ACCESS:
		_collectCallsExpr(expr->arrayAccess.array, list);
		_collectCallsExpr(expr->arrayAccess.index, list);
		break;
	case EXPR_CALL:
		for (ExprList *argNode = expr->call.args; argNode != NULL; argNode = argNode->next) {
			_collectCallsExpr(argNode->expr, list);
		}
		_callListAdd(list, expr);
		break;
	case EXPR_INTRINSIC:
		for (ExprList *argNode = expr->intrinsic.args; argNode != NULL; argNode = argNode->next) {
			_collectCallsExpr(argNode->expr, list);
		}
		break;
	case EXPR_POST_INC:
	case EXPR_POST_DEC:
		_collectCallsExpr(expr->postOp, list);
		break;
	default:
		break;
	}
}

static void
_collectCallsStmt(Stmt *stmt, CallList *list) {
	if (stmt == NULL) {
		return;
	}
	switch (stmt->kind) {
	case STMT_EXPR:
		_collectCallsExpr(stmt->exprStmt, list);
		break;
	case STMT_LOCAL_DECL:
		_collectCallsExpr(stmt->localDecl.arraySize, list);
		_collectCallsExpr(stmt->localDecl.init, list);
		for (ExprList *initNode = stmt->localDecl.initList; initNode != NULL; initNode = initNode->next) {
			_collectCallsExpr(initNode->expr, list);
		}
		break;
	case STMT_IF:
		_collectCallsExpr(stmt->ifStmt.condition, list);
		_collectCallsStmt(stmt->ifStmt.thenBranch, list);
		_collectCallsStmt(stmt->ifStmt.elseBranch, list);
		break;
	case STMT_FOR:
		_collectCallsStmt(stmt->forStmt.init, list);
		_collectCallsExpr(stmt->forStmt.condition, list);
		_collectCallsExpr(stmt->forStmt.update, list);
		_collectCallsStmt(stmt->forStmt.body, list);
		break;
	case STMT_WHILE:
		_collectCallsExpr(stmt->whileStmt.condition, list);
		_collectCallsStmt(stmt->whileStmt.body, list);
		break;
	case STMT_RETURN:
		_collectCallsExpr(stmt->returnExpr, list);
		break;
	case STMT_COMPOUND:
		for (StmtList *stmtNode = stmt->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
			_collectCallsStmt(stmtNode->stmt, list);
		}
		break;
	case STMT_PARALLEL:
		for (StmtList *stmtNode = stmt->parallel; stmtNode != NULL; stmtNode = stmtNode->next) {
			_collectCallsStmt(stmtNode->stmt, list);
		}
		break;
	}
}

static void
_emitInstances(CallList *calls) {
	for (int i = 0; i < calls->count; ++i) {
		Decl *callee = _findFunctionDecl(calls->items[i]->call.name);
		TypeKind returnType = callee != NULL ? callee->func.returnType : TYPE_INT32;
		fprintf(stdout, "  ");
		_emitType(returnType);
		fprintf(stdout, " __call%d;\n", i);
	}
	for (int i = 0; i < calls->count; ++i) {
		Expr *call = calls->items[i];
		Decl *callee = _findFunctionDecl(call->call.name);
		fprintf(stdout, "  %s __u%d (", call->call.name, i);
		ParamList *param = callee != NULL ? callee->func.params : NULL;
		for (ExprList *argNode = call->call.args; argNode != NULL && param != NULL;
		     argNode = argNode->next, param = param->next) {
			fprintf(stdout, ".%s(", param->param->name);
			_emitExpr(argNode->expr);
			fprintf(stdout, "), ");
		}
		fprintf(stdout, ".out(__call%d));\n", i);
	}
}

static void
_emitModule(Decl *function, bool importPackage) {
	fprintf(stdout, "module %s", function->func.name);
	if (importPackage) {
		fprintf(stdout, " import ccc_pkg::*;");
	}
	fprintf(stdout, " (\n");

	bool hasReturn = function->func.returnType != TYPE_VOID;
	for (ParamList *paramNode = function->func.params; paramNode != NULL; paramNode = paramNode->next) {
		_emitPort(paramNode->param, !hasReturn && paramNode->next == NULL);
	}
	if (hasReturn) {
		fprintf(stdout, "  output ");
		_emitType(function->func.returnType);
		fprintf(stdout, " out\n");
	}
	fprintf(stdout, ");\n");

	_emitLocalDecls(function->func.body);

	CallList calls = { .items = NULL, .count = 0, .capacity = 0 };
	_collectCallsStmt(function->func.body, &calls);
	_callNodes = calls.items;
	_callCount = calls.count;
	_emitInstances(&calls);

	if (_bodyHasPlainStatement(function->func.body)) {
		fprintf(stdout, "  always_comb begin\n");
		for (StmtList *stmtNode = function->func.body->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
			if (stmtNode->stmt->kind != STMT_PARALLEL) {
				_emitStmt(stmtNode->stmt, 4);
			}
		}
		fprintf(stdout, "  end\n");
	}
	for (StmtList *stmtNode = function->func.body->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
		if (stmtNode->stmt->kind == STMT_PARALLEL) {
			for (StmtList *inner = stmtNode->stmt->parallel; inner != NULL; inner = inner->next) {
				fprintf(stdout, "  always_comb begin\n");
				_emitStmt(inner->stmt, 4);
				fprintf(stdout, "  end\n");
			}
		}
	}
	fprintf(stdout, "endmodule\n");

	free(calls.items);
	_callNodes = NULL;
	_callCount = 0;
}

static bool
_emitPackage(Program *program) {
	bool any = false;
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (node->decl->kind == DECL_CONSTEXPR_VAR) {
			any = true;
			break;
		}
	}
	if (!any) {
		return false;
	}
	fprintf(stdout, "package ccc_pkg;\n");
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (node->decl->kind == DECL_CONSTEXPR_VAR) {
			fprintf(stdout, "  localparam ");
			_emitType(node->decl->constexprVar.type);
			fprintf(stdout, " %s = ", node->decl->constexprVar.name);
			_emitExpr(node->decl->constexprVar.init);
			fprintf(stdout, ";\n");
		}
	}
	fprintf(stdout, "endpackage\n\n");
	return true;
}

void
executeGenerator(CompilerState *compilerState) {
	logDebugging(_logger, "Generating SystemVerilog...");
	Program *program = compilerState->abstractSyntaxtTree;
	if (program == NULL) {
		return;
	}
	_program = program;
	bool importPackage = _emitPackage(program);
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (node->decl->kind == DECL_FUNCTION) {
			_emitModule(node->decl, importPackage);
			fprintf(stdout, "\n");
		}
	}
	logDebugging(_logger, "Generation is done.");
}
