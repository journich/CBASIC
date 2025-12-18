#!/usr/bin/env python3
"""
Compare CBASIC output against MS BASIC reference output.
Handles fuzzy floating-point comparison and output normalization.
"""

import sys
import re
import argparse
from typing import List, Tuple, Optional

# Floating point comparison tolerance
# Use relative tolerance for larger numbers, absolute for near-zero
RELATIVE_TOLERANCE = 1e-6  # 6 significant digits
ABSOLUTE_TOLERANCE = 1e-8  # For numbers near zero


def normalize_line(line: str) -> str:
    """Normalize a line for comparison."""
    # Strip all whitespace from ends
    line = line.strip()
    # Normalize multiple spaces to single space
    line = re.sub(r' +', ' ', line)
    return line


def normalize_for_content(line: str) -> str:
    """Aggressively normalize for content comparison (ignore formatting)."""
    # Remove all spaces
    return re.sub(r'\s+', '', line)


def parse_number(s: str) -> Optional[float]:
    """Try to parse a string as a number."""
    # Handle BASIC number formats
    s = s.strip()
    try:
        return float(s)
    except ValueError:
        return None


def numbers_equal(a: float, b: float) -> bool:
    """Compare two floats with appropriate tolerance."""
    if a == b:
        return True

    # For very small numbers, use absolute tolerance
    if abs(a) < 1e-6 and abs(b) < 1e-6:
        return abs(a - b) < ABSOLUTE_TOLERANCE

    # For larger numbers, use relative tolerance
    max_val = max(abs(a), abs(b))
    return abs(a - b) / max_val < RELATIVE_TOLERANCE


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
            diff = abs(num1 - num2)
            return False, f"numeric: {token1} vs {token2} (diff: {diff:.2e})"

    return False, f"string: '{token1}' vs '{token2}'"


def extract_content(line: str) -> List[str]:
    """Extract content tokens from a line, being lenient about formatting."""
    # Split on whitespace and punctuation but keep meaningful tokens
    tokens = re.findall(r'[A-Za-z]+|[-+]?\d*\.?\d+(?:[Ee][-+]?\d+)?|[^\s\w]', line)
    return [t for t in tokens if t.strip()]


def compare_lines_strict(line1: str, line2: str) -> Tuple[bool, str]:
    """Strict line comparison (original method)."""
    norm1 = normalize_line(line1)
    norm2 = normalize_line(line2)

    if norm1 == norm2:
        return True, ""

    tokens1 = norm1.split()
    tokens2 = norm2.split()

    if len(tokens1) != len(tokens2):
        return False, f"token count: {len(tokens1)} vs {len(tokens2)}"

    for i, (t1, t2) in enumerate(zip(tokens1, tokens2)):
        equal, reason = compare_tokens(t1, t2)
        if not equal:
            return False, f"token {i}: {reason}"

    return True, ""


def compare_lines_lenient(line1: str, line2: str) -> Tuple[bool, str]:
    """Lenient line comparison (ignores formatting differences)."""
    # First try strict
    equal, reason = compare_lines_strict(line1, line2)
    if equal:
        return True, ""

    # Try content-based comparison (ignore all formatting)
    content1 = extract_content(line1)
    content2 = extract_content(line2)

    if len(content1) != len(content2):
        return False, f"content differs: {len(content1)} vs {len(content2)} elements"

    for i, (t1, t2) in enumerate(zip(content1, content2)):
        equal, reason = compare_tokens(t1, t2)
        if not equal:
            return False, reason

    # Content matches, just formatting differs
    return True, ""


def compare_outputs(cbasic_lines: List[str], ref_lines: List[str],
                   lenient: bool = True) -> Tuple[bool, List[str], List[str]]:
    """Compare full outputs, return (success, differences, warnings)."""
    differences = []
    warnings = []

    # Remove empty trailing lines
    while cbasic_lines and not cbasic_lines[-1].strip():
        cbasic_lines = cbasic_lines[:-1]
    while ref_lines and not ref_lines[-1].strip():
        ref_lines = ref_lines[:-1]

    max_lines = max(len(cbasic_lines), len(ref_lines))

    for i in range(max_lines):
        line1 = cbasic_lines[i] if i < len(cbasic_lines) else ""
        line2 = ref_lines[i] if i < len(ref_lines) else ""

        if i >= len(cbasic_lines):
            differences.append(f"Line {i+1}: CBASIC missing, REF: {line2}")
            continue
        if i >= len(ref_lines):
            differences.append(f"Line {i+1}: REF missing, CBASIC: {line1}")
            continue

        if lenient:
            equal, reason = compare_lines_lenient(line1, line2)
        else:
            equal, reason = compare_lines_strict(line1, line2)

        if not equal:
            differences.append(f"Line {i+1}: {reason}")
            differences.append(f"  CBASIC: {line1.rstrip()}")
            differences.append(f"  MSBASIC: {line2.rstrip()}")

    return len(differences) == 0, differences, warnings


def main():
    parser = argparse.ArgumentParser(description='Compare CBASIC and reference outputs')
    parser.add_argument('cbasic_output', help='CBASIC output file')
    parser.add_argument('reference_output', help='Reference output file')
    parser.add_argument('--no-skip-header', action='store_true',
                       help='Do not skip CBASIC header line')
    parser.add_argument('--strict', action='store_true',
                       help='Use strict comparison (fail on formatting diffs)')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Show detailed comparison')
    args = parser.parse_args()

    try:
        with open(args.cbasic_output, 'r') as f:
            cbasic_lines = [l.rstrip('\n\r') for l in f.readlines()]
        with open(args.reference_output, 'r') as f:
            ref_lines = [l.rstrip('\n\r') for l in f.readlines()]
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(2)

    success, differences, warnings = compare_outputs(
        cbasic_lines, ref_lines,
        lenient=not args.strict
    )

    if success:
        print("PASS: Outputs match")
        sys.exit(0)
    else:
        print("FAIL: Outputs differ")
        for diff in differences:
            print(f"  {diff}")
        sys.exit(1)


if __name__ == '__main__':
    main()
