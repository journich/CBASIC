/*
 * CBASIC - Built-in Functions Module
 *
 * Implements all built-in mathematical and string functions matching
 * the 6502 assembly implementation.
 *
 * Based on 6502 implementation at lines 4112-6544 of m6502.asm
 *
 * Copyright (c) 2025 Tim Buchalka
 * Licensed under the MIT License
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include "basic.h"

/*
 * =============================================================================
 * MATHEMATICAL FUNCTIONS
 * =============================================================================
 * These functions match the behavior of the 6502 BASIC exactly.
 */

/*
 * basic_fn_sgn - Return sign of number
 *
 * Returns -1 if x < 0, 0 if x = 0, 1 if x > 0
 * Matches 6502 implementation at line 5572
 */
double basic_fn_sgn(double x)
{
    if (x < 0.0) return -1.0;
    if (x > 0.0) return 1.0;
    return 0.0;
}

/*
 * basic_fn_int - Greatest integer function
 *
 * Returns greatest integer less than or equal to x.
 * Note: For negative numbers, this rounds toward negative infinity,
 * not toward zero (e.g., INT(-1.5) = -2, not -1).
 *
 * Matches 6502 implementation at line 5673
 */
double basic_fn_int(double x)
{
    return floor(x);
}

/*
 * basic_fn_abs - Absolute value
 *
 * Returns |x|
 * Matches 6502 implementation at line 5594
 */
double basic_fn_abs(double x)
{
    return fabs(x);
}

/*
 * basic_fn_sqr - Square root
 *
 * Returns sqrt(x). Caller should check that x >= 0.
 * Matches 6502 implementation at line 6105
 */
double basic_fn_sqr(double x)
{
    return sqrt(x);
}

/*
 * basic_fn_log - Natural logarithm
 *
 * Returns ln(x). Caller should check that x > 0.
 * Matches 6502 implementation at line 5235
 */
double basic_fn_log(double x)
{
    return log(x);
}

/*
 * basic_fn_exp - Exponential function
 *
 * Returns e^x
 * Matches 6502 implementation at line 6246
 */
double basic_fn_exp(double x)
{
    return exp(x);
}

/*
 * basic_fn_sin - Sine function
 *
 * Returns sin(x) where x is in radians.
 * Matches 6502 implementation at line 6418
 */
double basic_fn_sin(double x)
{
    return sin(x);
}

/*
 * basic_fn_cos - Cosine function
 *
 * Returns cos(x) where x is in radians.
 * Matches 6502 implementation at line 6404
 */
double basic_fn_cos(double x)
{
    return cos(x);
}

/*
 * basic_fn_tan - Tangent function
 *
 * Returns tan(x) where x is in radians.
 * May overflow for values near pi/2.
 * Matches 6502 implementation at line 6446
 */
double basic_fn_tan(double x)
{
    return tan(x);
}

/*
 * basic_fn_atn - Arctangent function
 *
 * Returns arctan(x) in radians.
 * Matches 6502 implementation at line 6544
 */
double basic_fn_atn(double x)
{
    return atan(x);
}

/*
 * basic_fn_rnd - Random number generator (MS BASIC compatible)
 *
 * This implementation matches cbmbasic (Commodore 64 BASIC V2) which uses
 * the same Microsoft BASIC 6502 codebase.
 *
 * The algorithm uses the floating-point representation directly:
 *   1. Multiply previous seed by constant CONRND1
 *   2. Add constant CONRND2
 *   3. Swap high and low mantissa bytes
 *   4. Normalize to produce value in [0, 1)
 *
 * Key insight: After swapping bytes and forcing exponent to 128,
 * the NORMAL routine left-shifts the mantissa until bit 7 is set,
 * decrementing the exponent. This produces values across the full
 * [0, 1) range, not just [0.5, 1).
 */

/* MS BASIC 5-byte float to double */
static double msbas_to_double(const uint8_t *f)
{
    if (f[0] == 0) return 0.0;

    /* Mantissa with implied leading 1 */
    uint32_t m = ((uint32_t)(f[1] | 0x80) << 24) |
                 ((uint32_t)f[2] << 16) |
                 ((uint32_t)f[3] << 8) |
                 (uint32_t)f[4];

    double result = (double)m / 4294967296.0;  /* Divide by 2^32 */
    int exp = (int)f[0] - 128;
    return ldexp(result, exp);
}

