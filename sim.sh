#!/bin/bash
BIN=.build/Flex-Bison-Compiler
D=/tmp/sim; mkdir -p $D

cat > $D/buf.c <<'EOF'
void fill(uint32_t x, uint32_t buf[4]){ buf[0]=x; }
EOF
LOGGING_LEVEL=CRITICAL $BIN < $D/buf.c 2>/dev/null > $D/fill.sv
cat $D/fill.sv

cat > $D/tb.sv <<'EOF'
module tb;
  logic [31:0] x;
  logic [31:0] b [4];
  fill dut(.x(x), .\buf (b));
  initial begin
    x = 32'hDEAD;
    #1;
    $display("buf[0]=%h buf[1]=%h buf[2]=%h buf[3]=%h", b[0], b[1], b[2], b[3]);
    if (b[1] === 32'hxxxxxxxx) $display("RESULT: buf[1] is X (undriven latch) - BUG CONFIRMED");
    else $display("RESULT: buf[1] driven = %h", b[1]);
  end
endmodule
EOF
iverilog -g2012 -o $D/sim $D/fill.sv $D/tb.sv 2>&1 && vvp $D/sim 2>&1
