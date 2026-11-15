#!/bin/bash
BIN=.build/Flex-Bison-Compiler
run() {
  local name="$1"; local expect="$2"; local file="$3"
  LOGGING_LEVEL=CRITICAL $BIN < "$file" >/tmp/out.$$ 2>&1
  local ec=$?; local got="accept"; [ $ec -ne 0 ] && got="reject"
  local mark="OK"; [ "$got" != "$expect" ] && mark="**CHECK**"
  printf "%-54s expect=%-7s got=%-7s %s\n" "$name" "$expect" "$got" "$mark"
}
D=/tmp/p6; mkdir -p $D

# Prove the else-returns program is semantically valid (latch-free) by writing it the
# way the frontend accepts (guard-return first) and linting.
cat > $D/ok.c <<'EOF'
int32_t f(int32_t a){ int32_t t; if(a<=0){ return 0; } t=1; return t; }
EOF
echo "=== guard-return (accepted) SV lint ==="
LOGGING_LEVEL=CRITICAL $BIN < $D/ok.c 2>/dev/null > $D/ok.sv
cat $D/ok.sv
verilator --lint-only -Wall -Wno-DECLFILENAME $D/ok.sv 2>&1 | grep -iE "latch|warn|error" | head

# More over-rejection variants of terminal-return merge
# then returns, else assigns: if(c){return 0;}else{t=1;} return t;
cat > $D/v1 <<'EOF'
int32_t f(int32_t a){ int32_t t; if(a>0){ return 0; } else { t=1; } return t; }
EOF
run "F15 then-returns else-assigns read-after" accept $D/v1

# both branches assign, but nested: if(c){if(d){t=1;}else{t=2;}}else{t=3;} -> all paths assign
cat > $D/v2 <<'EOF'
int32_t f(int32_t a, int32_t b){ int32_t t; if(a>0){ if(b>0){t=1;}else{t=2;} } else { t=3; } return t; }
EOF
run "F15 fully-nested exhaustive assign" accept $D/v2

# FSM: sequential with ref output, read-write (reg) - lint the FSM
cat > $D/seq.c <<'EOF'
__sequential uint32_t sum_to(uint32_t n){ uint32_t acc=0; uint32_t i=0; while(i<n){ acc+=i; i++; } return acc; }
EOF
echo "=== sequential FSM lint ==="
LOGGING_LEVEL=CRITICAL $BIN < $D/seq.c 2>/dev/null > $D/seq.sv
verilator --lint-only -Wall -Wno-DECLFILENAME $D/seq.sv 2>&1 | grep -iE "latch|unopt|warn|error" | head

# constexpr division by zero guard?
cat > $D/dz <<'EOF'
constexpr int32_t x = 5 / 0;
int32_t f(){ return x; }
EOF
run "constexpr div-by-zero" reject $D/dz

# constexpr modulo by zero
cat > $D/mz <<'EOF'
constexpr int32_t x = 5 % 0;
int32_t f(){ return x; }
EOF
run "constexpr mod-by-zero" reject $D/mz

# INT_MIN / -1 overflow in constexpr
cat > $D/ov <<'EOF'
constexpr int64_t x = (-9223372036854775807 - 1) / -1;
int64_t f(){ return x; }
EOF
run "constexpr INT64_MIN / -1 overflow" reject $D/ov