/* Double to MS BASIC 5-byte float */
static void double_to_msbas(double d, uint8_t *f)
{
    if (d == 0.0) {
        memset(f, 0, 5);
        return;
    }

    int sign = (d < 0) ? 0x80 : 0;
    d = fabs(d);

    int exp;
    double mant = frexp(d, &exp);  /* d = mant * 2^exp, 0.5 <= mant < 1 */

    f[0] = (uint8_t)(exp + 128);
    uint32_t m = (uint32_t)(mant * 4294967296.0);

    f[1] = ((m >> 24) & 0x7F) | sign;
    f[2] = (m >> 16) & 0xFF;
    f[3] = (m >> 8) & 0xFF;
    f[4] = m & 0xFF;
}

/*
 * MS BASIC floating-point multiply using exact 6502 shift-and-add algorithm.
 * This replicates the FMULT/MLTPLY routine from the 6502 BASIC ROM exactly.
 *
 * The 6502 algorithm processes FAC mantissa byte-by-byte (FACOV, FACLO, FACMO,
 * FACMOH, FACHO), doing shift-and-add multiplication for each byte's 8 bits.
 *
 * CRITICAL: When a zero byte is processed (MULSHF path), the 6502 code checks
 * the carry state via SHIFTR. If carry is clear, it performs an extra bit-level
 * shift through SHFTR3. This affects the final result for certain input patterns
 * where zero bytes appear in the mantissa with specific carry states.
 */
