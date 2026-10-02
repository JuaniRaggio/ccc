#pragma once

#include "frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "support/logging/Logger.h"
#include "support/type/CompilerState.h"
#include "support/type/ModuleDestructor.h"
#include <stdio.h>

ModuleDestructor initializeGeneratorModule(void);

void executeGenerator(CompilerState *compilerState);
