#pragma once

#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdbool.h>
#include <stdlib.h>

ModuleDestructor initializeAbstractSyntaxTreeModule();


typedef enum TypeKind {
    TYPE_INT8, TYPE_INT16, TYPE_INT32, TYPE_INT64,
    TYPE_UINT8, TYPE_UINT16, TYPE_UINT32, TYPE_UINT64,
    TYPE_BOOL, TYPE_VOID
} TypeKind;

typedef enum AliasQualifier {
    QUALIFIER_UNIQUE,
    QUALIFIER_ALIASED
} AliasQualifier;

typedef enum BinaryOp {
    BINOP_ADD, BINOP_SUB, BINOP_MUL,
    BINOP_EQ, BINOP_NEQ,
    BINOP_LT, BINOP_GT, BINOP_LE, BINOP_GE,
    BINOP_AND, BINOP_OR
} BinaryOp;

typedef enum UnaryOp {
    UNOP_NEG,
    UNOP_NOT
} UnaryOp;

typedef enum AssignOp {
    ASSIGN_PLAIN,
    ASSIGN_ADD,
    ASSIGN_SUB,
    ASSIGN_MUL
} AssignOp;

typedef enum IntrinsicKind {
    INTRINSIC_ABS,
    INTRINSIC_MIN,
    INTRINSIC_MAX
} IntrinsicKind;

typedef enum ExprKind {
    EXPR_INT_LITERAL,
    EXPR_BOOL_LITERAL,
    EXPR_IDENTIFIER,
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_ASSIGN,
    EXPR_ARRAY_ACCESS,
    EXPR_CALL,
    EXPR_INTRINSIC,
    EXPR_POST_INC,
    EXPR_POST_DEC
} ExprKind;

typedef enum StmtKind {
    STMT_EXPR,
    STMT_LOCAL_DECL,
    STMT_IF,
    STMT_FOR,
    STMT_WHILE,
    STMT_RETURN,
    STMT_COMPOUND,
    STMT_PARALLEL
} StmtKind;

typedef enum ParamKind {
    PARAM_VALUE,
    PARAM_REF,
    PARAM_ARRAY
} ParamKind;

typedef enum DeclKind {
    DECL_FUNCTION,
    DECL_CONSTEXPR_FUNC,
    DECL_CONSTEXPR_VAR,
    DECL_CONSTEXPR_ARRAY
} DeclKind;

typedef struct Expr     Expr;
typedef struct ExprList ExprList;
typedef struct Stmt     Stmt;
typedef struct StmtList StmtList;
typedef struct Param    Param;
typedef struct ParamList ParamList;
typedef struct Decl     Decl;
typedef struct DeclList DeclList;
typedef struct Program  Program;

};

enum FactorType {
	CONSTANT,
	EXPRESSION
};

struct Constant {
	int value;
};

struct Factor {
	union {
		Constant * constant;
		Expression * expression;
	};
	FactorType type;
};

struct Expression {
	union {
		Factor * factor;
		struct {
			Expression * leftExpression;
			Expression * rightExpression;
		};
	};
	ExpressionType type;
};

struct Program {
	Expression * expression;
};

/**
 * Node recursive super-duper-trambolik-destructors.
 */

void destroyConstant(Constant * constant);
void destroyExpression(Expression * expression);
void destroyFactor(Factor * factor);
void destroyProgram(Program * program);

#endif
