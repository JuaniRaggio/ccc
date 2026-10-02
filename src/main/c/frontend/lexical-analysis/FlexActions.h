#pragma once

#include "support/configuration/Environment.h"
#include "support/language/String.h"
#include "support/logging/Logger.h"
#include "support/type/CompilationStatus.h"
#include "support/type/FlexContext.h"
#include "support/type/LexicalAnalyzer.h"
#include "support/type/ModuleDestructor.h"
#include "support/type/Token.h"
#include "support/type/TokenLabel.h"
#include "frontend/Frontend.h"
#include <string.h>

ModuleDestructor initializeFlexActionsModule(LexicalAnalyzer *lexicalAnalyzer);

CompilationStatus TokenLexemeAction(TokenLabel label);
CompilationStatus IntegerLexemeAction(void);
CompilationStatus IdentifierLexemeAction(void);
CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
CompilationStatus LeaveMultilineCommentLexemeAction(void);
CompilationStatus IgnoredLexemeAction(void);
CompilationStatus EOFLexemeAction(void);
CompilationStatus UnknownLexemeAction(void);
