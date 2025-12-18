#!/bin/bash
# Main test runner - compares CBASIC output against AppleSoft BASIC reference
# Usage: run_tests.sh [test_pattern] [--generate-refs]

# Don't exit on error - we handle test failures ourselves
# set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HARNESS_DIR="$(dirname "$SCRIPT_DIR")"
TESTS_DIR="$HARNESS_DIR/tests"
OUTPUT_DIR="$HARNESS_DIR/output"
REF_DIR="$HARNESS_DIR/reference/outputs"

CBASIC_BIN="${CBASIC_BIN:-/Users/tb/dev/CBASIC/bin/cbasic}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
PASS=0
FAIL=0
SKIP=0

# Parse arguments
TEST_PATTERN="*.bas"
GENERATE_REFS=false
for arg in "$@"; do
    case $arg in
        --generate-refs)
            GENERATE_REFS=true
            ;;
        -*)
            echo "Unknown option: $arg"
            exit 1
            ;;
        *)
            TEST_PATTERN="$arg"
            ;;
    esac
done

echo "=============================================="
echo "CBASIC vs AppleSoft Compatibility Test Suite"
echo "=============================================="
echo ""

# Create output directories
mkdir -p "$OUTPUT_DIR/cbasic"
mkdir -p "$REF_DIR"

# Run a single test
run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .bas)
    local cbasic_out="$OUTPUT_DIR/cbasic/${test_name}.out"
    local ref_out="$REF_DIR/${test_name}.ref"
    local input_file="$TESTS_DIR/${test_name}.input"

    printf "Testing %-30s " "$test_name..."

    # Check for input file
    local input_arg=""
    if [ -f "$input_file" ]; then
        input_arg="$input_file"
    fi

    # Run CBASIC
    if [ -n "$input_arg" ]; then
        "$CBASIC_BIN" "$test_file" < "$input_arg" > "$cbasic_out" 2>&1
    else
        "$CBASIC_BIN" "$test_file" < /dev/null > "$cbasic_out" 2>&1
    fi

    # Remove CBASIC header for comparison
    tail -n +2 "$cbasic_out" > "${cbasic_out}.tmp" && mv "${cbasic_out}.tmp" "$cbasic_out"

    # Check for reference output
    if [ ! -f "$ref_out" ]; then
        if [ "$GENERATE_REFS" = true ]; then
            # Copy CBASIC output as reference (user should verify)
            cp "$cbasic_out" "$ref_out"
            echo -e "${YELLOW}GENERATED${NC} (verify manually)"
            SKIP=$((SKIP + 1))
            return
        else
            echo -e "${YELLOW}SKIP${NC} (no reference)"
            SKIP=$((SKIP + 1))
            return
        fi
    fi

    # Compare outputs
    if python3 "$SCRIPT_DIR/compare_output.py" "$cbasic_out" "$ref_out" --no-skip-header > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${NC}"
        FAIL=$((FAIL + 1))
        # Show diff
        echo "  Differences:"
        python3 "$SCRIPT_DIR/compare_output.py" "$cbasic_out" "$ref_out" --no-skip-header 2>&1 | head -10 | sed 's/^/    /'
    fi
}

# Find and run tests
echo "Looking for tests in: $TESTS_DIR"
echo "Pattern: $TEST_PATTERN"
echo ""

if [ ! -d "$TESTS_DIR" ]; then
    echo "Error: Tests directory not found: $TESTS_DIR"
    echo "Please create test programs in $TESTS_DIR"
    exit 1
fi

test_count=0
for test_file in "$TESTS_DIR"/$TEST_PATTERN; do
    if [ -f "$test_file" ]; then
        run_test "$test_file"
        test_count=$((test_count + 1))
    fi
done

if [ $test_count -eq 0 ]; then
    echo "No tests found matching pattern: $TEST_PATTERN"
    exit 1
fi

# Summary
echo ""
echo "=============================================="
echo "Summary"
echo "=============================================="
echo -e "  ${GREEN}PASS${NC}: $PASS"
echo -e "  ${RED}FAIL${NC}: $FAIL"
echo -e "  ${YELLOW}SKIP${NC}: $SKIP"
echo "  Total: $test_count"
echo ""

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
