#!/bin/bash
BIN=.build/Flex-Bison-Compiler
run() {
  local name="$1"; local expect="$2"; local file="$3"
  LOGGING_LEVEL=CRITICAL $BIN < "$file" >/tmp/out.$$ 2>&1
  local ec=$?; local got="accept"; [ $ec -ne 0 ] && got="reject"
  local mark="OK"; [ "$got" != "$expect" ] && mark="**CHECK**"
  printf "%-50s expect=%-7s got=%-7s %s\n" "$name" "$expect" "$got" "$mark"
}
D=/tmp/p3; mkdir -p $D

# SOUNDNESS: write to one array element, read a DIFFERENT element. 
# collectStmtAccesses adds whole-array name to writes on a[0]=1, so a counts assigned.
# But a[1] was never written -> reading a[1] is a latch/X. Does frontend catch?
cat > $D/a <<'EOF'
int32_t f(){ int32_t a[4]; a[0]=1; return a[1]; }
EOF
run "SOUND array elem: write a[0] read a[1] (latch?)" reject $D/a

# SOUNDNESS: write in if-then marks array assigned via whole-name; read after. partial.
cat > $D/b <<'EOF'
int32_t f(int32_t c){ int32_t a[2]; if(c>0){a[0]=1;} return a[0]; }
EOF
run "SOUND array partial-assign in if (latch?)" reject $D/b

# A function that writes an out array only partially: buf[0]=x; (buf[1..3] undriven)
# This is F12 territory: output port only partially driven -> latch on unwritten bits?
cat > $D/c <<'EOF'
void fill(uint32_t x, uint32_t buf[4]){ buf[0]=x; }
EOF
run "F12 partial out-array drive (latch?)" accept $D/c

# ORDER within one statement: t = (t=5) + t   ... weird, assignment as expr
# SOUNDNESS of read/write ordering: does frontend treat RHS use correctly?
cat > $D/d <<'EOF'
int32_t f(){ int32_t t; int32_t u; u = (t = 5); return t + u; }
EOF
run "F15 assign-expr defines t" accept $D/d

# local assigned inside for-init (runs once) then read after loop
cat > $D/e <<'EOF'
int32_t f(){ int32_t t; for(t=0;t<4;t=t+1){} return t; }
EOF
run "F15 assigned in for-init read-after" accept $D/e

# local used as loop bound before assign
cat > $D/f <<'EOF'
int32_t f(){ int32_t n; int32_t s=0; for(int32_t i=0;i<n;i=i+1){s=s+i;} return s; }
EOF
run "F15 read n (unassigned) in for-cond (latch)" reject $D/f

# return in both branches, no fallthrough, local assigned in neither but not read after
cat > $D/g <<'EOF'
int32_t f(int32_t a){ int32_t t; if(a>0){ t=1; return t; } else { t=2; return t; } }
EOF
run "F15 both branches assign+return" accept $D/g

# local only read never assigned, but never reached? dead - still read in AST
cat > $D/h <<'EOF'
int32_t f(){ int32_t t; return t; }
EOF
run "F15 read fully-unassigned local (latch)" reject $D/h
