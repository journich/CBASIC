#!/usr/bin/env python3
"""
Compare CBASIC output against AppleSoft BASIC reference output.
Handles fuzzy floating-point comparison and output normalization.
"""

import sys
import re
import argparse
from typing import List, Tuple, Optional

# Floating point comparison tolerance (6 significant digits)
FLOAT_TOLERANCE = 1e-6
SIGNIFICANT_DIGITS = 6


def normalize_line(line: str) -> str:
    """Normalize a line for comparison."""
    # Strip trailing whitespace
    line = line.rstrip()
    # Normalize multiple spaces to single space
    line = re.sub(r' +', ' ', line)
    # Strip leading whitespace (optional - some BASIC outputs have significant leading spaces)
    # line = line.lstrip()
    return line


def parse_number(s: str) -> Optional[float]:
    """Try to parse a string as a number."""
    try:
        return float(s)
    except ValueError:
        return None


def numbers_equal(a: float, b: float, tolerance: float = FLOAT_TOLERANCE) -> bool:
    """Compare two floats with tolerance."""
    if a == b:
        return True
    if a == 0 or b == 0:
        return abs(a - b) < tolerance
    # Relative comparison
    return abs(a - b) / max(abs(a), abs(b)) < tolerance


def compare_tokens(token1: str, token2: str) -> Tuple[bool, str]:
    """Compare two tokens, handling numbers specially."""
    if token1 == token2:
        return True, ""

    # Try numeric comparison
    num1 = parse_number(token1)
    num2 = parse_number(token2)

    if num1 is not None and num2 is not None:
        if numbers_equal(num1, num2):
            return True, ""
        else:
            return False, f"numeric mismatch: {token1} vs {token2} (diff: {abs(num1-num2)})"

    return False, f"string mismatch: '{token1}' vs '{token2}'"


def tokenize_line(line: str) -> List[str]:
    """Split line into tokens (words and numbers)."""
    # Split on whitespace but keep structure
    return line.split()


def compare_lines(line1: str, line2: str) -> Tuple[bool, str]:
    """Compare two lines with fuzzy float matching."""
    norm1 = normalize_line(line1)
    norm2 = normalize_line(line2)

    # Exact match after normalization
    if norm1 == norm2:
        return True, ""

    # Token-by-token comparison
    tokens1 = tokenize_line(norm1)
    tokens2 = tokenize_line(norm2)

    if len(tokens1) != len(tokens2):
        return False, f"different token count: {len(tokens1)} vs {len(tokens2)}"

    for i, (t1, t2) in enumerate(zip(tokens1, tokens2)):
        equal, reason = compare_tokens(t1, t2)
        if not equal:
            return False, f"token {i}: {reason}"

    return True, ""


def compare_outputs(cbasic_lines: List[str], ref_lines: List[str],
                   skip_header: bool = True) -> Tuple[bool, List[str]]:
    """Compare full outputs, return (success, list of differences)."""
    differences = []

    # Optionally skip CBASIC header line
    if skip_header and cbasic_lines and cbasic_lines[0].startswith("CBASIC"):
        cbasic_lines = cbasic_lines[1:]

    # Remove empty trailing lines
    while cbasic_lines and not cbasic_lines[-1].strip():
        cbasic_lines = cbasic_lines[:-1]
    while ref_lines and not ref_lines[-1].strip():
        ref_lines = ref_lines[:-1]

    max_lines = max(len(cbasic_lines), len(ref_lines))

    for i in range(max_lines):
        line1 = cbasic_lines[i] if i < len(cbasic_lines) else "<missing>"
        line2 = ref_lines[i] if i < len(ref_lines) else "<missing>"

        if line1 == "<missing>" or line2 == "<missing>":
            differences.append(f"Line {i+1}: {line1} | {line2}")
            continue

        equal, reason = compare_lines(line1, line2)
        if not equal:
            differences.append(f"Line {i+1}: {reason}")
            differences.append(f"  CBASIC: {line1}")
            differences.append(f"  REF:    {line2}")

    return len(differences) == 0, differences


def main():
    parser = argparse.ArgumentParser(description='Compare CBASIC and reference outputs')
    parser.add_argument('cbasic_output', help='CBASIC output file')
    parser.add_argument('reference_output', help='Reference output file')
    parser.add_argument('--no-skip-header', action='store_true',
                       help='Do not skip CBASIC header line')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Show detailed comparison')
    args = parser.parse_args()

    try:
        with open(args.cbasic_output, 'r') as f:
            cbasic_lines = f.readlines()
        with open(args.reference_output, 'r') as f:
            ref_lines = f.readlines()
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(2)

    # Strip newlines
    cbasic_lines = [l.rstrip('\n\r') for l in cbasic_lines]
    ref_lines = [l.rstrip('\n\r') for l in ref_lines]

    success, differences = compare_outputs(
        cbasic_lines, ref_lines,
        skip_header=not args.no_skip_header
    )

    if success:
        print("PASS: Outputs match")
        sys.exit(0)
    else:
        print("FAIL: Outputs differ")
        if args.verbose or True:  # Always show differences for now
            for diff in differences:
                print(f"  {diff}")
        sys.exit(1)


if __name__ == '__main__':
    main()
