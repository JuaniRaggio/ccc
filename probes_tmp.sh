#!/bin/bash
BIN=.build/Flex-Bison-Compiler
run() {
  local name="$1"; local expect="$2"; local file="$3"
  LOGGING_LEVEL=CRITICAL $BIN < "$file" >/tmp/out.$$ 2>&1
  local ec=$?
  local got="accept"; [ $ec -ne 0 ] && got="reject"
  local mark="OK"; [ "$got" != "$expect" ] && mark="**MISMATCH**"
  printf "%-40s expect=%-7s got=%-7s %s\n" "$name" "$expect" "$got" "$mark"
}
D=/tmp/probes
mkdir -p $D

# ---- F11 probes ----
cat > $D/p11_1 <<'EOF'
uint32_t f(uint32_t b){ uint32_t r; r = 1 + b; return r; }
EOF
run "F11 1+b" accept $D/p11_1

cat > $D/p11_2 <<'EOF'
uint32_t f(uint32_t b){ uint32_t r; r = b + 1; return r; }
EOF
run "F11 b+1" accept $D/p11_2

# uint8 a + uint16 b assigned to uint8 : common type uint16, narrowing to uint8 -> should require cast?
cat > $D/p11_3 <<'EOF'
uint8_t f(uint8_t a, uint16_t b){ uint8_t r; r = a + b; return r; }
EOF
run "F11 u8+u16->u8 (narrow)" reject $D/p11_3

# int8 + int32 assigned int8 -> narrowing
cat > $D/p11_4 <<'EOF'
int8_t f(int8_t a, int32_t b){ return a + b; }
EOF
run "F11 i8+i32->i8 return" reject $D/p11_4

# literal that does NOT fit the other type: 1000 + int8 b
cat > $D/p11_5 <<'EOF'
int32_t f(int8_t b){ int32_t r; r = 1000 + b; return r; }
EOF
run "F11 1000+i8 (lit no fit i8)" accept $D/p11_5

# does a fitting literal cause over-narrowing: 1 + int32 b assigned int8?
cat > $D/p11_6 <<'EOF'
int8_t f(int32_t b){ return 1 + b; }
EOF
run "F11 1+i32->i8 (narrow)" reject $D/p11_6

# wider op assigned to wider: u8 + u16 -> u16, OK
cat > $D/p11_7 <<'EOF'
uint16_t f(uint8_t a, uint16_t b){ uint16_t r; r = a + b; return r; }
EOF
run "F11 u8+u16->u16" accept $D/p11_7

# same width ok
cat > $D/p11_8 <<'EOF'
int8_t f(int8_t a, int8_t b){ return a + b; }
EOF
run "F11 i8+i8->i8" accept $D/p11_8

# ---- F13 probes ----
cat > $D/p13_1 <<'EOF'
void accum(int32_t &acc, int32_t x){ acc = acc + x; }
EOF
run "F13 rw ref comb" reject $D/p13_1

# write-only ref ok
cat > $D/p13_2 <<'EOF'
void setit(int32_t &o, int32_t x){ o = x; }
EOF
run "F13 write-only ref" accept $D/p13_2

# write-then-read not a loop: o = x; y = o;  (o written before read)
cat > $D/p13_3 <<'EOF'
void f(int32_t &o, int32_t x){ int32_t y; o = x; y = o; o = y + 1; }
EOF
run "F13 write-then-read ref (over-rej?)" reject $D/p13_3

# bypass: read-write ref through a call
cat > $D/p13_4 <<'EOF'
void inc(int32_t &a){ a = a + 1; }
void outer(int32_t &b){ inc(b); }
EOF
run "F13 rw ref via call (bypass?)" reject $D/p13_4

# __sequential rw ref allowed
cat > $D/p13_5 <<'EOF'
void __sequential accum(int32_t &acc, int32_t x){ acc = acc + x; }
EOF
run "F13 rw ref sequential ok" accept $D/p13_5

# ---- F15 probes ----
cat > $D/p15_1 <<'EOF'
int32_t f(int32_t a){ int32_t t; if(a>0){t=1;} return t; }
EOF
run "F15 assigned only in if (latch)" reject $D/p15_1

cat > $D/p15_2 <<'EOF'
int32_t f(int32_t a){ int32_t t; if(a>0){t=1;}else{t=2;} return t; }
EOF
run "F15 if/else both assign" accept $D/p15_2

# assigned in loop that always runs -> OVER-REJECTION probe
cat > $D/p15_3 <<'EOF'
int32_t f(){ int32_t t; for(int32_t i=0;i<4;i=i+1){t=i;} return t; }
EOF
run "F15 assigned in for-loop (over-rej?)" reject $D/p15_3

# assigned via decl with init
cat > $D/p15_4 <<'EOF'
int32_t f(){ int32_t t = 5; return t; }
EOF
run "F15 decl-init" accept $D/p15_4

# compound assignment on unassigned local -> error (reads t)
cat > $D/p15_5 <<'EOF'
int32_t f(){ int32_t t; t += 1; return t; }
EOF
run "F15 compound on unassigned" reject $D/p15_5

# array local written then read
cat > $D/p15_6 <<'EOF'
int32_t f(){ int32_t a[4]; a[0]=1; return a[0]; }
EOF
run "F15 array write-then-read" accept $D/p15_6

# array local read before any write -> should reject? (latch)
cat > $D/p15_7 <<'EOF'
int32_t f(){ int32_t a[4]; return a[0]; }
EOF
run "F15 array read before write" reject $D/p15_7

# assigned in both branches via nested if chain (switch-like)
cat > $D/p15_8 <<'EOF'
int32_t f(int32_t a){ int32_t t; if(a>2){t=1;}else{ if(a>0){t=2;}else{t=3;} } return t; }
EOF
run "F15 nested if-else chain" accept $D/p15_8

# assigned via call ref output then read -> should accept
cat > $D/p15_9 <<'EOF'
void setit(int32_t &o){ o = 7; }
int32_t f(){ int32_t t; setit(t); return t; }
EOF
run "F15 assigned via call ref (over-rej?)" accept $D/p15_9

# read in own initializer? decl int t = t; -> t read before assigned
cat > $D/p15_10 <<'EOF'
int32_t f(){ int32_t t = t + 1; return t; }
EOF
run "F15 self-init read (latch?)" reject $D/p15_10

# ---- F14 probe (constexpr -INT64_MIN) ----
cat > $D/p14_1 <<'EOF'
constexpr int64_t x = -(-9223372036854775807 - 1);
int64_t f(){ return x; }
EOF
run "F14 neg INT64_MIN" reject $D/p14_1

cat > $D/p14_2 <<'EOF'
constexpr int64_t x = __abs(-9223372036854775807 - 1);
int64_t f(){ return x; }
EOF
run "F14 abs INT64_MIN" reject $D/p14_2
