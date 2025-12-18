#!/bin/bash
# Run a BASIC program through AppleSoft BASIC emulator and capture output
# Usage: run_applesoft.sh <program.bas> [input_file]
#
# Requires: linapple or MAME with Apple II ROM
#
# This script uses linapple in headless/batch mode

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HARNESS_DIR="$(dirname "$SCRIPT_DIR")"
PROGRAM="$1"
INPUT_FILE="$2"

# Emulator configuration - adjust paths as needed
LINAPPLE="${LINAPPLE:-linapple}"
MAME="${MAME:-mame}"
APPLE2_DISK="${APPLE2_DISK:-$HARNESS_DIR/reference/applesoft.dsk}"

if [ ! -f "$PROGRAM" ]; then
    echo "Error: Program not found: $PROGRAM" >&2
    exit 1
fi

# Convert BASIC program to Apple II format
# AppleSoft uses CR (0x0D) line endings and uppercase
convert_to_applesoft() {
    local input="$1"
    # Convert to uppercase and CR line endings
    cat "$input" | tr 'a-z' 'A-Z' | tr '\n' '\r'
}

# Method 1: Using linapple with -batch mode (if available)
run_with_linapple() {
    local bas_file="$1"
    local temp_dir=$(mktemp -d)
    local converted="$temp_dir/program.bas"

    convert_to_applesoft "$bas_file" > "$converted"

    # linapple batch mode (check linapple documentation)
    # This is a placeholder - actual implementation depends on linapple version
    if command -v "$LINAPPLE" &> /dev/null; then
        echo "Running with linapple..."
        # linapple -batch "$converted" 2>&1
        echo "ERROR: linapple batch mode not yet configured"
        rm -rf "$temp_dir"
        return 1
    fi

    rm -rf "$temp_dir"
    return 1
}

# Method 2: Using MAME Apple II emulation
run_with_mame() {
    local bas_file="$1"

    if command -v "$MAME" &> /dev/null; then
        echo "MAME found but automated Apple II testing not yet configured"
        echo "See instructions in test_harness/README.md"
        return 1
    fi

    return 1
}

# Method 3: Use pre-recorded reference outputs
use_reference_output() {
    local bas_file="$1"
    local basename=$(basename "$bas_file" .bas)
    local ref_file="$HARNESS_DIR/reference/outputs/${basename}.ref"

    if [ -f "$ref_file" ]; then
        cat "$ref_file"
        return 0
    fi

    return 1
}

# Try methods in order
if use_reference_output "$PROGRAM"; then
    exit 0
elif run_with_linapple "$PROGRAM"; then
    exit 0
elif run_with_mame "$PROGRAM"; then
    exit 0
else
    echo "ERROR: No AppleSoft emulator available" >&2
    echo "Please either:" >&2
    echo "  1. Install linapple: brew install linapple (or build from source)" >&2
    echo "  2. Install MAME: brew install mame" >&2
    echo "  3. Create reference outputs manually in reference/outputs/" >&2
    exit 1
fi
