#!/bin/bash
BIN=.build/Flex-Bison-Compiler
D=/tmp/p7; mkdir -p $D

cat > $D/er.c <<'EOF'
int32_t f(int32_t a){ if(a<=0){ return 7; } return 99; }
EOF
echo "=== early-return combinational SV ==="
LOGGING_LEVEL=CRITICAL $BIN < $D/er.c 2>/dev/null | tee $D/er.sv

cat > $D/tb.sv <<'EOF'
module tb;
  logic signed [31:0] a, o;
  f dut(.a(a), .out(o));
  initial begin
    a = -5; #1; $display("a=-5 -> out=%0d (expect 7)", o);
    a = 10; #1; $display("a=10 -> out=%0d (expect 99)", o);
  end
endmodule
EOF
iverilog -g2012 -o $D/sim $D/er.sv $D/tb.sv 2>&1 && vvp $D/sim 2>&1
