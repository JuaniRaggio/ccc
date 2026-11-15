BIN=.build/Flex-Bison-Compiler
printf '__sequential uint32_t f(uint32_t n){ uint32_t acc=0; if(n==0){ return 0; } uint32_t i=0; while(i<n){ acc+=i; i++; } return acc; }\n' > /tmp/s.c
echo "=== accept? ==="; LOGGING_LEVEL=CRITICAL $BIN < /tmp/s.c > /tmp/s.sv 2>/dev/null; echo "EXIT=$?"
grep -nE "out <=|if|CCC_" /tmp/s.sv | head -40