static void msbas_fmult(uint8_t *fac, const uint8_t *arg, uint8_t *ov)
{
    if (fac[0] == 0 || arg[0] == 0) {
        memset(fac, 0, 5);
        *ov = 0;
        return;
    }

    /* MULDIV: Calculate new exponent = exp1 + exp2 - 128 */
    int new_exp = (int)fac[0] + (int)arg[0] - 128;
    if (new_exp <= 0) {
        memset(fac, 0, 5);
        *ov = 0;
        return;
    }
    if (new_exp > 255) new_exp = 255;

    /* Set up ARG with implied 1 bit */
    uint8_t argho = arg[1] | 0x80;
    uint8_t argmoh = arg[2];
    uint8_t argmo = arg[3];
    uint8_t arglo = arg[4];

    /* Set up FAC mantissa bytes with implied 1 bit */
    uint8_t facho = fac[1] | 0x80;
    uint8_t facmoh = fac[2];
    uint8_t facmo = fac[3];
    uint8_t faclo = fac[4];
    uint8_t facov = *ov;

    /* Clear result accumulator */
    uint8_t resho = 0;
    uint8_t resmoh = 0;
    uint8_t resmo = 0;
    uint8_t reslo = 0;
    uint8_t res_ov = 0;

    /* Track carry state across byte processing (MULDIV ends with CLC) */
    uint8_t carry = 0;

    /* Process each FAC byte through MLTPLY algorithm */
    /* Order: FACOV, FACLO, FACMO, FACMOH, then FACHO (via MLTPL1) */
    uint8_t fac_bytes[5] = {facov, faclo, facmo, facmoh, facho};

    for (int i = 0; i < 5; i++) {
        uint8_t mult_byte = fac_bytes[i];
        int is_facho = (i == 4);

        /* MLTPLY: JEQ MULSHF - if byte is 0, shift result right */
        /* But FACHO uses MLTPL1 which skips this check */
        if (!is_facho && mult_byte == 0) {
            /* MULSHF: shift result right by one byte */
            res_ov = reslo;
            reslo = resmo;
            resmo = resmoh;
            resmoh = resho;
            resho = 0;

            /* SHIFTR: Check if extra bit shift is needed based on carry state.
             * The 6502 code does: ADC #8, (BMI/BEQ loop), SBC #8, BCS SHFTRT.
             * When carry entering MULSHF is 0: ADC #8 gives 8, SBC #8 gives 255
             * with borrow (carry=0), so BCS doesn't branch and SHFTR3 executes.
             * When carry is 1: ADC #8 gives 9, SBC #8 gives 0 with no borrow
             * (carry=1), so BCS branches to SHFTRT and we're done.
             */
            int a = 8 + carry;  /* ADC #8 */
            int new_carry = (a >= 256) ? 1 : 0;
            a &= 0xFF;
            /* SBC #8 with borrow */
            int result = a - 8 - (1 - new_carry);
            if (result < 0) {
                new_carry = 0;  /* Borrow occurred */
            } else {
                new_carry = 1;  /* No borrow */
            }

            if (new_carry) {
                /* BCS SHFTRT: done, clear carry */
                carry = 0;
            } else {
                /* Fall through to SHFTR3: one extra bit-level shift.
                 * SHFTR3 does: ASL RESHO, (INC if carry), ROR RESHO x2,
                 * then ROR through RESMOH, RESMO, RESLO, and res_ov.
                 */
                uint8_t c;

                /* ASL RESHO: shift left, bit 7 to carry */
                c = (resho >> 7) & 1;
                resho = (resho << 1) & 0xFF;

                /* BCC SHFTR4, INC RESHO if carry was set */
                if (c) {
                    resho = (resho + 1) & 0xFF;
                }

                /* ROR RESHO (first time) */
                uint8_t nc = resho & 1;
                resho = (resho >> 1) | (c << 7);
                c = nc;

                /* ROR RESHO (second time) */
                nc = resho & 1;
                resho = (resho >> 1) | (c << 7);
                c = nc;

                /* ROR RESMOH */
                nc = resmoh & 1;
                resmoh = (resmoh >> 1) | (c << 7);
                c = nc;

                /* ROR RESMO */
                nc = resmo & 1;
                resmo = (resmo >> 1) | (c << 7);
                c = nc;

                /* ROR RESLO */
                nc = reslo & 1;
                reslo = (reslo >> 1) | (c << 7);
                c = nc;

                /* ROR res_ov (FACOV in 6502) */
                res_ov = (res_ov >> 1) | (c << 7);

                /* SHFTRT: CLC */
                carry = 0;
            }
            continue;
        }

        /* MLTPL1: LSR A, ORA #$80 - set up for 8-bit loop */
        uint8_t a = mult_byte;
        carry = a & 1;  /* LSR puts bit 0 into carry */
        a = (a >> 1) | 0x80;    /* LSR then ORA #$80 */

        /* MLTPL2: Process 8 bits via shift-and-add */
        for (;;) {
            uint8_t y = a;  /* TAY - save counter */

            /* BCC MLTPL3 - if carry clear, skip add */
            if (carry) {
                /* Add ARG to RES with carry propagation */
                uint16_t sum;
                sum = (uint16_t)reslo + arglo;
                reslo = sum & 0xFF;
                carry = sum >> 8;

                sum = (uint16_t)resmo + argmo + carry;
                resmo = sum & 0xFF;
                carry = sum >> 8;

                sum = (uint16_t)resmoh + argmoh + carry;
                resmoh = sum & 0xFF;
                carry = sum >> 8;

                sum = (uint16_t)resho + argho + carry;
                resho = sum & 0xFF;
                carry = sum >> 8;
            } else {
                carry = 0;
            }

            /* MLTPL3: ROR RESHO, ROR RESMOH, ROR RESMO, ROR RESLO, ROR FACOV */
            /* Rotate right through all bytes, carry from add goes into bit 7 */
            uint8_t new_carry;

            new_carry = resho & 1;
            resho = (resho >> 1) | (carry ? 0x80 : 0);
            carry = new_carry;

            new_carry = resmoh & 1;
            resmoh = (resmoh >> 1) | (carry ? 0x80 : 0);
            carry = new_carry;

            new_carry = resmo & 1;
            resmo = (resmo >> 1) | (carry ? 0x80 : 0);
            carry = new_carry;

            new_carry = reslo & 1;
            reslo = (reslo >> 1) | (carry ? 0x80 : 0);
            carry = new_carry;

            new_carry = res_ov & 1;
            res_ov = (res_ov >> 1) | (carry ? 0x80 : 0);

            /* TYA, LSR A - get next bit into carry, decrement counter */
            carry = y & 1;
            a = y >> 1;

            /* BNE MLTPL2 - loop while counter != 0 */
            if (a == 0) break;
        }
    }

    /* MOVFR + NORMAL: Move result to FAC and normalize */
    /* Normalize by shifting left until bit 7 of resho is set */
    while (new_exp > 0 && (resho & 0x80) == 0) {
        /* Shift all bytes left, bringing in bits from overflow */
        uint8_t c = (res_ov >> 7) & 1;
        res_ov <<= 1;

        uint8_t new_carry = (reslo >> 7) & 1;
        reslo = (reslo << 1) | c;
        c = new_carry;

        new_carry = (resmo >> 7) & 1;
        resmo = (resmo << 1) | c;
        c = new_carry;

        new_carry = (resmoh >> 7) & 1;
        resmoh = (resmoh << 1) | c;
        c = new_carry;

        resho = (resho << 1) | c;

        new_exp--;
    }

    if (new_exp <= 0) {
        memset(fac, 0, 5);
        *ov = 0;
        return;
    }

    /* Store result */
    fac[0] = (uint8_t)new_exp;
    fac[1] = resho & 0x7F;  /* Clear implied 1 bit for storage */
    fac[2] = resmoh;
    fac[3] = resmo;
    fac[4] = reslo;
    *ov = res_ov;
}

