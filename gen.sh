#!/bin/bash
BIN=.build/Flex-Bison-Compiler
gen() {
  echo "===== $1 ====="
  LOGGING_LEVEL=CRITICAL $BIN < "$2" 2>/dev/null
  echo "----- lint -----"
  LOGGING_LEVEL=CRITICAL $BIN < "$2" 2>/dev/null > /tmp/mod.sv
  verilator --lint-only -Wall -Wno-MULTITOP -Wno-DECLFILENAME /tmp/mod.sv 2>&1 | head -30
  echo "=== END ==="
}
D=/tmp/gen; mkdir -p $D

cat > $D/arr <<'EOF'
int32_t f(){ int32_t a[4]; a[0]=1; return a[1]; }
EOF
gen "local array write a[0] read a[1]" $D/arr

cat > $D/buf <<'EOF'
void fill(uint32_t x, uint32_t buf[4]){ buf[0]=x; }
EOF
gen "partial output array (buf[0] only)" $D/buf

cat > $D/buffull <<'EOF'
void fill(uint32_t x, uint32_t buf[4]){ buf[0]=x; buf[1]=x; buf[2]=x; buf[3]=x; }
EOF
gen "full output array" $D/buffull
