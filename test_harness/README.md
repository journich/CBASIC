# CBASIC vs AppleSoft BASIC Compatibility Test Harness

This test harness compares CBASIC output against AppleSoft BASIC (the original Microsoft BASIC 6502 licensed to Apple) to ensure 100% compatibility.

## Quick Start

```bash
# Make scripts executable
chmod +x scripts/*.sh

# Generate reference outputs from CBASIC (for initial setup)
./scripts/run_tests.sh --generate-refs

# Run comparison tests
./scripts/run_tests.sh
```

## Directory Structure

```
test_harness/
├── scripts/
│   ├── run_tests.sh          # Main test runner
│   ├── run_cbasic.sh         # Run program through CBASIC
│   ├── run_applesoft.sh      # Run program through AppleSoft emulator
│   ├── compare_output.py     # Fuzzy output comparison
│   ├── generate_refs_applewin.py  # Auto-generate refs via emulator
│   └── import_refs.py        # Import manually captured refs
├── tests/
│   ├── t001_arithmetic.bas   # Test programs
│   ├── t002_comparison.bas
│   └── ...
├── reference/
│   └── outputs/              # AppleSoft reference outputs (.ref files)
├── output/
│   └── cbasic/              # CBASIC test outputs
└── README.md
```

## Creating Reference Outputs

Reference outputs must be captured from real AppleSoft BASIC. There are several methods:

### Method 1: Apple II Emulator (Recommended)

1. **Install an emulator**:
   ```bash
   # macOS - using Homebrew
   brew install --cask openemulator  # OpenEmulator
   # or
   brew install mame                  # MAME (includes Apple II)
   ```

2. **Get AppleSoft BASIC ROM** (legally):
   - Apple II ROM images are needed
   - Available from archive.org (search "Apple II ROM")
   - Or dump from original hardware

3. **Run test program and capture output**:
   - Boot Apple II with AppleSoft BASIC
   - Type in the test program (or load from disk)
   - Run and capture output
   - Save to `reference/outputs/t001_arithmetic.ref`

### Method 2: Online Apple II Emulator

1. Visit [Apple2js](https://www.scullinsteel.com/apple2/) or similar
2. Type in the test program
3. Copy/paste output
4. Use import script:
   ```bash
   python3 scripts/import_refs.py --test t001_arithmetic --file captured.txt
   ```

### Method 3: Real Apple II Hardware

1. Run test on real Apple II with serial capture
2. Import captured output:
   ```bash
   python3 scripts/import_refs.py --test t001_arithmetic --file serial_capture.txt
   ```

## Test Programs

| Test | Description |
|------|-------------|
| t001_arithmetic | Basic math: +, -, *, /, ^ |
| t002_comparison | Comparison operators: =, <>, <, >, <=, >= |
| t003_math_functions | ABS, INT, SGN, SQR, LOG, EXP, trig |
| t004_for_loops | FOR/NEXT with STEP, nested loops |
| t005_strings | LEN, LEFT$, RIGHT$, MID$, CHR$, ASC |
| t006_arrays | 1D, 2D, and string arrays |
| t007_gosub | GOSUB/RETURN, nested calls |
| t008_data_read | DATA, READ, RESTORE |
| t009_on_goto | ON...GOTO, ON...GOSUB |
| t010_logical | AND, OR, NOT operators |
| t011_precision | Floating point precision |
| t012_edge_cases | Boundary conditions |
| t013_print_format | PRINT formatting, TAB, SPC |
| t014_algorithms | Factorial, Fibonacci, primes |
| t015_trig_precision | Trigonometric accuracy |

## Comparison Features

The comparison script (`compare_output.py`) handles:

- **Fuzzy float comparison**: Numbers match if within 6 significant digits
- **Whitespace normalization**: Trailing spaces ignored
- **Line-by-line comparison**: Shows exact differences
- **Token-aware**: Compares numeric tokens numerically

## Running Tests

```bash
# Run all tests
./scripts/run_tests.sh

# Run specific test pattern
./scripts/run_tests.sh "t001*"

# Generate references from CBASIC output (for review)
./scripts/run_tests.sh --generate-refs

# Run single test manually
./scripts/run_cbasic.sh tests/t001_arithmetic.bas
```

## Understanding Results

```
Testing t001_arithmetic...              PASS
Testing t002_comparison...              PASS
Testing t003_math_functions...          FAIL
  Differences:
    Line 15: numeric mismatch: 1.41421356 vs 1.41421354 (diff: 2e-08)
```

- **PASS**: Output matches reference within tolerance
- **FAIL**: Output differs (shows line and difference)
- **SKIP**: No reference output available

## Known Differences

Some differences between CBASIC and AppleSoft are expected:

| Feature | CBASIC | AppleSoft | Impact |
|---------|--------|-----------|--------|
| Float precision | ~9 digits | ~9 digits | Should match |
| RND seed | Implementation-specific | Different | Avoid RND in tests |
| Error messages | May differ | Original | Compare carefully |
| Memory limits | System-dependent | 48K max | Test smaller cases |

## Adding New Tests

1. Create test file in `tests/`:
   ```basic
   10 REM TEST DESCRIPTION
   20 PRINT "OUTPUT"
   30 END
   ```

2. Run on AppleSoft and capture output
3. Save to `reference/outputs/testname.ref`
4. Run test: `./scripts/run_tests.sh testname.bas`

## Emulator-Specific Notes

### MAME Apple II

```bash
# Run MAME with Apple IIe
mame apple2e -window

# AppleSoft BASIC is in ROM, boots automatically
```

### linapple

```bash
# Install from source
git clone https://github.com/linappleii/linapple.git
cd linapple && make

# Run
./linapple
```

### OpenEmulator

- GUI-based, easier for manual testing
- Good for capturing output visually

## Troubleshooting

**Q: Tests fail with "no reference" error**
A: Generate reference outputs first. See "Creating Reference Outputs" above.

**Q: Floating point numbers don't match exactly**
A: This is expected. The comparison uses 6 significant digits tolerance.

**Q: Line endings differ**
A: The comparison normalizes line endings. If still failing, check for CR vs LF issues.

**Q: CBASIC header appears in output**
A: The test runner automatically strips the "CBASIC 1.1" header line.

## Contributing

1. Add test cases for untested features
2. Report compatibility issues
3. Submit reference outputs from verified AppleSoft runs
