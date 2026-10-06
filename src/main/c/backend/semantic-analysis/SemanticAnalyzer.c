#include "SemanticAnalyzer.h"
#include <string.h>

static Logger *_logger = NULL;

typedef void (*ExprVisitor)(Expr *expr, void *context);
typedef struct {
    Decl **funcs;
    int    count;
    bool  *adjRow;
} CallEdgeContext;

typedef struct {
    Program *program;
    bool     violation;
} ConstexprCallContext;

typedef struct {
    Program    *program;
    bool        inConstexprFunc;
    char      **params;
    int         nParams;
    const char *inductionVar;
    bool        violation;
} ForBoundContext;

bool executeSemanticAnalysis(CompilerState *compilerState) {
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
