#include "Generator.h"

#include "support/language/String.h"
#include <stdarg.h>

static const char _indentationCharacter = ' ';
static const char _indentationSize = 4;
static Logger *_logger = NULL;

static char *_indentation(const unsigned int level);
static void _output(const unsigned int indentationLevel, const char *const format, ...);
static void _generatePrologue(void);
static void _generateEpilogue(const int value);

void _shutdownGeneratorModule() {
    if (_logger != NULL) {
        logDebugging(_logger, "Destroying module: Generator...");
        destroyLogger(_logger);
        _logger = NULL;
    }
}

ModuleDestructor initializeGeneratorModule(void) {
    _logger = createLogger("Generator");
    return _shutdownGeneratorModule;
}

static char *_indentation(const unsigned int level) {
    return indentation(_indentationCharacter, level, _indentationSize);
}

static void _output(const unsigned int indentationLevel, const char *const format, ...) {
    va_list arguments;
    va_start(arguments, format);
    char *ind = _indentation(indentationLevel);
    char *effectiveFormat = concatenate(2, ind, format);
    vfprintf(stdout, effectiveFormat, arguments);
    fflush(stdout);
    free(effectiveFormat);
    free(ind);
    va_end(arguments);
}

static void _generatePrologue(void) {
    _output(0, "%s",
        "\\documentclass{standalone}\n\n"
        "\\usepackage[utf8]{inputenc}\n"
        "\\usepackage[T1]{fontenc}\n"
        "\\usepackage{amsmath}\n"
        "\\usepackage{forest}\n"
        "\\usepackage{microtype}\n\n"
        "\\begin{document}\n"
        "    \\centering\n"
        "    \\begin{forest}\n"
        "        [ \\text{$=$}, circle, draw, purple\n"
    );
}

static void _generateEpilogue(const int value) {
    _output(0, "%s%d%s",
        "            [ $", value, "$, circle, draw, blue ]\n"
        "        ]\n"
        "    \\end{forest}\n"
        "\\end{document}\n\n"
    );
}

void executeGenerator(CompilerState *compilerState) {
    logDebugging(_logger, "Generator stub: pending Stage III implementation.");
}
