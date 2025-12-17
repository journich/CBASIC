/*
 * CBASIC - Input/Output Module
 *
 * Implements terminal I/O functions including PRINT output formatting,
 * INPUT handling, and console interaction.
 *
 * Based on 6502 implementation terminal handling routines.
 *
 * Copyright (c) 2025 Tim Buchalka
 * Licensed under the MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "basic.h"

/*
 * =============================================================================
 * OUTPUT FUNCTIONS
 * =============================================================================
 */

/*
 * basic_print_char - Print a single character
 *
 * Handles terminal position tracking and line wrapping.
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   c     - Character to print
 */
void basic_print_char(BasicState *state, char c)
{
    if (state == NULL) {
        putchar(c);
        return;
    }

    /* Handle special characters */
    if (c == '\n' || c == '\r') {
        putchar('\n');
        state->terminal_pos = 0;

        /* Output null characters for slow terminals (if configured) */
        for (int i = 0; i < state->null_count; i++) {
            putchar('\0');
        }
        return;
    }

    /* Handle backspace */
    if (c == '\b') {
        if (state->terminal_pos > 0) {
            putchar('\b');
            state->terminal_pos--;
        }
        return;
    }

    /* Regular character */
    putchar(c);
    state->terminal_pos++;

    /* Handle line wrap at terminal width */
    if (state->terminal_pos >= state->terminal_width) {
        putchar('\n');
        state->terminal_pos = 0;

        for (int i = 0; i < state->null_count; i++) {
            putchar('\0');
        }
    }

    /* Flush for immediate output */
    fflush(stdout);
}

/*
 * basic_print_string - Print a null-terminated string
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   s     - String to print
 */
void basic_print_string(BasicState *state, const char *s)
{
    if (s == NULL) {
        return;
    }

    while (*s != '\0') {
        basic_print_char(state, *s++);
    }
}

/*
 * basic_print_newline - Print a newline/carriage return
 *
 * Parameters:
 *   state - BASIC interpreter state
 */
void basic_print_newline(BasicState *state)
{
    basic_print_char(state, '\n');
}

/*
 * basic_print_tab - Move cursor to specified position
 *
 * If current position is past the tab position, moves to next line first.
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   pos   - Target column position (1-based)
 */
void basic_print_tab(BasicState *state, int pos)
{
    if (state == NULL) {
        return;
    }

    /* Convert to 0-based */
    pos--;
    if (pos < 0) pos = 0;

    /* If already past position, go to next line */
    if (state->terminal_pos >= pos) {
        basic_print_newline(state);
    }

    /* Print spaces to reach position */
    while (state->terminal_pos < pos) {
        basic_print_char(state, ' ');
    }
}

/*
 * basic_print_number - Print a numeric value
 *
 * Formats number according to Microsoft BASIC conventions:
 * - Leading space for positive numbers (instead of sign)
 * - Trailing space after number
 * - Integer formatting for whole numbers
 * - Scientific notation for very large/small numbers
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   n     - Number to print
 */
void basic_print_number(BasicState *state, double n)
{
    char buffer[32];

    /* Handle special cases */
    if (isnan(n)) {
        basic_print_string(state, " NAN ");
        return;
    }
    if (isinf(n)) {
        if (n > 0) {
            basic_print_string(state, " INF ");
        } else {
            basic_print_string(state, "-INF ");
        }
        return;
    }

    /* Leading space for positive, minus for negative */
    if (n >= 0) {
        basic_print_char(state, ' ');
    }

    /* Format the number */
    double abs_n = fabs(n);

    if (n == 0.0) {
        /* Zero */
        basic_print_char(state, '0');
    } else if (abs_n >= 1e10 || abs_n < 1e-9) {
        /* Use scientific notation for very large/small numbers */
        snprintf(buffer, sizeof(buffer), "%.8E", n);
        /* Trim trailing zeros before E */
        char *e_pos = strchr(buffer, 'E');
        if (e_pos != NULL) {
            char *trim = e_pos - 1;
            while (trim > buffer && *trim == '0') {
                trim--;
            }
            if (*trim == '.') {
                trim++;  /* Keep at least one digit after decimal */
            }
            trim++;
            memmove(trim, e_pos, strlen(e_pos) + 1);
        }
        basic_print_string(state, buffer);
    } else if (abs_n == floor(abs_n) && abs_n < 1e10) {
        /* Integer - print without decimal point */
        snprintf(buffer, sizeof(buffer), "%.0f", n);
        basic_print_string(state, buffer);
    } else {
        /* Regular decimal number */
        /* Use %g for automatic formatting, but limit precision */
        snprintf(buffer, sizeof(buffer), "%.9G", n);

        /* Remove trailing zeros after decimal point */
        char *dot = strchr(buffer, '.');
        if (dot != NULL && strchr(buffer, 'E') == NULL) {
            char *end = buffer + strlen(buffer) - 1;
            while (end > dot && *end == '0') {
                *end-- = '\0';
            }
            /* Remove decimal point if no digits after it */
            if (*end == '.') {
                *end = '\0';
            }
        }

        basic_print_string(state, buffer);
    }

    /* Trailing space */
    basic_print_char(state, ' ');
}

