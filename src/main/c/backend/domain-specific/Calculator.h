#pragma once

#include "frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "support/logging/Logger.h"
#include "support/type/CompilerState.h"
#include "support/type/ModuleDestructor.h"
#include <limits.h>
#include <stdbool.h>

ModuleDestructor initializeCalculatorModule(void);

typedef struct {
    bool succeeded;
    int  value;
} ComputationResult;

typedef ComputationResult (*BinaryOperator)(const int, const int);

ComputationResult add(const int leftAddend, const int rightAddend);
ComputationResult divide(const int dividend, const int divisor);
ComputationResult multiply(const int multiplicand, const int multiplier);
ComputationResult subtract(const int minuend, const int subtrahend);

ComputationResult executeCalculator(CompilerState *compilerState);
