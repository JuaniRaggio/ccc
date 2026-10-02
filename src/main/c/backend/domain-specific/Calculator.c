#include "Calculator.h"

static Logger *_logger = NULL;

static ComputationResult _invalidComputation(void);
static ComputationResult _invalidBinaryOperator(const int x, const int y);

void _shutdownCalculatorModule() {
    if (_logger != NULL) {
        logDebugging(_logger, "Destroying module: Calculator...");
        destroyLogger(_logger);
        _logger = NULL;
    }
}

ModuleDestructor initializeCalculatorModule(void) {
    _logger = createLogger("Calculator");
    return _shutdownCalculatorModule;
}

static ComputationResult _invalidComputation(void) {
    ComputationResult result = { .succeeded = false, .value = 0 };
    return result;
}

static ComputationResult _invalidBinaryOperator(const int x, const int y) {
    return _invalidComputation();
}

ComputationResult add(const int leftAddend, const int rightAddend) {
    ComputationResult result = { .succeeded = true, .value = leftAddend + rightAddend };
    return result;
}

ComputationResult divide(const int dividend, const int divisor) {
    const int sign = dividend < 0 ? -1 : +1;
    const bool divisionByZero = divisor == 0;
    if (divisionByZero) {
        logError(_logger, "The divisor cannot be zero (the computation was %d/%d).", dividend, divisor);
    }
    ComputationResult result = {
        .succeeded = !divisionByZero,
        .value = divisionByZero ? (sign * INT_MAX) : (dividend / divisor)
    };
    return result;
}

ComputationResult multiply(const int multiplicand, const int multiplier) {
    ComputationResult result = { .succeeded = true, .value = multiplicand * multiplier };
    return result;
}

ComputationResult subtract(const int minuend, const int subtrahend) {
    ComputationResult result = { .succeeded = true, .value = minuend - subtrahend };
    return result;
}

ComputationResult executeCalculator(CompilerState *compilerState) {
    logDebugging(_logger, "Calculator stub: pending Stage III implementation.");
    ComputationResult result = { .succeeded = true, .value = 0 };
    return result;
}
