BIN=.build/Flex-Bison-Compiler
cat > /tmp/s1 <<'EOF'
void __sequential accum(int32_t &acc, int32_t x){ acc = acc + x; }
EOF
echo "=== full output (default logging) ==="
LOGGING_LEVEL=ERROR $BIN < /tmp/s1 2>&1 | tail -20
echo "EXIT=$?"
