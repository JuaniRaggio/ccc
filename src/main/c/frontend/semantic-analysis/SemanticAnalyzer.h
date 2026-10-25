#pragma once

#include "frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "support/logging/Logger.h"
#include "support/type/CompilerState.h"
#include "support/type/ModuleDestructor.h"
#include <stdbool.h>

ModuleDestructor initializeSemanticAnalysisModule(void);

bool executeSemanticAnalysis(CompilerState *compilerState);
