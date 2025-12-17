/*
 * CBASIC - Error Handling Module
 *
 * Implements error message display and error handling routines matching
 * the 6502 assembly implementation's error codes and messages.
 *
 * Based on 6502 implementation at lines 1251-1287 of m6502.asm
 *
 * Copyright (c) 2025 Tim Buchalka
 * Licensed under the MIT License
 */

#include <stdio.h>
#include "basic.h"

/*
 * =============================================================================
 * ERROR MESSAGE TABLES
 * =============================================================================
 * Two versions: short (2-character) codes matching original BASIC,
 * and long descriptive messages.
 */

/* Short error codes (original 6502 format) */
static const char *error_codes_short[] = {
    "OK",       /* ERR_NONE */
    "NF",       /* ERR_NF - NEXT without FOR */
    "SN",       /* ERR_SN - Syntax error */
    "RG",       /* ERR_RG - RETURN without GOSUB */
    "OD",       /* ERR_OD - Out of DATA */
    "FC",       /* ERR_FC - Illegal function call */
    "OV",       /* ERR_OV - Overflow */
    "OM",       /* ERR_OM - Out of memory */
    "US",       /* ERR_US - Undefined statement */
    "BS",       /* ERR_BS - Bad subscript */
    "DD",       /* ERR_DD - Redimensioned array */
    "/0",       /* ERR_DZ - Division by zero */
    "ID",       /* ERR_ID - Illegal direct */
    "TM",       /* ERR_TM - Type mismatch */
    "LS",       /* ERR_LS - String too long */
    "FD",       /* ERR_FD - File data error */
    "ST",       /* ERR_ST - String formula too complex */
    "CN",       /* ERR_CN - Can't continue */
    "UF",       /* ERR_UF - Undefined function */
    "BR",       /* ERR_BREAK - Break */
};

/* Long error messages */
static const char *error_codes_long[] = {
    "OK",                           /* ERR_NONE */
    "NEXT WITHOUT FOR",             /* ERR_NF */
    "SYNTAX ERROR",                 /* ERR_SN */
    "RETURN WITHOUT GOSUB",         /* ERR_RG */
    "OUT OF DATA",                  /* ERR_OD */
    "ILLEGAL QUANTITY",             /* ERR_FC */
    "OVERFLOW",                     /* ERR_OV */
    "OUT OF MEMORY",                /* ERR_OM */
    "UNDEF'D STATEMENT",            /* ERR_US */
    "BAD SUBSCRIPT",                /* ERR_BS */
    "REDIM'D ARRAY",                /* ERR_DD */
    "DIVISION BY ZERO",             /* ERR_DZ */
    "ILLEGAL DIRECT",               /* ERR_ID */
    "TYPE MISMATCH",                /* ERR_TM */
    "STRING TOO LONG",              /* ERR_LS */
    "FILE DATA ERROR",              /* ERR_FD */
    "FORMULA TOO COMPLEX",          /* ERR_ST */
    "CAN'T CONTINUE",               /* ERR_CN */
    "UNDEF'D FUNCTION",             /* ERR_UF */
    "BREAK",                        /* ERR_BREAK */
};

/*
 * =============================================================================
 * ERROR HANDLING FUNCTIONS
 * =============================================================================
 */

/*
 * basic_error_short - Get short (2-character) error code
 *
 * Returns the classic Microsoft BASIC 2-character error code.
 *
 * Parameters:
 *   code - Error code
 *
 * Returns:
 *   Pointer to static string with short error code
 */
const char *basic_error_short(ErrorCode code)
{
    if (code >= 0 && code < ERR_COUNT) {
        return error_codes_short[code];
    }
    return "??";
}

/*
 * basic_error_message - Get descriptive error message
 *
 * Returns a full descriptive error message.
 *
 * Parameters:
 *   code - Error code
 *
 * Returns:
 *   Pointer to static string with error message
 */
const char *basic_error_message(ErrorCode code)
{
#if FEATURE_LONG_ERRORS
    if (code >= 0 && code < ERR_COUNT) {
        return error_codes_long[code];
    }
    return "UNKNOWN ERROR";
#else
    /* Short error format: "?XX ERROR" */
    static char short_msg[16];
    snprintf(short_msg, sizeof(short_msg), "?%s ERROR", basic_error_short(code));
    return short_msg;
#endif
}

/*
 * basic_error - Report an error and stop execution
 *
 * Displays the error message, line number, and stops the program.
 * Sets up state so CONT can resume after STOP-style errors.
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   code  - Error code
 */
void basic_error(BasicState *state, ErrorCode code)
{
    if (state == NULL) {
        fprintf(stderr, "\n?%s ERROR\n", basic_error_short(code));
        return;
    }

    /* Stop execution */
    state->running = false;
    state->last_error = code;
    state->error_line = state->current_line_num;

    /* Most errors prevent CONT */
    if (code != ERR_BREAK && code != ERR_NONE) {
        state->can_continue = false;
    } else {
        /* BREAK allows CONT */
        state->can_continue = true;
        state->cont_line = state->current_line;
        state->cont_ptr = state->text_ptr;
    }

    /* Print newline first (in case we're mid-output) */
    basic_print_newline(state);

    /* Print error message */
    basic_print_char(state, '?');

#if FEATURE_LONG_ERRORS
    basic_print_string(state, basic_error_message(code));
#else
    basic_print_string(state, basic_error_short(code));
    basic_print_string(state, " ERROR");
#endif

    /* Print line number if in a program */
    if (state->current_line_num > 0) {
        basic_print_string(state, " IN ");
        char line_str[16];
        snprintf(line_str, sizeof(line_str), "%d", state->current_line_num);
        basic_print_string(state, line_str);
    }

    basic_print_newline(state);
}

/*
 * basic_check_break - Check for user break (CTRL-C)
 *
 * This function should be called periodically during program execution
 * to check if the user has requested a break.
 *
 * In a terminal environment, this typically checks for SIGINT.
 * For simplicity in this portable implementation, we don't implement
 * interrupt handling, but the function is provided for completeness.
 *
 * Parameters:
 *   state - BASIC interpreter state
 *
 * Returns:
 *   true if break was requested, false otherwise
 */
bool basic_check_break(BasicState *state)
{
    (void)state;  /* Unused in this simple implementation */

    /* In a full implementation, this would:
     * 1. Check a volatile flag set by SIGINT handler
     * 2. Check for keyboard input (platform-specific)
     *
     * For now, we just return false (no break).
     */
    return false;
}
