#include "Generator.h"
#include "frontend/semantic-analysis/ConstexprEval.h"
#include "frontend/syntactic-analysis/AccessSets.h"
#include "frontend/syntactic-analysis/AstWalk.h"
#include "frontend/syntactic-analysis/SeqSchedule.h"
#include "frontend/syntactic-analysis/TypeInfo.h"
#include <inttypes.h>
#include <string.h>

static Logger *_logger = NULL;
static Program *_program = NULL;
static Expr **_callNodes = NULL;
static int _callCount = 0;
/* The function currently being emitted, used to resolve the type of an identifier so a
 * right-shift of a signed operand can be emitted as the arithmetic `>>>`. */
static Decl *_currentFunction = NULL;

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

static bool
_isReservedSV(const char *name) {
	static const char *keywords[]
	    = { "buf",       "wire",     "reg",       "and",       "or",         "not",        "nand",     "nor",
		    "xor",       "xnor",     "logic",     "bit",       "byte",       "time",       "real",     "event",
		    "table",     "input",    "output",    "inout",     "module",     "endmodule",  "begin",    "end",
		    "always",    "assign",   "initial",   "parameter", "localparam", "genvar",     "signed",   "unsigned",
		    "tri",       "supply0",  "supply1",   "posedge",   "negedge",    "edge",       "disable",  "force",
		    "release",   "fork",     "join",      "task",      "function",   "casex",      "casez",    "specify",
		    "primitive", "cell",     "config",    "wait",      "automatic",  "final",      "int",      "integer",
		    "longint",   "shortint", "shortreal", "realtime",  "string",     "type",       "typedef",  "const",
		    "static",    "class",    "interface", "package",   "endpackage", "program",    "struct",   "union",
		    "enum",      "var",      "ref",       "virtual",   "extends",    "super",      "this",     "null",
		    "new",       "local",    "protected", "rand",      "randc",      "constraint", "generate", "endgenerate",
		    "forever",   "repeat",   "foreach",   "do",        "break",      "continue",   "assert",   "assume",
		    "cover",     "bind",     "import",    "export",    "extern",     "wand",       "wor",      "chandle",
		    "modport",   "clocking", "property",  "sequence",  "case",       "endcase",    "default",  NULL };
	for (int i = 0; keywords[i] != NULL; ++i) {
		if (strcmp(keywords[i], name) == 0) {
			return true;
		}
	}
	return false;
}

static void
_emitIdent(const char *name) {
	if (_isReservedSV(name)) {
		fprintf(stdout, "\\%s ", name);
	} else {
		fprintf(stdout, "%s", name);
	}
}

static void
_emitType(TypeKind type) {
	if (type == TYPE_BOOL) {
		fprintf(stdout, "logic");
		return;
	}
	fprintf(stdout, "logic %s[%d:0]", typeSigned(type) ? "signed " : "", typeWidth(type) - 1);
}

static void _emitExpr(Expr *expr);

/* Looks up the type of a name in the current function (parameter or local) or a constexpr
 * global, used only to decide the signedness of a right-shift. Returns false if unknown. */
typedef struct {
	const char *name;
	TypeKind type;
	bool found;
} NameTypeQuery;

static void
_findLocalType(Stmt *stmt, void *context) {
	NameTypeQuery *query = context;
	if (!query->found && stmt->kind == STMT_LOCAL_DECL && strcmp(stmt->localDecl.name, query->name) == 0) {
		query->type = stmt->localDecl.type;
		query->found = true;
	}
}

static bool
_lookupNameType(const char *name, TypeKind *out) {
	if (_currentFunction != NULL) {
		for (ParamList *node = _currentFunction->func.params; node != NULL; node = node->next) {
			if (strcmp(node->param->name, name) == 0) {
				*out = node->param->type;
				return true;
			}
		}
		NameTypeQuery query = { .name = name, .found = false };
		walkStmts(_currentFunction->func.body, _findLocalType, &query);
		if (query.found) {
			*out = query.type;
			return true;
		}
	}
	for (DeclList *node = _program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (decl->kind == DECL_CONSTEXPR_VAR && strcmp(decl->constexprVar.name, name) == 0) {
			*out = decl->constexprVar.type;
			return true;
		}
		if (decl->kind == DECL_CONSTEXPR_ARRAY && strcmp(decl->constexprArray.name, name) == 0) {
			*out = decl->constexprArray.type;
			return true;
		}
	}
	return false;
}

