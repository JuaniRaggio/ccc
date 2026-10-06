#pragma once

#include "support/logging/Logger.h"
#include "support/type/CompilationStatus.h"
#include "support/type/FlexContext.h"
#include "support/type/InputBuffer.h"
#include "support/type/LexicalAnalyzer.h"
#include "support/type/ModuleDestructor.h"
#include "support/type/Token.h"
#include "support/type/TokenLabel.h"
#include "frontend/lexical-analysis/FlexScanner.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

ModuleDestructor initializeFrontendModule(LexicalAnalyzer *lexicalAnalyzer);
InputBuffer *createInputBuffer(LexicalAnalyzer *lexicalAnalyzer, const char *path);
LexicalAnalyzer *createLexicalAnalyzer(void);
Token *createToken(LexicalAnalyzer *lexicalAnalyzer, TokenLabel label);
FlexContext currentLexicalAnalyzerContext(LexicalAnalyzer *lexicalAnalyzer);
void destroyInputBuffer(InputBuffer *inputBuffer);
void destroyLexicalAnalyzer(LexicalAnalyzer *lexicalAnalyzer);
void destroyToken(Token *token);
void enterLexicalAnalyzerContext(LexicalAnalyzer *lexicalAnalyzer, FlexContext flexContext);
CompilationStatus executeLexicalAnalysis(LexicalAnalyzer *lexicalAnalyzer);
CompilationStatus executeSyntacticAnalysis(void);
void leaveLexicalAnalyzerContext(LexicalAnalyzer *lexicalAnalyzer);
bool popInputBuffer(LexicalAnalyzer *lexicalAnalyzer);
void pushInputBuffer(InputBuffer *inputBuffer);
CompilationStatus pushToken(LexicalAnalyzer *lexicalAnalyzer, Token *token);
