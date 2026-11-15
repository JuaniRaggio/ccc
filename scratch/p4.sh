#!/bin/bash
BIN=.build/Flex-Bison-Compiler
run() {
  local name="$1"; local expect="$2"; local file="$3"
  LOGGING_LEVEL=CRITICAL $BIN < "$file" >/tmp/out.$$ 2>&1
  local ec=$?; local got="accept"; [ $ec -ne 0 ] && got="reject"
  local mark="OK"; [ "$got" != "$expect" ] && mark="**CHECK**"
  printf "%-52s expect=%-7s got=%-7s %s\n" "$name" "$expect" "$got" "$mark"
}
D=/tmp/p4; mkdir -p $D

# chained: (a + 1) where a:int8 -> common int8; then + b:int32 -> narrow? 
# a + 1 => int8 (1 fits int8, a wins). (int8) + int32 b -> common int32. assign int8 -> reject
cat > $D/a <<'EOF'
int8_t f(int8_t a, int32_t b){ int8_t r; r = (a + 1) + b; return r; }
EOF
run "F11 chained (a+1)+b narrow" reject $D/a

# a:int8 + 1 assigned to int8 -> int8, fine
cat > $D/b <<'EOF'
int8_t f(int8_t a){ int8_t r; r = a + 1; return r; }
EOF
run "F11 a+1 -> int8" accept $D/b

# int32 c = 1 + b, b:int8 ; 1 fits int8 so common=int8, assign to int32 widening ok
cat > $D/c <<'EOF'
int32_t f(int8_t b){ int32_t r; r = 1 + b; return r; }
EOF
run "F11 1+i8 -> i32 (widen)" accept $D/c

# SILENT NARROW hole hunt: a:int32 + 0 assigned int8. 0 fits int8, so leftAdapts? 
# left=a(int32) is not const. right=0 const fits int32(lt) -> rightAdapts, common=lt=int32.
# assign int32 -> int8 -> reject. good, no silent narrow.
cat > $D/d <<'EOF'
int8_t f(int32_t a){ int8_t r; r = a + 0; return r; }
EOF
run "F11 i32+0 -> i8 (narrow)" reject $D/d

# both constants: 100 + 20 assigned int8 -> common? both adapt. 120 fits int8. 
cat > $D/e <<'EOF'
int8_t f(){ int8_t r; r = 100 + 20; return r; }
EOF
run "F11 const 100+20 -> i8 (120 fits)" accept $D/e

# both constants overflow target: 100 + 100 = 200 > int8 max. 
# these are int literals (TYPE_INT32 default?). common type both adapt... 
# Does assignment narrowing use evalConstexpr(whole expr)=200, fits int8? no -> reject
cat > $D/f <<'EOF'
int8_t f(){ int8_t r; r = 100 + 100; return r; }
EOF
run "F11 const 100+100 -> i8 (200 overflow)" reject $D/f

# mixed signedness still rejected even if const adapts
cat > $D/g <<'EOF'
int32_t f(int32_t a, uint32_t b){ return a + b; }
EOF
run "F1/F11 signed+unsigned still reject" reject $D/g

# literal fits the OTHER operand but narrower result: u16 b + 300 assigned u8. 
# 300 fits u16 -> leftover... right=300 const fits u16(lt)? yes rightAdapts, common=u16. assign u8 reject
cat > $D/h <<'EOF'
uint8_t f(uint16_t b){ uint8_t r; r = b + 300; return r; }
EOF
run "F11 u16+300 -> u8 narrow" reject $D/h

# WIDTH question: int64 + int32 -> int64, assign int64 ok
cat > $D/i <<'EOF'
int64_t f(int64_t a, int32_t b){ return a + b; }
EOF
run "F11 i64+i32 -> i64" accept $D/i

# does bool enter arithmetic? a:int8 + true -> category mismatch should reject
cat > $D/j <<'EOF'
int8_t f(int8_t a){ return a + true; }
EOF
run "arith int + bool (category)" reject $D/j