/*
 * MS BASIC floating-point add using integer arithmetic
 * Uses extended 40-bit precision with overflow byte for accuracy
 */
static void msbas_fadd(uint8_t *fac, const uint8_t *arg, uint8_t *ov)
{
    if (arg[0] == 0) return;
    if (fac[0] == 0) {
        memcpy(fac, arg, 5);
        *ov = 0;
        return;
    }

    /* Get mantissas with implied leading 1, extended with ov byte */
    uint64_t m1 = (((uint64_t)(fac[1] | 0x80) << 24) |
                   ((uint64_t)fac[2] << 16) |
                   ((uint64_t)fac[3] << 8) |
                   (uint64_t)fac[4]) << 8 | *ov;
    uint64_t m2 = (((uint64_t)(arg[1] | 0x80) << 24) |
                   ((uint64_t)arg[2] << 16) |
                   ((uint64_t)arg[3] << 8) |
                   (uint64_t)arg[4]) << 8;

    int exp1 = fac[0];
    int exp2 = arg[0];
    int exp_diff = exp1 - exp2;

    /* Align exponents - use 64-bit threshold to match 6502 behavior */
    if (exp_diff > 64) return;  /* arg is negligible */
    if (exp_diff < -64) {
        memcpy(fac, arg, 5);
        *ov = 0;
        return;
    }

    if (exp_diff > 0) {
        m2 >>= exp_diff;
    } else if (exp_diff < 0) {
        m1 >>= -exp_diff;
        exp1 = exp2;
    }

    /* Add mantissas */
    uint64_t sum = m1 + m2;

    /* Handle overflow */
    if (sum >= (1ULL << 40)) {
        sum >>= 1;
        exp1++;
    }

    /* Extract result */
    uint32_t result = (uint32_t)(sum >> 8);
    *ov = (uint8_t)(sum & 0xFF);

    /* Normalize if needed */
    while (exp1 > 0 && (result & 0x80000000) == 0 && result != 0) {
        result = (result << 1) | (*ov >> 7);
        *ov <<= 1;
        exp1--;
    }

    if (exp1 <= 0 || exp1 > 255 || result == 0) {
        memset(fac, 0, 5);
        *ov = 0;
        return;
    }

    /* Store result */
    fac[0] = (uint8_t)exp1;
    fac[1] = (result >> 24) & 0x7F;
    fac[2] = (result >> 16) & 0xFF;
    fac[3] = (result >> 8) & 0xFF;
    fac[4] = result & 0xFF;
}

