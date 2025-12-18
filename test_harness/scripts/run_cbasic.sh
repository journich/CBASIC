#!/bin/bash
# Run a BASIC program through CBASIC and capture output
# Usage: run_cbasic.sh <program.bas> [input_file]

CBASIC_BIN="${CBASIC_BIN:-/Users/tb/dev/CBASIC/bin/cbasic}"
PROGRAM="$1"
INPUT_FILE="$2"

if [ ! -f "$PROGRAM" ]; then
    echo "Error: Program not found: $PROGRAM" >&2
    exit 1
fi

if [ -n "$INPUT_FILE" ] && [ -f "$INPUT_FILE" ]; then
    "$CBASIC_BIN" "$PROGRAM" < "$INPUT_FILE" 2>&1
else
    "$CBASIC_BIN" "$PROGRAM" < /dev/null 2>&1
fi

# Remove the CBASIC header line for comparison
# | tail -n +2
