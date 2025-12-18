#!/bin/bash
# Run a BASIC program through Microsoft BASIC (cbmbasic) and capture output
# Usage: run_msbasic.sh <program.bas>
#
# cbmbasic is Commodore 64 BASIC V2, which is the same Microsoft BASIC
# 6502 codebase as AppleSoft BASIC (just different I/O routines)

PROGRAM="$1"

if [ ! -f "$PROGRAM" ]; then
    echo "Error: Program not found: $PROGRAM" >&2
    exit 1
fi

# Create a temp file with the program and RUN command
TEMP_FILE=$(mktemp)
trap "rm -f $TEMP_FILE" EXIT

# Convert program to uppercase (MS BASIC convention) and add RUN
cat "$PROGRAM" | tr 'a-z' 'A-Z' > "$TEMP_FILE"
echo "" >> "$TEMP_FILE"
echo "RUN" >> "$TEMP_FILE"

# Run through cbmbasic and filter output
# - Remove carriage returns and control characters
# - Extract only the program output (between first READY. and last READY.)
cbmbasic < "$TEMP_FILE" 2>&1 | \
    tr -d '\r' | \
    tr -d '\035' | \
    sed -n '/^READY\.$/,/^READY\.$/p' | \
    sed '1d;$d'
