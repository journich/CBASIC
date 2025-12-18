#!/bin/bash
# Fully automated CBASIC vs MS BASIC comparison
# Usage: run_comparison.sh [test_pattern]
#
# Compares CBASIC output against cbmbasic (Commodore 64 BASIC V2)
# which uses the same Microsoft BASIC 6502 source code as AppleSoft

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HARNESS_DIR="$(dirname "$SCRIPT_DIR")"
TESTS_DIR="$HARNESS_DIR/tests"
OUTPUT_DIR="$HARNESS_DIR/output"

CBASIC_BIN="${CBASIC_BIN:-/Users/tb/dev/CBASIC/bin/cbasic}"

# Check for cbmbasic
if ! command -v cbmbasic &> /dev/null; then
    echo "Error: cbmbasic not found. Install with: brew install cbmbasic"
    exit 1
fi

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Counters
PASS=0
FAIL=0
TOTAL=0

TEST_PATTERN="${1:-*.bas}"

echo "=============================================="
echo "CBASIC vs Microsoft BASIC Automated Comparison"
echo "=============================================="
echo "Reference: Commodore 64 BASIC V2 (cbmbasic)"
echo "          Same MS BASIC 6502 codebase as AppleSoft"
echo ""

mkdir -p "$OUTPUT_DIR/cbasic" "$OUTPUT_DIR/msbasic"

run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .bas)
    local cbasic_out="$OUTPUT_DIR/cbasic/${test_name}.out"
    local msbasic_out="$OUTPUT_DIR/msbasic/${test_name}.out"

    printf "Testing %-30s " "$test_name..."

    # Run CBASIC (skip header "CBASIC 1.1" and blank line after it)
    "$CBASIC_BIN" "$test_file" < /dev/null 2>&1 | tail -n +3 > "$cbasic_out"

    # Run MS BASIC (cbmbasic)
    "$SCRIPT_DIR/run_msbasic.sh" "$test_file" > "$msbasic_out" 2>&1

    # Compare with fuzzy float matching
    if python3 "$SCRIPT_DIR/compare_output.py" "$cbasic_out" "$msbasic_out" --no-skip-header > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${NC}"
        FAIL=$((FAIL + 1))
        # Show differences
        echo -e "  ${CYAN}Differences:${NC}"
        python3 "$SCRIPT_DIR/compare_output.py" "$cbasic_out" "$msbasic_out" --no-skip-header 2>&1 | head -15 | sed 's/^/    /'
    fi

    TOTAL=$((TOTAL + 1))
}

# Run tests
for test_file in "$TESTS_DIR"/$TEST_PATTERN; do
    if [ -f "$test_file" ]; then
        run_test "$test_file"
    fi
done

if [ $TOTAL -eq 0 ]; then
    echo "No tests found matching: $TEST_PATTERN"
    exit 1
fi

# Summary
echo ""
echo "=============================================="
echo "Summary"
echo "=============================================="
echo -e "  ${GREEN}PASS${NC}: $PASS"
echo -e "  ${RED}FAIL${NC}: $FAIL"
echo "  Total: $TOTAL"
echo ""

PASS_RATE=$((PASS * 100 / TOTAL))
if [ $PASS_RATE -eq 100 ]; then
    echo -e "${GREEN}100% compatible with Microsoft BASIC!${NC}"
elif [ $PASS_RATE -ge 90 ]; then
    echo -e "${YELLOW}${PASS_RATE}% compatible${NC}"
else
    echo -e "${RED}${PASS_RATE}% compatible - significant differences found${NC}"
fi

exit $FAIL
