#!/bin/bash
BIN=.build/Flex-Bison-Compiler
run() {
  local name="$1"; local expect="$2"; local file="$3"
  LOGGING_LEVEL=CRITICAL $BIN < "$file" >/dev/null 2>&1
  local ec=$?; local got="accept"; [ $ec -ne 0 ] && got="reject"
  printf "%-40s expect=%-7s got=%-7s %s\n" "$name" "$expect" "$got" "$([ "$got" = "$expect" ] && echo OK || echo MISMATCH)"
}
D=/tmp/p8; mkdir -p $D
# clamp via if/else (documented, returns in both branches, nothing after) - should be correct
cat > $D/clamp.c <<'EOF'
int32_t clamp(int32_t v, int32_t lo, int32_t hi){ if(v<lo){return lo;} if(v>hi){return hi;} return v; }
EOF
echo "=== clamp (chained early returns) ==="
LOGGING_LEVEL=CRITICAL $BIN < $D/clamp.c 2>/dev/null | tee $D/clamp.sv
cat > $D/tb.sv <<'EOF'
module tb; logic signed [31:0] v,lo,hi,o;
 clamp d(.v(v),.lo(lo),.hi(hi),.out(o));
 initial begin lo=0;hi=10;
  v=-5;#1 $display("v=-5 lo=0 hi=10 -> %0d (expect 0)",o);
  v=5;#1  $display("v=5 -> %0d (expect 5)",o);
  v=20;#1 $display("v=20 -> %0d (expect 10)",o);
 end endmodule
EOF
iverilog -g2012 -o $D/sim $D/clamp.sv $D/tb.sv 2>&1 && vvp $D/sim 2>&1

# run the full existing accept/reject suite quickly to ensure no regressions
echo "=== existing suite ==="
pass=0; fail=0
for f in src/test/c/accept/*; do
  [ -f "$f" ] || continue
  LOGGING_LEVEL=CRITICAL $BIN < "$f" >/dev/null 2>&1 && pass=$((pass+1)) || { echo "ACCEPT-FAIL: $f"; fail=$((fail+1)); }
done
for f in src/test/c/reject/*; do
  [ -f "$f" ] || continue
  LOGGING_LEVEL=CRITICAL $BIN < "$f" >/dev/null 2>&1 && { echo "REJECT-FAIL(accepted): $f"; fail=$((fail+1)); } || pass=$((pass+1))
done
echo "suite: pass=$pass fail=$fail"
