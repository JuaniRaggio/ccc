#!/bin/bash
BIN=.build/Flex-Bison-Compiler
run() {
  local name="$1"; local expect="$2"; local file="$3"
  LOGGING_LEVEL=CRITICAL $BIN < "$file" >/tmp/out.$$ 2>&1
  local ec=$?
  local got="accept"; [ $ec -ne 0 ] && got="reject"
  local mark="OK"; [ "$got" != "$expect" ] && mark="**MISMATCH**"
  printf "%-45s expect=%-7s got=%-7s %s\n" "$name" "$expect" "$got" "$mark"
}
D=/tmp/p2; mkdir -p $D

# F13 rw ref sequential, correct syntax
cat > $D/a <<'EOF'
__sequential void accum(int32_t &acc, int32_t x){ acc = acc + x; }
EOF
run "F13 rw ref __sequential (correct syntax)" accept $D/a

# F15: local assigned every iter of always-running for, read after
cat > $D/b <<'EOF'
int32_t f(){ int32_t t; for(int32_t i=0;i<4;i=i+1){t=i;} return t; }
EOF
run "F15 loop-assigned read-after (over-rej)" reject $D/b

# F15: while always-runs variant
cat > $D/c <<'EOF'
int32_t f(){ int32_t t; int32_t i=0; while(i<4){t=i; i=i+1;} return t; }
EOF
run "F15 while-assigned read-after (over-rej)" reject $D/c

# F15 SOUNDNESS: assigned in then-branch, read in the SAME branch after -> fine
cat > $D/d <<'EOF'
int32_t f(int32_t a){ int32_t t; if(a>0){ t=1; return t; } return 0; }
EOF
run "F15 assign+read same branch" accept $D/d

# F15 SOUNDNESS HOLE candidate: assign in if with no else, but early-return in the OTHER path.
# then read after if. Classic definite-assignment: if(c){t=1;}else{return 0;} use t  -> assigned
cat > $D/e <<'EOF'
int32_t f(int32_t a){ int32_t t; if(a>0){ t=1; } else { return 0; } return t; }
EOF
run "F15 assign-in-then else-returns (over-rej?)" accept $D/e

# F15 SOUNDNESS: if(c) return; t=1 ... actually: if(!c){return 0;} t=1; use t -> but read of t in then? 
cat > $D/f <<'EOF'
int32_t f(int32_t a){ int32_t t; if(a<=0){ return 0; } t=1; return t; }
EOF
run "F15 guard-return then assign" accept $D/f

# F15 SOUNDNESS: read inside a nested branch where NOT all outer paths assign
cat > $D/g <<'EOF'
int32_t f(int32_t a){ int32_t t; if(a>0){ if(a>5){ t=1; } } return t; }
EOF
run "F15 nested-if partial assign (latch)" reject $D/g

# F15: assigned in both if/else but one via compound (reads unassigned?) 
cat > $D/h <<'EOF'
int32_t f(int32_t a){ int32_t t; if(a>0){t=1;}else{t=2;} t = t + 1; return t; }
EOF
run "F15 if/else then compound-use" accept $D/h

# F15 param used as local - params pre-assigned; array param
cat > $D/i <<'EOF'
void f(uint32_t buf[4]){ buf[0]=buf[1]; }
EOF
run "F15 array param read (pre-assigned)" accept $D/i

# F15: ref param read-before-write in __sequential should be allowed (it's a reg)
cat > $D/j <<'EOF'
__sequential void f(int32_t &acc){ acc = acc + 1; }
EOF
run "F15 seq ref read (reg)" accept $D/j

# F15 SOUNDNESS HOLE: a local read in the condition of if, assigned only before in one path
cat > $D/k <<'EOF'
int32_t f(int32_t a){ int32_t t; if(a>0){t=1;} if(t>0){ return 1; } return 0; }
EOF
run "F15 read in later if-cond (latch)" reject $D/k

# F15: local declared then assigned in both arms of a for-contained if that always runs? messy - skip

# DEEP: definite assignment with parallel block
cat > $D/l <<'EOF'
int32_t f(int32_t a){ int32_t t; __parallel{ t = a; } return t; }
EOF
run "F15 parallel assign then read" accept $D/l

# DEEP: read a local inside __parallel that another unit writes (concurrent, X) -> should reject
cat > $D/m <<'EOF'
void f(int32_t a, int32_t buf[2]){ int32_t t; __parallel{ t = a; buf[0] = t; } }
EOF
run "F15 parallel read-other-write (X)" reject $D/m
