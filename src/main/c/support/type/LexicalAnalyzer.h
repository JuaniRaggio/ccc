#pragma once

#include "support/logging/Logger.h"

/**
 * A lexical-analyzer and its internal state.
 */
typedef struct {
	Logger *logger;
	void *location;
	void *parser;
	void *scanner;
} LexicalAnalyzer;