/* Conservative signedness of an expression: true only when it is provably a signed integer.
 * Used solely to pick `>>>` over `>>`; an unknown expression defaults to unsigned (`>>`),
 * which is harmless because the semantic analyzer already forbids mixing signedness. */
static bool
_exprSigned(Expr *expr) {
	if (expr == NULL) {
		return false;
	}
	switch (expr->kind) {
	case EXPR_INT_LITERAL:
		return true; /* a bare literal is int32_t (signed). */
	case EXPR_IDENTIFIER: {
		TypeKind type;
		return _lookupNameType(expr->name, &type) && typeSigned(type);
	}
	case EXPR_BINARY:
		/* Shifts keep the left type; arithmetic shares the operands' signedness. */
		if (expr->binary.op == BINOP_LSHIFT || expr->binary.op == BINOP_RSHIFT) {
			return _exprSigned(expr->binary.left);
		}
		return _exprSigned(expr->binary.left) || _exprSigned(expr->binary.right);
	case EXPR_UNARY:
		return expr->unary.op == UNOP_NEG && _exprSigned(expr->unary.operand);
	case EXPR_CAST:
		return typeSigned(expr->cast.type);
	case EXPR_ARRAY_ACCESS:
		return _exprSigned(expr->arrayAccess.array);
	case EXPR_INTRINSIC:
		return expr->intrinsic.args != NULL && _exprSigned(expr->intrinsic.args->expr);
	case EXPR_CALL: {
		Decl *callee = _findFunctionDecl(expr->call.name);
		return callee != NULL && typeSigned(callee->func.returnType);
	}
	default:
		return false;
	}
}

