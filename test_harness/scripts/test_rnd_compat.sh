#!/bin/bash
#
# RND Compatibility Test
#
# Tests that CBASIC and cbmbasic produce identical random number sequences.
# This is critical for game compatibility since most games rely on RND.
#

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
CBASIC="$PROJECT_DIR/bin/cbasic"
OUTPUT_DIR="$SCRIPT_DIR/../output/rnd_tests"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "=============================================="
echo "RND Compatibility Test"
echo "=============================================="
echo ""

# Check dependencies
if ! command -v cbmbasic &> /dev/null; then
    echo -e "${RED}Error: cbmbasic is required${NC}"
    exit 1
fi

if [ ! -x "$CBASIC" ]; then
    echo -e "${RED}Error: CBASIC not found at $CBASIC${NC}"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

pass=0
fail=0

# Test 1: First 100 values after RND(-1) seed
echo "Test 1: First 100 values after RND(-1) seed"
cat > "$OUTPUT_DIR/test1.bas" << 'EOF'
10 X = RND(-1)
20 FOR I = 1 TO 100
30 PRINT RND(1)
40 NEXT I
EOF

# Normalize: remove leading 0 before decimal, strip whitespace and control chars
# Note: cbmbasic outputs very small numbers in scientific notation (e.g., 1.03733097E-03)
$CBASIC "$OUTPUT_DIR/test1.bas" 2>/dev/null | grep -E '^\s*[0-9.-]' | sed 's/^ *//' | sed 's/ *$//' | sed 's/^0\./\./' > "$OUTPUT_DIR/test1_cbasic.txt"
cbmbasic "$OUTPUT_DIR/test1.bas" 2>/dev/null | grep -E '^\s*[0-9.-]' | tr -d '\r\035' | sed 's/^ *//' | sed 's/ *$//' > "$OUTPUT_DIR/test1_cbm.txt"

# Use Python fuzzy comparison (allows for last-digit rounding differences)
if python3 "$SCRIPT_DIR/compare_output.py" "$OUTPUT_DIR/test1_cbm.txt" "$OUTPUT_DIR/test1_cbasic.txt" > /dev/null 2>&1; then
    echo -e "  ${GREEN}PASS${NC} - 100 values match (fuzzy comparison)"
    pass=$((pass + 1))
else
    echo -e "  ${RED}FAIL${NC} - Values differ significantly"
    echo "  First difference:"
    diff "$OUTPUT_DIR/test1_cbasic.txt" "$OUTPUT_DIR/test1_cbm.txt" | head -3
    fail=$((fail + 1))
fi

# Test 2: 10000 values
echo "Test 2: First 10000 values"
cat > "$OUTPUT_DIR/test2.bas" << 'EOF'
10 X = RND(-1)
20 FOR I = 1 TO 10000
30 Y = RND(1)
40 NEXT I
50 PRINT Y
EOF

my=$($CBASIC "$OUTPUT_DIR/test2.bas" 2>/dev/null | grep -E '^\s*[0-9]' | head -1 | tr -d ' ' | sed 's/^0\./\./')
cbm=$(cbmbasic "$OUTPUT_DIR/test2.bas" 2>/dev/null | grep -E '^\s*\.[0-9]' | tail -1 | tr -d ' \r' | tr -d '\035')

if [ "$my" = "$cbm" ]; then
    echo -e "  ${GREEN}PASS${NC} - 10000th value: $my"
    pass=$((pass + 1))
else
    echo -e "  ${RED}FAIL${NC} - CBASIC: $my, cbmbasic: $cbm"
    fail=$((fail + 1))
fi

# Test 3: 100000 values
echo "Test 3: First 100000 values"
cat > "$OUTPUT_DIR/test3.bas" << 'EOF'
10 X = RND(-1)
20 FOR I = 1 TO 100000
30 Y = RND(1)
40 NEXT I
50 PRINT Y
EOF

my=$($CBASIC "$OUTPUT_DIR/test3.bas" 2>/dev/null | grep -E '^\s*[0-9]' | head -1 | tr -d ' ' | sed 's/^0\./\./')
cbm=$(cbmbasic "$OUTPUT_DIR/test3.bas" 2>/dev/null | grep -E '^\s*\.[0-9]' | tail -1 | tr -d ' \r' | tr -d '\035')