/*
 * =============================================================================
 * INPUT FUNCTIONS
 * =============================================================================
 */

/*
 * basic_input_line - Read a line of input from the user
 *
 * Displays an optional prompt and reads input until Enter is pressed.
 * Handles CTRL-C for break.
 *
 * Parameters:
 *   state  - BASIC interpreter state
 *   prompt - Prompt string to display (may be NULL)
 *   buffer - Buffer to receive input
 *   size   - Size of buffer
 *
 * Returns:
 *   true on successful input, false on break/error
 */
bool basic_input_line(BasicState *state, const char *prompt, char *buffer, size_t size)
{
    if (buffer == NULL || size == 0) {
        return false;
    }

    /* Display prompt */
    if (prompt != NULL && !state->suppress_prompt) {
        basic_print_string(state, prompt);
        fflush(stdout);
    }

    /* Read line */
    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return false;  /* EOF or error */
    }

    /* Remove trailing newline */
    size_t len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
        buffer[--len] = '\0';
    }

    /* Update terminal position */
    if (state != NULL) {
        state->terminal_pos = 0;  /* After newline */
    }

    return true;
}

/*
 * basic_input_value - Read and parse a single value
 *
 * Parameters:
 *   state    - BASIC interpreter state
 *   prompt   - Prompt to display
 *   result   - Receives the parsed value
 *   expected - Expected value type
 *
 * Returns:
 *   ERR_NONE on success, error code on failure
 */
ErrorCode basic_input_value(BasicState *state, const char *prompt, Value *result, ValueType expected)
{
    char buffer[256];

    if (!basic_input_line(state, prompt, buffer, sizeof(buffer))) {
        return ERR_BREAK;
    }

    if (expected == VAL_STRING) {
        result->type = VAL_STRING;
        result->v.string = basic_copy_string(state, buffer, strlen(buffer));
    } else {
        result->type = VAL_NUMBER;
        result->v.number = basic_fn_val(buffer);
    }

    return ERR_NONE;
}

/*
 * =============================================================================
 * PROGRAM LISTING OUTPUT
 * =============================================================================
 */

/*
 * basic_list - List program lines
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   start - Starting line number
 *   end   - Ending line number
 */
void basic_list(BasicState *state, int32_t start, int32_t end)
{
    if (state == NULL || state->program == NULL) {
        return;
    }

    ProgramLine *line = state->program;

    while (line != NULL) {
        if (line->line_number > end) {
            break;
        }

        if (line->line_number >= start) {
            /* Print line number */
            char num_str[16];
            snprintf(num_str, sizeof(num_str), "%d ", line->line_number);
            basic_print_string(state, num_str);

            /* Detokenize and print line content */
            char *text = basic_detokenize(line->text, line->length);
            if (text != NULL) {
                basic_print_string(state, text);
                free(text);
            }

            basic_print_newline(state);
        }

        line = line->next;
    }
}

/*
 * =============================================================================
 * STARTUP AND READY MESSAGES
 * =============================================================================
 */

/*
 * basic_print_ready - Print the READY prompt
 *
 * Parameters:
 *   state - BASIC interpreter state
 */
void basic_print_ready(BasicState *state)
{
    basic_print_string(state, "\nREADY.\n");
}

/*
 * basic_print_banner - Print startup banner
 *
 * Parameters:
 *   state - BASIC interpreter state
 */
void basic_print_banner(BasicState *state)
{
    basic_print_newline(state);
    basic_print_string(state, "**** CBASIC ****\n");
    basic_print_string(state, "MICROSOFT BASIC 1.1 COMPATIBLE\n");
    basic_print_string(state, "C17 PORT (C) 2025\n");

    /* Show memory available */
    int32_t free_mem = basic_fn_fre(state, 0);
    char mem_str[64];
    snprintf(mem_str, sizeof(mem_str), "%d BYTES FREE\n", (int)free_mem);
    basic_print_string(state, mem_str);
}
