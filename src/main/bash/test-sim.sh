#! /bin/bash

# Simulates the generated SystemVerilog with Icarus Verilog. For each
# src/test/sv/<name>_tb.sv, compiles src/test/c/accept/<name> to SystemVerilog and
# runs the module against its testbench. A passing testbench prints "PASS".
# Requires Icarus (included in the Docker image). Run after build.sh.

set -uo pipefail

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

GREEN='\033[0;32m'
RED='\033[0;31m'
OFF='\033[0m'
STATUS=0

if ! command -v iverilog >/dev/null; then
	echo "iverilog not found; skipping simulation."
	exit 0
fi

OUT="$(mktemp -d)"
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
	sv="$OUT/$name.sv"
	LOGGING_LEVEL=CRITICAL ".build/Flex-Bison-Compiler" <"$src" >"$sv" 2>/dev/null
	if iverilog -g2012 -o "$OUT/$name.vvp" "$sv" "$tb" >/dev/null 2>&1 && vvp -n "$OUT/$name.vvp" 2>/dev/null | grep -q PASS; then
		echo -e "    $name, ${GREEN}simulation ok${OFF}"
	else
		STATUS=1
		echo -e "    $name, ${RED}simulation failed${OFF}"
		vvp -n "$OUT/$name.vvp" 2>&1 | sed 's/^/        /'
	fi
done
echo ""
rm -rf "$OUT"
exit $STATUS