if [ "$my" = "$cbm" ]; then
    echo -e "  ${GREEN}PASS${NC} - 100000th value: $my"
    pass=$((pass + 1))
else
    echo -e "  ${RED}FAIL${NC} - CBASIC: $my, cbmbasic: $cbm"
    fail=$((fail + 1))
fi

# Test 4: 1000000 values
echo "Test 4: First 1000000 values (may take a moment)"
cat > "$OUTPUT_DIR/test4.bas" << 'EOF'
10 X = RND(-1)
20 FOR I = 1 TO 1000000
30 Y = RND(1)
40 NEXT I
50 PRINT Y
EOF

my=$($CBASIC "$OUTPUT_DIR/test4.bas" 2>/dev/null | grep -E '^\s*[0-9]' | head -1 | tr -d ' ' | sed 's/^0\./\./')
cbm=$(cbmbasic "$OUTPUT_DIR/test4.bas" 2>/dev/null | grep -E '^\s*\.[0-9]' | tail -1 | tr -d ' \r' | tr -d '\035')

if [ "$my" = "$cbm" ]; then
    echo -e "  ${GREEN}PASS${NC} - 1000000th value: $my"
    pass=$((pass + 1))
else
    echo -e "  ${RED}FAIL${NC} - CBASIC: $my, cbmbasic: $cbm"
    fail=$((fail + 1))
fi

# Test 5: Edge case at iteration 89071 (was previously failing)
echo "Test 5: Iteration 89071 edge case"
cat > "$OUTPUT_DIR/test5.bas" << 'EOF'
10 X = RND(-1)
20 FOR I = 1 TO 89071
30 Y = RND(1)
40 NEXT I
50 PRINT Y
EOF

my=$($CBASIC "$OUTPUT_DIR/test5.bas" 2>/dev/null | grep -E '^\s*[0-9]' | head -1 | tr -d ' ' | sed 's/^0\./\./')
cbm=$(cbmbasic "$OUTPUT_DIR/test5.bas" 2>/dev/null | grep -E '^\s*\.[0-9]' | tail -1 | tr -d ' \r' | tr -d '\035')

if [ "$my" = "$cbm" ]; then
    echo -e "  ${GREEN}PASS${NC} - 89071st value: $my"
    pass=$((pass + 1))
else
    echo -e "  ${RED}FAIL${NC} - CBASIC: $my, cbmbasic: $cbm"
    fail=$((fail + 1))
fi

# Test 6: RND(0) behavior (platform-specific)
# Note: On C64, RND(0) uses system timers. On AppleSoft, RND(0) returns last value.
# CBASIC implements AppleSoft behavior (returns last value).
echo "Test 6: RND(0) returns last value (AppleSoft behavior)"
cat > "$OUTPUT_DIR/test6.bas" << 'EOF'
10 X = RND(-1)
20 A = RND(1)
30 B = RND(0)
40 IF A = B THEN PRINT "MATCH"
50 IF A <> B THEN PRINT "DIFFER"
EOF

my=$($CBASIC "$OUTPUT_DIR/test6.bas" 2>/dev/null | grep -E 'MATCH|DIFFER')

# CBASIC should return MATCH (AppleSoft behavior where RND(0) returns last value)
if [ "$my" = "MATCH" ]; then
    echo -e "  ${GREEN}PASS${NC} - RND(0) returns last value (AppleSoft-compatible)"
    pass=$((pass + 1))
else
    echo -e "  ${RED}FAIL${NC} - Expected MATCH, got: $my"
    fail=$((fail + 1))
fi
echo "  (Note: cbmbasic uses C64 timer-based RND(0), which differs)"

echo ""
echo "=============================================="
echo "Summary"
echo "=============================================="
echo -e "  ${GREEN}PASS${NC}: $pass"
echo -e "  ${RED}FAIL${NC}: $fail"
echo "  Total: $((pass + fail))"
echo ""

if [ $fail -eq 0 ]; then
    echo -e "${GREEN}All RND tests pass! Game compatibility verified.${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed.${NC}"
    exit 1
fi
