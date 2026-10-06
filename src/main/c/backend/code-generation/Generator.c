#include "Generator.h"

static Logger *_logger = NULL;

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

void
executeGenerator(CompilerState *compilerState) {
	logDebugging(_logger, "Generator stub: pending Stage III implementation.");
}
