#!/usr/bin/env python3
"""
Generate AppleSoft BASIC reference outputs using AppleWin or linapple.

This script helps automate the creation of reference outputs by:
1. Converting BASIC programs to Apple II format
2. Creating automation scripts for the emulator
3. Capturing output

For manual reference generation, use the companion generate_refs_manual.py
"""

import os
import sys
import argparse
import subprocess
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
HARNESS_DIR = SCRIPT_DIR.parent
TESTS_DIR = HARNESS_DIR / "tests"
REF_DIR = HARNESS_DIR / "reference" / "outputs"


def convert_to_applesoft(bas_content: str) -> bytes:
    """Convert BASIC program to Apple II format (uppercase, CR line endings)."""
    # Convert to uppercase
    content = bas_content.upper()
    # Convert LF to CR (Apple II line ending)
    content = content.replace('\n', '\r')
    return content.encode('ascii', errors='replace')


def create_applesoft_script(bas_file: Path) -> str:
    """Create an AppleSoft automation script."""
    with open(bas_file, 'r') as f:
        content = f.read()

    # AppleSoft script: type in the program, run it, and save output
    script = convert_to_applesoft(content).decode('ascii')
    script += '\rRUN\r'
    return script


def generate_with_linapple(bas_file: Path, output_file: Path) -> bool:
    """Generate reference output using linapple."""
    # Check if linapple is available
    try:
        result = subprocess.run(['which', 'linapple'], capture_output=True)
        if result.returncode != 0:
            return False
    except FileNotFoundError:
        return False

    # linapple automation is complex - needs disk image creation
    # This is a placeholder for full implementation
    print(f"linapple found but automated capture not implemented yet")
    print(f"Please run {bas_file.name} manually and save output to {output_file}")
    return False


def main():
    parser = argparse.ArgumentParser(
        description='Generate AppleSoft BASIC reference outputs')
    parser.add_argument('--test', help='Specific test file to process')
    parser.add_argument('--all', action='store_true', help='Process all tests')
    parser.add_argument('--list', action='store_true', help='List tests needing refs')
    args = parser.parse_args()

    REF_DIR.mkdir(parents=True, exist_ok=True)

    if args.list:
        print("Tests without reference outputs:")
        for test_file in sorted(TESTS_DIR.glob("*.bas")):
            ref_file = REF_DIR / f"{test_file.stem}.ref"
            if not ref_file.exists():
                print(f"  {test_file.name}")
        return

    if args.test:
        test_files = [TESTS_DIR / args.test]
    elif args.all:
        test_files = sorted(TESTS_DIR.glob("*.bas"))
    else:
        parser.print_help()
        return

    for test_file in test_files:
        if not test_file.exists():
            print(f"Error: {test_file} not found")
            continue

        ref_file = REF_DIR / f"{test_file.stem}.ref"
        if ref_file.exists():
            print(f"Skipping {test_file.name} (reference exists)")
            continue

        print(f"Processing {test_file.name}...")

        # Try linapple
        if generate_with_linapple(test_file, ref_file):
            print(f"  Generated {ref_file.name}")
        else:
            print(f"  Manual generation needed for {test_file.name}")


if __name__ == '__main__':
    main()
