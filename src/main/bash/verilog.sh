#! /bin/bash

# Verifica (Verilator) y simula (Icarus Verilog) módulos de SystemVerilog.
# Uso:
#   src/main/bash/verilog.sh                  todos los módulos de doc/examples
#   src/main/bash/verilog.sh <modulo.sv>      un único módulo
# Si junto al módulo existe un testbench "<modulo>_tb.sv", también se simula.

set -uo pipefail

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

GREEN='\033[0;32m'
RED='\033[0;31m'
OFF='\033[0m'

if [ "$#" -gt 0 ]; then
	MODULES=("$@")
else
	MODULES=($(ls doc/examples/*.sv | grep -v '_tb\.sv$'))
fi

BUILD_PATH=".build/verilog"
mkdir --parents "$BUILD_PATH"
STATUS=0

for module in "${MODULES[@]}"; do
	name="$(basename "$module" .sv)"
	testbench="$(dirname "$module")/${name}_tb.sv"
	echo "== $name"
	if ! verilator --lint-only "$module" >/dev/null; then
		echo -e "    ${RED}lint failed${OFF}"
		STATUS=1
		continue
	fi
	echo -e "    ${GREEN}lint ok${OFF}"
	if [ -f "$testbench" ]; then
		if iverilog -g2012 -o "$BUILD_PATH/$name" "$module" "$testbench" && vvp -n "$BUILD_PATH/$name" | sed 's/^/    /' && [ "${PIPESTATUS[0]}" == "0" ]; then
			echo -e "    ${GREEN}simulation ok${OFF}"
		else
			echo -e "    ${RED}simulation failed${OFF}"
			STATUS=1
		fi
	fi
done

exit $STATUS
