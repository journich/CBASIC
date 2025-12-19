#!/bin/bash
#
# Automated testing of David Ahl's BASIC Computer Games
#
# This script:
# 1. Downloads the games if not present
# 2. Runs deterministic (non-interactive) games through both CBASIC and cbmbasic
# 3. Compares outputs using fuzzy float matching
#
# Some games are interactive and cannot be tested automatically.
# This script focuses on games that produce deterministic output.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HARNESS_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_DIR="$(dirname "$HARNESS_DIR")"
GAMES_DIR="$PROJECT_DIR/test_programs/ahl_games"
OUTPUT_DIR="$HARNESS_DIR/output/ahl_games"
CBASIC="$PROJECT_DIR/bin/cbasic"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Test timeout in seconds
TIMEOUT=10

# macOS doesn't have timeout command; try gtimeout or skip timeout
if command -v gtimeout &> /dev/null; then
    TIMEOUT_CMD="gtimeout"
elif command -v timeout &> /dev/null; then
    TIMEOUT_CMD="timeout"
else
    TIMEOUT_CMD=""
    echo -e "${YELLOW}Note: timeout command not found. Tests may hang on infinite loops.${NC}"
    echo ""
fi

run_with_timeout() {
    if [ -n "$TIMEOUT_CMD" ]; then
        $TIMEOUT_CMD $TIMEOUT "$@"
    else
        "$@"
    fi
}

echo "=============================================="
echo "David Ahl's BASIC Computer Games Test Suite"
echo "=============================================="
echo ""

# Check dependencies
if ! command -v cbmbasic &> /dev/null; then
    echo -e "${RED}Error: cbmbasic is required but not installed.${NC}"
    echo "Install with: brew install cbmbasic"
    exit 1
fi

if [ ! -x "$CBASIC" ]; then
    echo -e "${RED}Error: CBASIC not found at $CBASIC${NC}"
    echo "Build with: make"
    exit 1
fi

# Download games if not present
if [ ! -d "$GAMES_DIR" ] || [ -z "$(ls -A "$GAMES_DIR" 2>/dev/null)" ]; then
    echo -e "${YELLOW}Games not found. Downloading...${NC}"
    echo ""
    # Run download script non-interactively
    echo "y" | "$SCRIPT_DIR/download_ahl_games.sh"
    echo ""
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# List of deterministic games that can be tested automatically
# These games produce consistent output without requiring user input
# Found via: grep -Li "INPUT" test_programs/ahl_games/*.bas
TESTABLE_GAMES=(
    "19_bunny"        # ASCII art bunny
    "21_calendar"     # Prints calendar
    "70_poetry"       # Random poetry (uses RND)
    "78_sine_wave"    # Prints sine wave
    "87_3d_plot"      # 3D function plot
)

# Statistics
pass=0
fail=0
skip=0
total=0

echo "Testing deterministic games..."
echo ""

# Test each game
for game in "$GAMES_DIR"/*.bas; do
    if [ ! -f "$game" ]; then
        continue
    fi

    game_name=$(basename "$game" .bas)
    total=$((total + 1))

    # Check if this game is testable
    testable=false
    for t in "${TESTABLE_GAMES[@]}"; do
        if [ "$t" = "$game_name" ]; then
            testable=true
            break
        fi
    done

    if [ "$testable" = false ]; then
        # Skip interactive games
        printf "  %-25s ${YELLOW}SKIP${NC} (interactive)\n" "$game_name"
        skip=$((skip + 1))
        continue
    fi

    # Create test input (empty for most games)
    input=""

    # Run through cbmbasic (reference)
    cbm_out="$OUTPUT_DIR/${game_name}_cbm.txt"
    if run_with_timeout bash -c "echo '$input' | cbmbasic '$game' 2>/dev/null" > "$cbm_out" 2>&1; then
        cbm_ok=true
    else
        cbm_ok=false
    fi

    # Run through CBASIC
    my_out="$OUTPUT_DIR/${game_name}_cbasic.txt"
    if run_with_timeout bash -c "echo '$input' | '$CBASIC' '$game' 2>/dev/null" > "$my_out" 2>&1; then
        my_ok=true
    else
        my_ok=false
    fi

    # Compare outputs (using Python fuzzy comparison if available)
    if [ "$cbm_ok" = true ] && [ "$my_ok" = true ]; then
        if python3 "$SCRIPT_DIR/compare_output.py" "$cbm_out" "$my_out" > /dev/null 2>&1; then
            printf "  %-25s ${GREEN}PASS${NC}\n" "$game_name"
            pass=$((pass + 1))
        else
            printf "  %-25s ${RED}FAIL${NC} (output differs)\n" "$game_name"
            fail=$((fail + 1))
        fi
    elif [ "$cbm_ok" = false ] && [ "$my_ok" = false ]; then
        printf "  %-25s ${YELLOW}SKIP${NC} (both timeout/error)\n" "$game_name"
        skip=$((skip + 1))
    elif [ "$cbm_ok" = false ]; then
        printf "  %-25s ${YELLOW}SKIP${NC} (cbmbasic error)\n" "$game_name"
        skip=$((skip + 1))
    else
        printf "  %-25s ${RED}FAIL${NC} (CBASIC error)\n" "$game_name"
        fail=$((fail + 1))
    fi
done

echo ""
echo "=============================================="
echo "Summary"
echo "=============================================="
echo -e "  ${GREEN}PASS${NC}: $pass"
echo -e "  ${RED}FAIL${NC}: $fail"
echo -e "  ${YELLOW}SKIP${NC}: $skip (interactive or error)"
echo "  Total: $total"
echo ""

if [ $fail -eq 0 ] && [ $pass -gt 0 ]; then
    echo -e "${GREEN}All testable games pass!${NC}"
elif [ $fail -gt 0 ]; then
    echo -e "${YELLOW}Some games failed. Check $OUTPUT_DIR for details.${NC}"
fi

echo ""
echo "Output files saved to: $OUTPUT_DIR"
echo ""

# Exit with failure if any tests failed
[ $fail -eq 0 ]
