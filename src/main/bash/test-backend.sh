#! /bin/bash

# Lints the SystemVerilog generated for every accepted program with Verilator.
# Requires Verilator (included in the Docker image). Run after build.sh.

set -uo pipefail

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

GREEN='\033[0;32m'
RED='\033[0;31m'
OFF='\033[0m'
STATUS=0

if ! command -v verilator >/dev/null; then
	echo "verilator not found; skipping backend lint."
	exit 0
fi

OUT="$(mktemp -d)"
echo "Backend should generate lint-clean SystemVerilog..."
echo ""
for test in $(ls src/test/c/accept/); do
	sv="$OUT/$test.sv"
	LOGGING_LEVEL=CRITICAL ".build/Flex-Bison-Compiler" <"src/test/c/accept/$test" >"$sv" 2>/dev/null
	# -Wno-MULTITOP: a file of independent modules has no single top (expected for a module library).
	if verilator --lint-only -Wno-MULTITOP "$sv" >/dev/null 2>&1; then
		echo -e "    $test, ${GREEN}lint ok${OFF}"
	else
		STATUS=1
		echo -e "    $test, ${RED}lint failed${OFF}"
	fi
done
echo ""
rm -rf "$OUT"
exit $STATUS
