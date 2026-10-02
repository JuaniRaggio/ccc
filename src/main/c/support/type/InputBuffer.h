#pragma once

#include "support/logging/Logger.h"
#include "LexicalAnalyzer.h"
#include <stdio.h>

/**
 * A lexical-analyzer input buffer.
 */
typedef struct {
	FILE * file;
	LexicalAnalyzer * lexicalAnalyzer;
	unsigned int bufferSizeInBytes;
	void * buffer;
} InputBuffer;