double basic_fn_rnd(BasicState *state, double x)
{
    /*
     * Constants from C64 BASIC ROM (same as cbmbasic uses).
     * CONRND1 = multiplier (approximately 11879546)
     * CONRND2 = addend (approximately 3.927677739e-8)
     */
    static const uint8_t CONRND1[5] = {0x98, 0x35, 0x44, 0x7A, 0x00};
    static const uint8_t CONRND2[5] = {0x68, 0x28, 0xB1, 0x46, 0x00};

    if (state == NULL) {
        return 0.0;
    }

    uint8_t fac[5];   /* FAC: exp, ho, moh, mo, lo */
    uint8_t ov = 0;   /* Overflow byte for extended precision */

    if (x < 0.0) {
        /* Negative: seed from argument, then swap and normalize */
        double_to_msbas(fabs(x), fac);
        ov = 0;
        /* Jump to byte swap/normalize (skip multiply/add) */
        goto rnd_swap;
    }

    if (x == 0.0) {
        /* Zero: return current seed without changing it */
        return msbas_to_double(state->rnd_seed);
    }

    /* Positive: multiply and add using MS BASIC integer math */
    memcpy(fac, state->rnd_seed, 5);
    ov = 0;
    msbas_fmult(fac, CONRND1, &ov);
    msbas_fadd(fac, CONRND2, &ov);

rnd_swap:
    /* Set implied 1 bit before swap (FAC always has implied 1 visible in 6502) */
    fac[1] |= 0x80;

    /* Swap FACHO (fac[1]) <-> FACLO (fac[4]) */
    {
        uint8_t tmp = fac[1];
        fac[1] = fac[4];
        fac[4] = tmp;
    }
    /* Swap FACMOH (fac[2]) <-> FACMO (fac[3]) - C64/Commodore style */
    {
        uint8_t tmp = fac[2];
        fac[2] = fac[3];
        fac[3] = tmp;
    }

    /* STRNEX: Set up for normalization.
     * Note: Do NOT clear bit 7 of fac[1] here - after swap, fac[1] contains
     * the old low byte which has 8 data bits. The "clear sign" in 6502
     * refers to the separate FACSIGN register, not to bit 7 of the mantissa. */
    ov = fac[0];     /* FACOV = FACEXP (save exponent for normalization) */
    fac[0] = 0x80;   /* FACEXP = 128 (so result will be < 1) */

    /* NORMAL: Shift mantissa left until bit 7 of ho is set */
    while (fac[0] > 0 && (fac[1] & 0x80) == 0) {
        /* Shift all bytes left, bringing in ov bit by bit */
        uint8_t carry = (ov >> 7) & 1;  /* Top bit of ov */
        ov <<= 1;  /* Shift ov left */

        /* Shift mantissa left with carry from ov */
        uint8_t new_lo = (fac[4] << 1) | carry;
        carry = (fac[4] >> 7) & 1;

        uint8_t new_mo = (fac[3] << 1) | carry;
        carry = (fac[3] >> 7) & 1;

        uint8_t new_moh = (fac[2] << 1) | carry;
        carry = (fac[2] >> 7) & 1;

        uint8_t new_ho = (fac[1] << 1) | carry;

        fac[4] = new_lo;
        fac[3] = new_mo;
        fac[2] = new_moh;
        fac[1] = new_ho;

        fac[0]--;  /* Decrement exponent */
    }

    /* Handle underflow - if exp went to 0, result is 0 */
    if (fac[0] == 0) {
        memset(fac, 0, 5);
        ov = 0;
    }

    /* ROUND: If ov bit 7 is set, round up the mantissa (MOVMF behavior) */
    if (ov & 0x80) {
        /* Increment low byte, propagate carry */
        fac[4]++;
        if (fac[4] == 0) {
            fac[3]++;
            if (fac[3] == 0) {
                fac[2]++;
                if (fac[2] == 0) {
                    fac[1]++;
                    if (fac[1] == 0) {
                        /* Overflow - shift right and increment exponent */
                        fac[1] = 0x80;
                        fac[0]++;
                    }
                }
            }
        }
    }

    /* Store result back to RNDX (clear implied 1 bit like MOVMF does) */
    fac[1] &= 0x7F;
    memcpy(state->rnd_seed, fac, 5);

    /* Return the result as a double */
    return msbas_to_double(state->rnd_seed);
}

/*
 * basic_fn_fre - Free memory function
 *
 * Returns amount of free memory available.
 * The argument is ignored in string BASIC (evaluates to force garbage collection).
 *
 * Matches 6502 implementation at line 4113
 */
int32_t basic_fn_fre(BasicState *state, double x)
{
    (void)x;  /* Argument ignored */

    if (state == NULL) {
        return 0;
    }

    /* Perform garbage collection if string argument */
    basic_garbage_collect(state);

    /* Return approximate free memory */
    size_t used = (size_t)(state->string_ptr - state->string_space);
    size_t free_mem = state->string_space_size - used;

    /* Also account for stack space */
    size_t stack_free = (size_t)(state->stack_size - state->stack_ptr) * sizeof(StackEntry);

    return (int32_t)(free_mem + stack_free);
}

/*
 * basic_fn_pos - Cursor position function
 *
 * Returns current horizontal cursor position (1-based).
 * The argument is ignored.
 *
 * Matches 6502 implementation at line 4130
 */
int32_t basic_fn_pos(BasicState *state, double x)
{
    (void)x;  /* Argument ignored */

    if (state == NULL) {
        return 1;
    }

    return state->terminal_pos + 1;  /* Convert to 1-based */
}

/*
 * basic_fn_peek - Read byte from memory
 *
 * Returns the byte value at the specified memory address.
 * In this C implementation, we simulate a memory space.
 *
 * Matches 6502 implementation at line 4808
 */
