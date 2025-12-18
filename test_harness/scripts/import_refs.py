#!/usr/bin/env python3
"""
Import reference outputs captured from a real Apple II or emulator.

Usage:
    1. Run the test program on AppleSoft BASIC
    2. Capture the output (copy/paste or serial capture)
    3. Save to a text file
    4. Run: import_refs.py --test t001_arithmetic --file captured_output.txt

This script normalizes the output for comparison.
"""

import os
import sys
import argparse
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
HARNESS_DIR = SCRIPT_DIR.parent
REF_DIR = HARNESS_DIR / "reference" / "outputs"


def normalize_applesoft_output(content: str) -> str:
    """Normalize AppleSoft output for comparison."""
    lines = []
    for line in content.split('\n'):
        # Convert CR to LF (Apple II uses CR)
        line = line.replace('\r', '')
        # Strip trailing whitespace
        line = line.rstrip()
        # Skip empty prompt lines
        if line == ']' or line == ']_':
            continue
        # Skip RUN command echo
        if line == 'RUN' or line == ']RUN':
            continue
        lines.append(line)

    # Remove trailing empty lines
    while lines and not lines[-1]:
        lines.pop()

    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(
        description='Import AppleSoft reference outputs')
    parser.add_argument('--test', required=True, help='Test name (e.g., t001_arithmetic)')
    parser.add_argument('--file', required=True, help='Captured output file')
    parser.add_argument('--force', action='store_true', help='Overwrite existing')
    args = parser.parse_args()

    REF_DIR.mkdir(parents=True, exist_ok=True)

    input_file = Path(args.file)
    if not input_file.exists():
        print(f"Error: Input file not found: {input_file}")
        sys.exit(1)

    output_file = REF_DIR / f"{args.test}.ref"
    if output_file.exists() and not args.force:
        print(f"Error: Reference already exists: {output_file}")
        print("Use --force to overwrite")
        sys.exit(1)

    with open(input_file, 'r') as f:
        content = f.read()

    normalized = normalize_applesoft_output(content)

    with open(output_file, 'w') as f:
        f.write(normalized)

    print(f"Created reference: {output_file}")
    print(f"Lines: {len(normalized.splitlines())}")


if __name__ == '__main__':
    main()
