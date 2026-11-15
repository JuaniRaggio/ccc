#! /bin/bash

# Simulates the generated SystemVerilog with Icarus Verilog. For each
# src/test/sv/<name>_tb.sv, compiles src/test/c/accept/<name> to SystemVerilog and
# runs the module against its testbench. A passing testbench prints a line "PASS".
# Requires Icarus (included in the Docker image). Run after build.sh.

set -uo pipefail

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

GREEN='\033[0;32m'
RED='\033[0;31m'
OFF='\033[0m'
STATUS=0
SIM_TIMEOUT=30

if ! command -v iverilog >/dev/null; then
	echo "iverilog not found; skipping simulation."
	exit 0
fi

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

echo "Backend should generate simulable SystemVerilog..."
echo ""
for tb in src/test/sv/*_tb.sv; do
	[ -e "$tb" ] || continue
	name="$(basename "$tb" _tb.sv)"
	src="src/test/c/accept/$name"
	if [ ! -f "$src" ]; then
		echo -e "    $name, ${RED}no accept source${OFF}"
		STATUS=1
		continue
	fi

	# Codegen must succeed and produce a non-empty module.
	sv="$OUT/$name.sv"
	if ! LOGGING_LEVEL=CRITICAL ".build/Flex-Bison-Compiler" <"$src" >"$sv" 2>/dev/null || [ ! -s "$sv" ]; then
		echo -e "    $name, ${RED}codegen failed${OFF}"
		STATUS=1
		continue
	fi

	if ! iverilog -g2012 -o "$OUT/$name.vvp" "$sv" "$tb" >/dev/null 2>&1; then
		echo -e "    $name, ${RED}compile failed${OFF}"
		STATUS=1
		continue
	fi

	# Single run with an external wall-clock guard; the testbench prints "PASS" on
	# success and aborts (non-zero exit) on mismatch or its own timeout.
	log="$OUT/$name.log"
	timeout "$SIM_TIMEOUT" vvp -n "$OUT/$name.vvp" >"$log" 2>&1
	code="$?"
	if [ "$code" == "0" ] && grep -qx PASS "$log"; then
		echo -e "    $name, ${GREEN}simulation ok${OFF}"
	elif [ "$code" == "124" ]; then
		STATUS=1
		echo -e "    $name, ${RED}simulation timeout${OFF}"
		sed 's/^/        /' "$log"
	else
		STATUS=1
		echo -e "    $name, ${RED}simulation failed${OFF}"
		sed 's/^/        /' "$log"
	fi
done
echo ""
exit $STATUS