int32_t basic_fn_peek(BasicState *state, int32_t addr)
{
    if (state == NULL || state->memory == NULL) {
        return 0;
    }

    if (addr < 0 || (size_t)addr >= state->memory_size) {
        return 0;  /* Out of range */
    }

    return state->memory[addr];
}

/*
 * =============================================================================
 * STRING FUNCTIONS
 * =============================================================================
 */

/*
 * basic_fn_str - Convert number to string
 *
 * Converts a numeric value to its string representation.
 * Positive numbers have a leading space.
 *
 * Matches 6502 implementation at line 4242
 */
StringDescriptor basic_fn_str(BasicState *state, double x)
{
    StringDescriptor result = {0, NULL, false};
    char buffer[32];

    /* Format number - leading space for positive */
    int len;
    if (x >= 0.0) {
        buffer[0] = ' ';
        len = 1;
    } else {
        len = 0;
    }

    /* Check if it's an integer */
    if (x == floor(x) && fabs(x) < 1e10) {
        len += snprintf(buffer + len, sizeof(buffer) - (size_t)len, "%.0f", x);
    } else {
        /* Use %g for general format, similar to original BASIC */
        len += snprintf(buffer + len, sizeof(buffer) - (size_t)len, "%.9g", x);
    }

    /* Allocate and copy */
    char *str = basic_alloc_string(state, (size_t)len);
    if (str != NULL) {
        memcpy(str, buffer, (size_t)len);
        result.data = str;
        result.length = (uint8_t)len;
        result.is_temp = true;
    }

    return result;
}

/*
 * basic_fn_val - Convert string to number
 *
 * Parses a string as a number. Leading spaces are skipped.
 * Returns 0 if no valid number found.
 *
 * Matches 6502 implementation at line 4763
 */
double basic_fn_val(const char *s)
{
    if (s == NULL) {
        return 0.0;
    }

    /* Skip leading spaces */
    while (*s == ' ') {
        s++;
    }

    /* Parse number */
    char *endptr;
    double result = strtod(s, &endptr);

    /* If no conversion happened, return 0 */
    if (endptr == s) {
        return 0.0;
    }

    return result;
}

/*
 * basic_fn_len - String length
 *
 * Returns the length of a string.
 *
 * Matches 6502 implementation at line 4730
 */
int32_t basic_fn_len(StringDescriptor *s)
{
    if (s == NULL) {
        return 0;
    }
    return s->length;
}

/*
 * basic_fn_asc - ASCII value of first character
 *
 * Returns the ASCII value of the first character of a string.
 * Caller should verify string is not empty.
 *
 * Matches 6502 implementation at line 4741
 */
int32_t basic_fn_asc(StringDescriptor *s)
{
    if (s == NULL || s->length == 0) {
        return 0;
    }
    return (unsigned char)s->data[0];
}

/*
 * basic_fn_chr - Character from ASCII value
 *
 * Returns a single-character string for the given ASCII value.
 *
 * Matches 6502 implementation at line 4632
 */
StringDescriptor basic_fn_chr(BasicState *state, int32_t x)
{
    StringDescriptor result = {0, NULL, false};

    if (x < 0 || x > 255) {
        return result;  /* Invalid, caller should check */
    }

    char *str = basic_alloc_string(state, 1);
    if (str != NULL) {
        str[0] = (char)x;
        result.data = str;
        result.length = 1;
        result.is_temp = true;
    }

    return result;
}

/*
 * basic_fn_left - Left substring
 *
 * Returns the leftmost n characters of a string.
 *
 * Matches 6502 implementation at line 4648
 */
StringDescriptor basic_fn_left(BasicState *state, StringDescriptor *s, int32_t n)
{
    StringDescriptor result = {0, "", false};

    if (s == NULL || n <= 0) {
        return result;
    }

    /* Clamp length */
    if (n > s->length) {
        n = s->length;
    }

    char *str = basic_alloc_string(state, (size_t)n);
    if (str != NULL) {
        memcpy(str, s->data, (size_t)n);
        result.data = str;
        result.length = (uint8_t)n;
        result.is_temp = true;
    }

    return result;
}

/*
 * basic_fn_right - Right substring
 *
 * Returns the rightmost n characters of a string.
 *
 * Matches 6502 implementation at line 4672
 */
