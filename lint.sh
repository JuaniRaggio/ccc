#!/bin/bash
BIN=.build/Flex-Bison-Compiler
D=/tmp/lint; mkdir -p $D

# Calibration: hand-written SV with a known latch (scalar partial assign in always_comb)
cat > $D/latch.sv <<'EOF'
module latch_test (input logic c, output logic [31:0] o);
  always_comb begin
    if (c) o = 1;
  end
endmodule
EOF
echo "=== CALIBRATION: known scalar latch ==="
verilator --lint-only -Wall $D/latch.sv; echo "exit=$?"

# hand-written partial array latch
cat > $D/alatch.sv <<'EOF'
module arr_latch (input logic [31:0] x, output logic [31:0] b [4]);
  always_comb begin
    b[0] = x;
  end
endmodule
EOF
echo "=== CALIBRATION: partial array in always_comb ==="
verilator --lint-only -Wall $D/alatch.sv; echo "exit=$?"

# local array read unwritten element
cat > $D/arr <<'EOF'
int32_t f(){ int32_t a[4]; a[0]=1; return a[1]; }
EOF
LOGGING_LEVEL=CRITICAL $BIN < $D/arr 2>/dev/null > $D/arr.sv
echo "=== generated local array read-unwritten ==="
verilator --lint-only -Wall $D/arr.sv; echo "exit=$?"
