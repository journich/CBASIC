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
 * basic_fn_rnd - Random number generator
 *
 * If x > 0: Return new random number in [0, 1)
 * If x = 0: Return last random number
 * If x < 0: Seed random generator with x, return new random
 *
 * The 6502 uses a simple linear congruential generator.
 * We match that behavior but use a better PRNG for quality.
 *
 * Matches 6502 implementation at line 6353
 */
double basic_fn_rnd(BasicState *state, double x)
{
    if (state == NULL) {
        return 0.0;
    }

    if (x < 0.0) {
        /* Seed the generator */
        state->rnd_seed = (uint32_t)(fabs(x) * 65536.0);
        if (state->rnd_seed == 0) {
            state->rnd_seed = 1;
        }
    }

    if (x != 0.0) {
        /* Generate new random number using LCG (matching 6502 style) */
        state->rnd_seed = state->rnd_seed * 1103515245 + 12345;
    }

    /* Convert to [0, 1) */
    return (double)(state->rnd_seed & 0x7FFFFFFF) / 2147483648.0;
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