StringDescriptor basic_fn_right(BasicState *state, StringDescriptor *s, int32_t n)
{
    StringDescriptor result = {0, "", false};

    if (s == NULL || n <= 0) {
        return result;
    }

    /* Clamp length */
    if (n > s->length) {
        n = s->length;
    }

    int32_t start = s->length - n;

    char *str = basic_alloc_string(state, (size_t)n);
    if (str != NULL) {
        memcpy(str, s->data + start, (size_t)n);
        result.data = str;
        result.length = (uint8_t)n;
        result.is_temp = true;
    }

    return result;
}

/*
 * basic_fn_mid - Middle substring
 *
 * Returns a substring starting at position 'start' (1-based) with
 * length 'len'. If len is omitted or extends past end, returns to end.
 *
 * Matches 6502 implementation at line 4684
 */
StringDescriptor basic_fn_mid(BasicState *state, StringDescriptor *s, int32_t start, int32_t len)
{
    StringDescriptor result = {0, "", false};

    if (s == NULL || start < 1) {
        return result;
    }

    /* Convert to 0-based index */
    start--;

    /* Check bounds */
    if (start >= s->length) {
        return result;  /* Empty string */
    }

    /* Calculate actual length */
    int32_t max_len = s->length - start;
    if (len > max_len) {
        len = max_len;
    }
    if (len <= 0) {
        return result;
    }

    char *str = basic_alloc_string(state, (size_t)len);
    if (str != NULL) {
        memcpy(str, s->data + start, (size_t)len);
        result.data = str;
        result.length = (uint8_t)len;
        result.is_temp = true;
    }

    return result;
}

/*
 * =============================================================================
 * STRING MEMORY MANAGEMENT
 * =============================================================================
 */

/*
 * basic_alloc_string - Allocate space for a string
 *
 * Allocates space in the string storage area.
 * Strings grow downward from the top of memory (like the 6502 version).
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   len   - Length of string to allocate
 *
 * Returns:
 *   Pointer to allocated space, or NULL if out of memory
 */
char *basic_alloc_string(BasicState *state, size_t len)
{
    if (state == NULL || len == 0) {
        return NULL;
    }

    if (len > BASIC_STRING_MAX) {
        return NULL;
    }

    /* Check if we have space */
    size_t used = (size_t)(state->string_ptr - state->string_space);
    if (used + len > state->string_space_size) {
        /* Try garbage collection */
        basic_garbage_collect(state);

        used = (size_t)(state->string_ptr - state->string_space);
        if (used + len > state->string_space_size) {
            return NULL;  /* Still not enough */
        }
    }

    /* Allocate from string space */
    char *result = state->string_ptr;
    state->string_ptr += len;

    return result;
}

/*
 * basic_free_string - Free a string (mark for garbage collection)
 *
 * In this implementation, we don't actually free individual strings.
 * They'll be reclaimed during garbage collection.
 */
void basic_free_string(BasicState *state, StringDescriptor *s)
{
    (void)state;
    (void)s;
    /* No-op - garbage collection handles this */
}

/*
 * basic_garbage_collect - Compact string space
 *
 * This is a simplified garbage collection. In the 6502 version,
 * this is a complex process that compacts strings toward the top
 * of memory. Here we just reset if no variables reference strings.
 *
 * A full implementation would scan all variables and arrays,
 * marking strings in use, then compacting the string space.
 */
void basic_garbage_collect(BasicState *state)
{
    if (state == NULL) {
        return;
    }

    /* For simplicity in this implementation, we don't do full GC.
     * A production version should:
     * 1. Mark all strings referenced by variables and arrays
     * 2. Compact the string space, moving referenced strings
     * 3. Update all string pointers
     *
     * For now, we just note that GC was requested.
     * The string space will be reclaimed when CLEAR or NEW is executed.
     */
}

/*
 * basic_copy_string - Create a copy of a string
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   s     - Source string
 *   len   - Length to copy
 *
 * Returns:
 *   New StringDescriptor with copied data
 */
StringDescriptor basic_copy_string(BasicState *state, const char *s, size_t len)
{
    StringDescriptor result = {0, "", false};

    if (s == NULL || len == 0) {
        return result;
    }

    if (len > BASIC_STRING_MAX) {
        len = BASIC_STRING_MAX;
    }

    char *str = basic_alloc_string(state, len);
    if (str != NULL) {
        memcpy(str, s, len);
        result.data = str;
        result.length = (uint8_t)len;
        result.is_temp = true;
    }

    return result;
}
