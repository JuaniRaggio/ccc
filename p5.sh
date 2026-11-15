#!/bin/bash
BIN=.build/Flex-Bison-Compiler
run() {
  local name="$1"; local expect="$2"; local file="$3"
  LOGGING_LEVEL=CRITICAL $BIN < "$file" >/tmp/out.$$ 2>&1
  local ec=$?; local got="accept"; [ $ec -ne 0 ] && got="reject"
  local mark="OK"; [ "$got" != "$expect" ] && mark="**CHECK**"
  printf "%-54s expect=%-7s got=%-7s %s\n" "$name" "$expect" "$got" "$mark"
}
D=/tmp/p5; mkdir -p $D

# F9 still works: write-only ref via nested call -> output port, accept
cat > $D/a <<'EOF'
void setit(int32_t &o){ o = 5; }
void outer(int32_t &dst){ setit(dst); }
EOF
run "F9 write-only ref via call (still output)" accept $D/a
echo "--- outer SV ---"
LOGGING_LEVEL=CRITICAL $BIN < $D/a 2>/dev/null | grep -A3 "module outer"

# F13: the combinational check only looks at PARAM_REF directly read+written in the SAME function body.
# Does it see read+write when the write is via a call and read is direct? 
cat > $D/b <<'EOF'
void store(int32_t &o, int32_t v){ o = v; }
void f(int32_t &acc, int32_t x){ int32_t t; t = acc + x; store(acc, t); }
EOF
run "F13 ref read direct + write via call (hole?)" reject $D/b
echo "--- b SV (if accepted, check inout + comb loop) ---"
LOGGING_LEVEL=CRITICAL $BIN < $D/b 2>/dev/null | grep -E "module f|input|output|inout"

# F13 the simplest: read+write via TWO separate calls
cat > $D/c <<'EOF'
int32_t get(int32_t &a){ return a; }
void put(int32_t &a, int32_t v){ a = v; }
void f(int32_t &r){ int32_t t; t = get(r); put(r, t); }
EOF
run "F13 ref read-via-call + write-via-call (hole?)" reject $D/c
echo "--- c SV ---"
LOGGING_LEVEL=CRITICAL $BIN < $D/c 2>/dev/null | grep -E "module f|input|output|inout"

# F11 accept generates correct SV
cat > $D/d <<'EOF'
uint32_t f(uint32_t b){ uint32_t r; r = 1 + b; return r; }
EOF
echo "--- F11 1+b SV ---"
LOGGING_LEVEL=CRITICAL $BIN < $D/d 2>/dev/null
LOGGING_LEVEL=CRITICAL $BIN < $D/d 2>/dev/null > $D/d.sv
verilator --lint-only -Wall -Wno-DECLFILENAME $D/d.sv 2>&1 | grep -iE "warn|error" | head