static void
_emitIntrinsic(Expr *expr) {
	if (expr->intrinsic.args == NULL) {
		return;
	}
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
	if (expr->intrinsic.args->next == NULL) {
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
		fprintf(stdout, "%" PRId64, expr->intValue);
		break;
	case EXPR_BOOL_LITERAL:
		fprintf(stdout, "1'b%d", expr->boolValue ? 1 : 0);
		break;
	case EXPR_IDENTIFIER:
		_emitIdent(expr->name);
		break;
	case EXPR_BINARY: {
		/* SystemVerilog `>>` is always logical; a signed right-shift must use `>>>`. */
		const char *op = _binaryOp(expr->binary.op);
		if (expr->binary.op == BINOP_RSHIFT && _exprSigned(expr->binary.left)) {
			op = ">>>";
		}
		fprintf(stdout, "(");
		_emitExpr(expr->binary.left);
		fprintf(stdout, " %s ", op);
		_emitExpr(expr->binary.right);
		fprintf(stdout, ")");
		break;
	}
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
	case EXPR_CAST:
		if (expr->cast.castKind == CAST_REINTERPRET) {
			/* bit_cast: same width, reinterpret signedness only. */
			fprintf(stdout, typeSigned(expr->cast.type) ? "signed'(" : "unsigned'(");
			_emitExpr(expr->cast.operand);
			fprintf(stdout, ")");
		} else if (typeSigned(expr->cast.type)) {
			/* static_cast to signed: W'(x) truncates/zero-extends, signed'(...) sign-extends. */
			fprintf(stdout, "signed'(%d'(", typeWidth(expr->cast.type));
			_emitExpr(expr->cast.operand);
			fprintf(stdout, "))");
		} else {
			fprintf(stdout, "%d'(", typeWidth(expr->cast.type));
			_emitExpr(expr->cast.operand);
			fprintf(stdout, ")");
		}
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
		/* A bare call is realized by its module instantiation, not a procedural statement. */
		if (stmt->exprStmt != NULL && stmt->exprStmt->kind == EXPR_CALL) {
			break;
		}
		_emitIndent(indent);
		_emitStep(stmt->exprStmt);
		fprintf(stdout, ";\n");
		break;
	case STMT_LOCAL_DECL:
		if (stmt->localDecl.init != NULL) {
			_emitIndent(indent);
			_emitIdent(stmt->localDecl.name);
			fprintf(stdout, " = ");
			_emitExpr(stmt->localDecl.init);
			fprintf(stdout, ";\n");
		}
		int index = 0;
		for (ExprList *initNode = stmt->localDecl.initList; initNode != NULL; initNode = initNode->next) {
			_emitIndent(indent);
			_emitIdent(stmt->localDecl.name);
			fprintf(stdout, "[%d] = ", index++);
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
			_emitIdent(stmt->forStmt.init->localDecl.name);
			fprintf(stdout, " = ");
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

/* Every local becomes one module-level signal. Locals of the same name in sibling scopes
 * (two `for (uint32_t i ...)` loops) share it: the semantic analysis guarantees they have
 * the same type and size and never shadow each other, so their lifetimes do not overlap. */
static void
_emitLocalDecl(Stmt *stmt, void *context) {
	NameSet *emitted = context;
	if (stmt->kind != STMT_LOCAL_DECL || nameSetContains(emitted, stmt->localDecl.name)) {
		return;
	}
	nameSetAdd(emitted, stmt->localDecl.name);
	fprintf(stdout, "  ");
	_emitType(stmt->localDecl.type);
	fprintf(stdout, " ");
	_emitIdent(stmt->localDecl.name);
	if (stmt->localDecl.arraySize != NULL) {
		fprintf(stdout, " [");
		_emitExpr(stmt->localDecl.arraySize);
		fprintf(stdout, "]");
	}
	fprintf(stdout, ";\n");
}

static void
_emitLocalDecls(Stmt *body) {
	NameSet emitted = { 0 };
	walkStmts(body, _emitLocalDecl, &emitted);
	nameSetFree(&emitted);
}

/* A bare call (realized by its instantiation) and a parallel block are not procedural statements. */
static bool
_isProceduralStatement(Stmt *stmt) {
	if (stmt->kind == STMT_PARALLEL) {
		return false;
	}
	if (stmt->kind == STMT_EXPR && stmt->exprStmt != NULL && stmt->exprStmt->kind == EXPR_CALL) {
		return false;
	}
	return true;
}

static bool
_bodyHasPlainStatement(Stmt *body) {
	for (StmtList *stmtNode = body->compound; stmtNode != NULL; stmtNode = stmtNode->next) {
		if (_isProceduralStatement(stmtNode->stmt)) {
			return true;
		}
	}
	return false;
}

/* A reference read-only maps to an input, write-only to an output, both to inout. */
static const char *
_refDirection(Stmt *body, const char *name) {
	NameSet reads = { 0 };
	NameSet writes = { 0 };
	collectStmtAccesses(body, _program, &reads, &writes);
	bool read = nameSetContains(&reads, name);
	bool written = nameSetContains(&writes, name);
	nameSetFree(&reads);
	nameSetFree(&writes);
	if (read && written) {
		return "inout ";
	}
	return written ? "output" : "input ";
}

static void
_emitPort(Param *param, const char *direction, const char *suffix, bool last) {
	fprintf(stdout, "  %s ", direction);
	_emitType(param->type);
	fprintf(stdout, " ");
	_emitIdent(param->name);
	fprintf(stdout, "%s", suffix);
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
_collectCall(Expr *expr, void *context) {
	if (expr->kind == EXPR_CALL) {
		_callListAdd((CallList *)context, expr);
	}
}

static void
_emitInstances(CallList *calls) {
	for (int i = 0; i < calls->count; ++i) {
		Decl *callee = _findFunctionDecl(calls->items[i]->call.name);
		if (callee == NULL || callee->func.returnType == TYPE_VOID) {
			continue;
		}
		fprintf(stdout, "  ");
		_emitType(callee->func.returnType);
		fprintf(stdout, " __call%d;\n", i);
	}
	for (int i = 0; i < calls->count; ++i) {
		Expr *call = calls->items[i];
		Decl *callee = _findFunctionDecl(call->call.name);
		bool hasReturn = callee != NULL && callee->func.returnType != TYPE_VOID;
		fprintf(stdout, "  ");
		_emitIdent(call->call.name);
		fprintf(stdout, " __u%d (", i);
		ParamList *param = callee != NULL ? callee->func.params : NULL;
		bool first = true;
		for (ExprList *argNode = call->call.args; argNode != NULL && param != NULL;
		     argNode = argNode->next, param = param->next) {
			fprintf(stdout, first ? "." : ", .");
			_emitIdent(param->param->name);
			fprintf(stdout, "(");
			_emitExpr(argNode->expr);
			fprintf(stdout, ")");
			first = false;
		}
		if (hasReturn) {
			fprintf(stdout, first ? ".out(__call%d)" : ", .out(__call%d)", i);
		}
		fprintf(stdout, ");\n");
	}
}

/* --- Sequential (FSM) backend ---
 * A __sequential function becomes a clocked module with a start/done handshake. The body
 * is scheduled into numbered states (see SeqSchedule.h): each run of simple statements
 * takes one clock, and each loop takes a condition-check state plus its body (a `for` adds
 * init and update states). Register updates use nonblocking assignment, so a state reads
 * the values latched at the previous edge. */

static void _emitSeqStmt(Stmt *stmt, int indent);

/* Width of the state register, derived from the number of states the body schedules. */
static int _seqWidth;

static void
_emitStateLabel(int id) {
	fprintf(stdout, "%d'd%d", _seqWidth, id);
}

static void
_emitGoto(int id, int indent) {
	_emitIndent(indent);
	fprintf(stdout, "ccc_state <= ");
	_emitStateLabel(id);
	fprintf(stdout, ";\n");
}

static void
_emitStateOpen(int id) {
	fprintf(stdout, "        ");
	_emitStateLabel(id);
	fprintf(stdout, ": begin\n");
}

static void
_emitSeqAssign(Expr *assign, int indent) {
	_emitIndent(indent);
	_emitExpr(assign->assign.target);
	fprintf(stdout, " <= ");
	const char *op = _compoundOp(assign->assign.op);
	if (op != NULL) {
		_emitExpr(assign->assign.target);
		fprintf(stdout, " %s ", op);
	}
	_emitExpr(assign->assign.value);
	fprintf(stdout, ";\n");
}

static void
_emitSeqStep(Expr *expr, int indent) {
	if (expr == NULL) {
		return;
	}
	if (expr->kind == EXPR_POST_INC || expr->kind == EXPR_POST_DEC) {
		_emitIndent(indent);
		_emitExpr(expr->postOp);
		fprintf(stdout, " <= ");
		_emitExpr(expr->postOp);
		fprintf(stdout, " %s 1;\n", expr->kind == EXPR_POST_INC ? "+" : "-");
	} else if (expr->kind == EXPR_ASSIGN) {
		_emitSeqAssign(expr, indent);
	}
}

static void
_emitSeqBranch(Stmt *stmt, int indent) {
	if (stmt != NULL && stmt->kind == STMT_COMPOUND) {
		for (StmtList *node = stmt->compound; node != NULL; node = node->next) {
			_emitSeqStmt(node->stmt, indent);
		}
	} else {
		_emitSeqStmt(stmt, indent);
	}
}

static void
_emitSeqStmt(Stmt *stmt, int indent) {
	if (stmt == NULL) {
		return;
	}
	switch (stmt->kind) {
	case STMT_EXPR:
		_emitSeqStep(stmt->exprStmt, indent);
		break;
	case STMT_LOCAL_DECL:
		if (stmt->localDecl.init != NULL) {
			_emitIndent(indent);
			_emitIdent(stmt->localDecl.name);
			fprintf(stdout, " <= ");
			_emitExpr(stmt->localDecl.init);
			fprintf(stdout, ";\n");
		}
		int index = 0;
		for (ExprList *initNode = stmt->localDecl.initList; initNode != NULL; initNode = initNode->next) {
			_emitIndent(indent);
			_emitIdent(stmt->localDecl.name);
			fprintf(stdout, "[%d] <= ", index++);
			_emitExpr(initNode->expr);
			fprintf(stdout, ";\n");
		}
		break;
	case STMT_IF:
		_emitIndent(indent);
		fprintf(stdout, "if (");
		_emitExpr(stmt->ifStmt.condition);
		fprintf(stdout, ") begin\n");
		_emitSeqBranch(stmt->ifStmt.thenBranch, indent + 2);
		_emitIndent(indent);
		fprintf(stdout, "end\n");
		if (stmt->ifStmt.elseBranch != NULL) {
			_emitIndent(indent);
			fprintf(stdout, "else begin\n");
			_emitSeqBranch(stmt->ifStmt.elseBranch, indent + 2);
			_emitIndent(indent);
			fprintf(stdout, "end\n");
		}
		break;
	case STMT_COMPOUND:
		/* A bare block of simple statements runs in the enclosing cycle (the semantic
		 * analysis rejects loops and returns inside one). */
		for (StmtList *node = stmt->compound; node != NULL; node = node->next) {
			_emitSeqStmt(node->stmt, indent);
		}
		break;
	default:
		break;
	}
}

/* Emits a statement, mapping a return to the output register. */
static void
_emitSeqOrReturn(Stmt *stmt, int indent) {
	if (stmt->kind == STMT_RETURN) {
		if (stmt->returnExpr != NULL) {
			_emitIndent(indent);
			fprintf(stdout, "out <= ");
			_emitExpr(stmt->returnExpr);
			fprintf(stdout, ";\n");
		}
	} else {
		_emitSeqStmt(stmt, indent);
	}
}

/* State 0 = IDLE, 1 = DONE, 2+ allocated below. States reference each other by id;
 * forward ids are allocated before the referencing arm is emitted. */
static int _seqState;

static int
_seqAlloc(void) {
	return _seqState++;
}

static int _emitSeqRegion(Stmt *body, int exitId);

static int
_emitSeqLoop(Stmt *loop, int exitId) {
	bool isFor = loop->kind == STMT_FOR;
	Expr *cond = isFor ? loop->forStmt.condition : loop->whileStmt.condition;
	Expr *update = isFor ? loop->forStmt.update : NULL;
	Stmt *init = isFor ? loop->forStmt.init : NULL;

	int checkId = _seqAlloc();
	int updateId = update != NULL ? _seqAlloc() : -1;
	int bodyExit = updateId != -1 ? updateId : checkId;
	int bodyEntry = _emitSeqRegion(seqLoopBody(loop), bodyExit);

	_emitStateOpen(checkId);
	fprintf(stdout, "          if (");
	_emitExpr(cond);
	fprintf(stdout, ") ccc_state <= ");
	_emitStateLabel(bodyEntry);
	fprintf(stdout, ";\n          else ccc_state <= ");
	_emitStateLabel(exitId);
	fprintf(stdout, ";\n        end\n");

	if (updateId != -1) {
		_emitStateOpen(updateId);
		_emitSeqStep(update, 10);
		_emitGoto(checkId, 10);
		fprintf(stdout, "        end\n");
	}

	if (init != NULL) {
		int initId = _seqAlloc();
		_emitStateOpen(initId);
		_emitSeqOrReturn(init, 10);
		_emitGoto(checkId, 10);
		fprintf(stdout, "        end\n");
		return initId;
	}
	return checkId;
}

/* Emits a region's segments right to left, so each one transitions to the next one's
 * entry; returns the region's entry state. */
static int
_emitSeqRegion(Stmt *body, int exitId) {
	SeqRegion region = seqRegionOf(body);
	int nextEntry = exitId;
	for (int i = region.count - 1; i >= 0; --i) {
		SeqSegment *segment = &region.segments[i];
		if (segment->isLoop) {
			nextEntry = _emitSeqLoop(segment->stmts[0], nextEntry);
			continue;
		}
		int stateId = _seqAlloc();
		_emitStateOpen(stateId);
		for (int k = 0; k < segment->count; ++k) {
			_emitSeqOrReturn(segment->stmts[k], 10);
		}
		_emitGoto(nextEntry, 10);
		fprintf(stdout, "        end\n");
		nextEntry = stateId;
	}
	seqRegionFree(&region);
	return nextEntry;
}

static bool
_isReadWriteRef(Decl *function, Param *param) {
	return (param->kind == PARAM_REF || param->kind == PARAM_ARRAY)
	       && strcmp(_refDirection(function->func.body, param->name), "inout ") == 0;
}

/* A reference read and written by a __sequential function is split into an input port
 * `<name>_in` (the caller's value) and an output register `<name>`, loaded on start. A
 * module cannot drive an inout from always_ff. */
static void
_emitRefLoads(Decl *function) {
	for (ParamList *node = function->func.params; node != NULL; node = node->next) {
		Param *param = node->param;
		if (!_isReadWriteRef(function, param)) {
			continue;
		}
		if (param->kind == PARAM_ARRAY) {
			fprintf(stdout, "            for (int ccc_k = 0; ccc_k < ");
			_emitExpr(param->arraySize);
			fprintf(stdout, "; ccc_k++) %s[ccc_k] <= %s_in[ccc_k];\n", param->name, param->name);
		} else {
			fprintf(stdout, "            %s <= %s_in;\n", param->name, param->name);
		}
	}
}

static int
_bitsFor(int values) {
	int bits = 1;
	while ((1 << bits) < values) {
		++bits;
	}
	return bits;
}

static void
_emitSequentialModule(Decl *function, bool importPackage) {
	_currentFunction = function;
	fprintf(stdout, "module ");
	_emitIdent(function->func.name);
	if (importPackage) {
		fprintf(stdout, " import ccc_pkg::*;");
	}
	fprintf(stdout, " (\n");
	fprintf(stdout, "  input logic clk,\n");
	fprintf(stdout, "  input logic rst,\n");
	fprintf(stdout, "  input logic start,\n");
	fprintf(stdout, "  output logic done,\n");

	bool hasReturn = function->func.returnType != TYPE_VOID;
	for (ParamList *paramNode = function->func.params; paramNode != NULL; paramNode = paramNode->next) {
		Param *param = paramNode->param;
		bool last = !hasReturn && paramNode->next == NULL;
		if (_isReadWriteRef(function, param)) {
			_emitPort(param, "input ", "_in", false);
			_emitPort(param, "output", "", last);
			continue;
		}
		const char *direction = (param->kind == PARAM_REF || param->kind == PARAM_ARRAY)
		                            ? _refDirection(function->func.body, param->name)
		                            : "input ";
		_emitPort(param, direction, "", last);
	}
	if (hasReturn) {
		fprintf(stdout, "  output ");
		_emitType(function->func.returnType);
		fprintf(stdout, " out\n");
	}
	fprintf(stdout, ");\n");

	_seqWidth = _bitsFor(2 + seqCountStates(function->func.body));
	fprintf(stdout, "  logic [%d:0] ccc_state;\n", _seqWidth - 1);
	_emitLocalDecls(function->func.body);

	fprintf(stdout, "  always_ff @(posedge clk) begin\n");
	fprintf(stdout, "    if (rst) begin\n");
	_emitGoto(0, 6);
	fprintf(stdout, "      done <= 1'b0;\n");
	fprintf(stdout, "    end else begin\n");
	fprintf(stdout, "      case (ccc_state)\n");

	_seqState = 2;
	int entry = _emitSeqRegion(function->func.body, 1);

	_emitStateOpen(0);
	fprintf(stdout, "          done <= 1'b0;\n");
	fprintf(stdout, "          if (start) begin\n");
	_emitRefLoads(function);
	_emitGoto(entry, 12);
	fprintf(stdout, "          end\n");
	fprintf(stdout, "        end\n");
	_emitStateOpen(1);
	fprintf(stdout, "          done <= 1'b1;\n");
	_emitGoto(0, 10);
	fprintf(stdout, "        end\n");
	fprintf(stdout, "        default: ");
	_emitGoto(0, 0);
	fprintf(stdout, "      endcase\n");
	fprintf(stdout, "    end\n");
	fprintf(stdout, "  end\n");
	fprintf(stdout, "endmodule\n");
}

static void
_emitModule(Decl *function, bool importPackage) {
	_currentFunction = function;
	fprintf(stdout, "module ");
	_emitIdent(function->func.name);
	if (importPackage) {
		fprintf(stdout, " import ccc_pkg::*;");
	}
	fprintf(stdout, " (\n");

	bool hasReturn = function->func.returnType != TYPE_VOID;
	for (ParamList *paramNode = function->func.params; paramNode != NULL; paramNode = paramNode->next) {
		const char *direction = (paramNode->param->kind == PARAM_REF || paramNode->param->kind == PARAM_ARRAY)
		                            ? _refDirection(function->func.body, paramNode->param->name)
		                            : "input ";
		_emitPort(paramNode->param, direction, "", !hasReturn && paramNode->next == NULL);
	}
	if (hasReturn) {
		fprintf(stdout, "  output ");
		_emitType(function->func.returnType);
		fprintf(stdout, " out\n");
	}
	fprintf(stdout, ");\n");

	_emitLocalDecls(function->func.body);

	CallList calls = { .items = NULL, .count = 0, .capacity = 0 };
	walkStmtExprs(function->func.body, _collectCall, &calls);
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

static void
_emitConstexprValue(Expr *expr, TypeKind type) {
	int64_t value;
	if (evalConstexpr(expr, _program, &value)) {
		fprintf(stdout, "%" PRId64, maskToType(value, type));
	} else {
		_emitExpr(expr);
	}
}

static void
_emitConstexprSize(Expr *expr) {
	int64_t value;
	if (evalConstexpr(expr, _program, &value)) {
		fprintf(stdout, "%" PRId64, value);
	} else {
		_emitExpr(expr);
	}
}

static bool
_emitPackage(Program *program) {
	bool any = false;
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (node->decl->kind == DECL_CONSTEXPR_VAR || node->decl->kind == DECL_CONSTEXPR_ARRAY) {
			any = true;
			break;
		}
	}
	if (!any) {
		return false;
	}
	fprintf(stdout, "package ccc_pkg;\n");
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		Decl *decl = node->decl;
		if (decl->kind == DECL_CONSTEXPR_VAR) {
			fprintf(stdout, "  localparam ");
			_emitType(decl->constexprVar.type);
			fprintf(stdout, " ");
			_emitIdent(decl->constexprVar.name);
			fprintf(stdout, " = ");
			_emitConstexprValue(decl->constexprVar.init, decl->constexprVar.type);
			fprintf(stdout, ";\n");
		} else if (decl->kind == DECL_CONSTEXPR_ARRAY) {
			fprintf(stdout, "  localparam ");
			_emitType(decl->constexprArray.type);
			fprintf(stdout, " ");
			_emitIdent(decl->constexprArray.name);
			fprintf(stdout, " [");
			_emitConstexprSize(decl->constexprArray.size);
			fprintf(stdout, "] = '{");
			for (ExprList *element = decl->constexprArray.init; element != NULL; element = element->next) {
				_emitConstexprValue(element->expr, decl->constexprArray.type);
				if (element->next != NULL) {
					fprintf(stdout, ", ");
				}
			}
			fprintf(stdout, "};\n");
		}
	}
	fprintf(stdout, "endpackage\n\n");
	return true;
}

void
executeGenerator(CompilerState *compilerState) {
	logDebugging(_logger, "Generating SystemVerilog...");
	Program *program = compilerState->abstractSyntaxTree;
	if (program == NULL) {
		return;
	}
	_program = program;
	bool importPackage = _emitPackage(program);
	for (DeclList *node = program->decls; node != NULL; node = node->next) {
		if (node->decl->kind == DECL_FUNCTION) {
			if (node->decl->func.sequential) {
				_emitSequentialModule(node->decl, importPackage);
			} else {
				_emitModule(node->decl, importPackage);
			}
			fprintf(stdout, "\n");
		}
	}
	logDebugging(_logger, "Generation is done.");
}
