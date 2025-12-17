/*
 * CBASIC - Main Interpreter Module
 *
 * Implements the core interpreter loop, program management, and REPL
 * (Read-Eval-Print Loop) for interactive mode.
 *
 * Based on 6502 implementation's main loop and line handling routines.
 *
 * Copyright (c) 2025 Tim Buchalka
 * Licensed under the MIT License
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "basic.h"

/* External declarations */
extern void basic_clear_variables(BasicState *state);
extern void basic_print_ready(BasicState *state);
extern void basic_print_banner(BasicState *state);
extern bool basic_check_break(BasicState *state);

/*
 * =============================================================================
 * INITIALIZATION AND CLEANUP
 * =============================================================================
 */

/*
 * basic_init - Initialize the BASIC interpreter
 *
 * Allocates and initializes all state required for the interpreter.
 *
 * Returns:
 *   Pointer to new interpreter state, or NULL on memory error
 */
BasicState *basic_init(void)
{
    BasicState *state = calloc(1, sizeof(BasicState));
    if (state == NULL) {
        return NULL;
    }

    /* Allocate string space (16KB default) */
    state->string_space_size = 16384;
    state->string_space = malloc(state->string_space_size);
    if (state->string_space == NULL) {
        free(state);
        return NULL;
    }
    state->string_ptr = state->string_space;

    /* Allocate runtime stack */
    state->stack_size = BASIC_STACK_SIZE;
    state->stack = calloc((size_t)state->stack_size, sizeof(StackEntry));
    if (state->stack == NULL) {
        free(state->string_space);
        free(state);
        return NULL;
    }
    state->stack_ptr = 0;

    /* Allocate simulated memory for PEEK/POKE (64KB) */
    state->memory_size = BASIC_MEMORY_SIZE;
    state->memory = calloc(state->memory_size, 1);
    if (state->memory == NULL) {
        free(state->stack);
        free(state->string_space);
        free(state);
        return NULL;
    }

    /* Initialize terminal settings */
    state->terminal_width = BASIC_TERMINAL_WIDTH;
    state->terminal_pos = 0;
    state->null_count = BASIC_NULL_COUNT;

    /* Initialize random number generator with default seed */
    state->rnd_seed = 12345;

    /* Initialize FAC and ARG */
    memset(&state->fac, 0, sizeof(state->fac));
    memset(&state->arg, 0, sizeof(state->arg));

    /* Other initializations */
    state->running = false;
    state->direct_mode = true;
    state->can_continue = false;
    state->suppress_prompt = false;
    state->trace_mode = false;

    return state;
}

/*
 * basic_cleanup - Free all interpreter resources
 *
 * Parameters:
 *   state - BASIC interpreter state to clean up
 */
void basic_cleanup(BasicState *state)
{
    if (state == NULL) {
        return;
    }

    /* Free program lines */
    ProgramLine *line = state->program;
    while (line != NULL) {
        ProgramLine *next = line->next;
        free(line->text);
        free(line);
        line = next;
    }

    /* Free variables */
    basic_clear_variables(state);

    /* Free allocated memory */
    free(state->memory);
    free(state->stack);
    free(state->string_space);
    free(state);
}

/*
 * basic_reset - Reset interpreter to initial state
 *
 * Clears program, variables, and resets execution state.
 *
 * Parameters:
 *   state - BASIC interpreter state
 */
void basic_reset(BasicState *state)
{
    if (state == NULL) {
        return;
    }

    basic_new(state);
    state->rnd_seed = 12345;
}

/*
 * =============================================================================
 * PROGRAM LINE MANAGEMENT
 * =============================================================================
 */

/*
 * basic_store_line - Store a line in the program
 *
 * If the line starts with a line number, it's added to the program.
 * Lines are kept sorted by line number.
 * If a line with the same number exists, it's replaced.
 * Empty lines delete existing lines.
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   line  - Line to store (may include line number)
 *
 * Returns:
 *   true if line was stored/executed, false on error
 */
bool basic_store_line(BasicState *state, const char *line)
{
    if (state == NULL || line == NULL) {
        return false;
    }

    /* Skip leading spaces */
    while (*line == ' ') {
        line++;
    }

    /* Check for line number */
    if (!BASIC_IS_DIGIT(*line)) {
        return false;  /* Not a numbered line */
    }

    /* Parse line number */
    int32_t line_num = 0;
    while (BASIC_IS_DIGIT(*line)) {
        line_num = line_num * 10 + (*line - '0');
        line++;
        if (line_num > BASIC_LINE_NUM_MAX) {
            return false;  /* Line number too large */
        }
    }

    /* Skip spaces after line number */
    while (*line == ' ') {
        line++;
    }

    /* If nothing after line number, delete the line */
    if (*line == '\0' || *line == '\n') {
        basic_delete_line(state, line_num);
        return true;
    }

    /* Tokenize the line content */
    size_t token_len;
    char *tokenized = basic_tokenize(line, &token_len);
    if (tokenized == NULL) {
        return false;
    }

    /* Delete any existing line with this number */
    basic_delete_line(state, line_num);

    /* Create new program line */
    ProgramLine *new_line = malloc(sizeof(ProgramLine));
    if (new_line == NULL) {
        free(tokenized);
        return false;
    }

    new_line->line_number = line_num;
    new_line->text = tokenized;
    new_line->length = token_len;

    /* Insert in sorted order */
    ProgramLine **pp = &state->program;
    while (*pp != NULL && (*pp)->line_number < line_num) {
        pp = &(*pp)->next;
    }
    new_line->next = *pp;
    *pp = new_line;

    /* Invalidate CONT */
    state->can_continue = false;

    return true;
}

/*
 * basic_delete_line - Delete a line from the program
 *
 * Parameters:
 *   state    - BASIC interpreter state
 *   line_num - Line number to delete
 */
void basic_delete_line(BasicState *state, int32_t line_num)
{
    if (state == NULL) {
        return;
    }

    ProgramLine **pp = &state->program;
    while (*pp != NULL) {
        if ((*pp)->line_number == line_num) {
            ProgramLine *to_delete = *pp;
            *pp = to_delete->next;
            free(to_delete->text);
            free(to_delete);
            return;
        }
        if ((*pp)->line_number > line_num) {
            return;  /* Not found */
        }
        pp = &(*pp)->next;
    }
}

/*
 * basic_find_line - Find a program line by number
 *
 * Parameters:
 *   state    - BASIC interpreter state
 *   line_num - Line number to find
 *
 * Returns:
 *   Pointer to line, or NULL if not found
 */
ProgramLine *basic_find_line(BasicState *state, int32_t line_num)
{
    if (state == NULL) {
        return NULL;
    }

    ProgramLine *line = state->program;
    while (line != NULL) {
        if (line->line_number == line_num) {
            return line;
        }
        if (line->line_number > line_num) {
            return NULL;  /* Not found */
        }
        line = line->next;
    }

    return NULL;
}

/*
 * basic_new - Clear program and variables (NEW command)
 *
 * Parameters:
 *   state - BASIC interpreter state
 */
void basic_new(BasicState *state)
{
    if (state == NULL) {
        return;
    }

    /* Free program lines */
    ProgramLine *line = state->program;
    while (line != NULL) {
        ProgramLine *next = line->next;
        free(line->text);
        free(line);
        line = next;
    }
    state->program = NULL;

    /* Clear variables */
    basic_clear_variables(state);

    /* Reset execution state */
    state->current_line = NULL;
    state->current_line_num = 0;
    state->text_ptr = NULL;
    state->running = false;
    state->can_continue = false;
    state->stack_ptr = 0;
}

/*
 * basic_goto_line - Jump to a program line
 *
 * Sets up execution to continue from the specified line.
 *
 * Parameters:
 *   state    - BASIC interpreter state
 *   line_num - Line number to go to
 */
void basic_goto_line(BasicState *state, int32_t line_num)
{
    if (state == NULL) {
        return;
    }

    ProgramLine *line = basic_find_line(state, line_num);
    if (line == NULL) {
        /* Try to find next line >= line_num */
        line = state->program;
        while (line != NULL && line->line_number < line_num) {
            line = line->next;
        }
    }

    state->current_line = line;
    if (line != NULL) {
        state->current_line_num = line->line_number;
        state->text_ptr = line->text;
    } else {
        state->current_line_num = 0;
        state->text_ptr = NULL;
    }
}

/*
 * =============================================================================
 * EXECUTION
 * =============================================================================
 */

/*
 * basic_execute_line - Execute a single line of BASIC
 *
 * Used for direct mode execution.
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   line  - Line to execute
 *
 * Returns:
 *   ERR_NONE on success, error code on failure
 */
ErrorCode basic_execute_line(BasicState *state, const char *line)
{
    if (state == NULL || line == NULL) {
        return ERR_SN;
    }

    /* Skip leading spaces */
    while (*line == ' ') {
        line++;
    }

    /* Check for line number (stored, not executed directly) */
    if (BASIC_IS_DIGIT(*line)) {
        if (basic_store_line(state, line)) {
            return ERR_NONE;
        }
        return ERR_SN;
    }

    /* Tokenize the line */
    size_t token_len;
    char *tokenized = basic_tokenize(line, &token_len);
    if (tokenized == NULL) {
        return ERR_SN;
    }

    /* Set up for direct mode execution */
    state->direct_mode = true;
    state->current_line = NULL;
    state->current_line_num = 0;
    state->text_ptr = tokenized;

    /* Execute statements */
    ErrorCode err = ERR_NONE;
    while (!BASIC_IS_EOL(*state->text_ptr) && err == ERR_NONE && !state->running) {
        err = basic_execute_statement(state);

        /* Skip to next statement */
        if (err == ERR_NONE) {
            basic_skip_spaces(state);
            if (*state->text_ptr == ':') {
                state->text_ptr++;
            }
        }
    }

    /* If RUN was executed, continue running */
    if (state->running) {
        free(tokenized);
        return basic_run(state);
    }

    free(tokenized);
    return err;
}

/*
 * basic_run - Run the current program
 *
 * Main program execution loop.
 *
 * Parameters:
 *   state - BASIC interpreter state
 *
 * Returns:
 *   ERR_NONE on normal termination, error code on error
 */
int basic_run(BasicState *state)
{
    if (state == NULL) {
        return ERR_SN;
    }

    /* Start from first line if not already set up */
    if (state->current_line == NULL && state->program != NULL) {
        state->current_line = state->program;
        state->current_line_num = state->current_line->line_number;
        state->text_ptr = state->current_line->text;
    }

    state->direct_mode = false;
    state->running = true;

    ErrorCode err = ERR_NONE;

    while (state->running && state->current_line != NULL) {
        /* Check for user break */
        if (basic_check_break(state)) {
            err = ERR_BREAK;
            break;
        }

        /* Trace mode output */
        if (state->trace_mode && state->current_line_num > 0) {
            char trace_str[16];
            snprintf(trace_str, sizeof(trace_str), "[%d]", state->current_line_num);
            basic_print_string(state, trace_str);
        }

        /* Execute current statement */
        err = basic_execute_statement(state);

        if (err != ERR_NONE) {
            break;
        }

        /* Move to next statement */
        basic_skip_spaces(state);

        if (*state->text_ptr == ':') {
            /* Statement separator - continue on same line */
            state->text_ptr++;
        } else if (BASIC_IS_EOL(*state->text_ptr)) {
            /* End of line - move to next line */
            if (state->current_line != NULL) {
                state->current_line = state->current_line->next;
                if (state->current_line != NULL) {
                    state->current_line_num = state->current_line->line_number;
                    state->text_ptr = state->current_line->text;
                } else {
                    /* End of program */
                    state->running = false;
                }
            }
        }
    }

    state->running = false;

    return err;
}

/*
 * =============================================================================
 * REPL (Read-Eval-Print Loop)
 * =============================================================================
 */

/*
 * basic_repl - Main REPL loop
 *
 * Runs an interactive BASIC session.
 *
 * Parameters:
 *   state - BASIC interpreter state
 */
void basic_repl(BasicState *state)
{
    if (state == NULL) {
        return;
    }

    /* Print startup banner */
    basic_print_banner(state);
    basic_print_ready(state);

    char line_buffer[BASIC_LINE_MAX + 1];

    while (1) {
        /* Read a line */
        if (!basic_input_line(state, "", line_buffer, sizeof(line_buffer))) {
            /* EOF or error */
            break;
        }

        /* Skip empty lines */
        if (line_buffer[0] == '\0') {
            continue;
        }

        /* Check for quit command (extension) */
        char *cmd = line_buffer;
        while (*cmd == ' ') cmd++;

        if (strcasecmp(cmd, "QUIT") == 0 || strcasecmp(cmd, "EXIT") == 0 ||
            strcasecmp(cmd, "BYE") == 0 || strcasecmp(cmd, "SYSTEM") == 0) {
            break;
        }

        /* Execute the line */
        ErrorCode err = basic_execute_line(state, line_buffer);

        /* Display error if any */
        if (err != ERR_NONE) {
            basic_error(state, err);
        }

        /* Print READY prompt after each command */
        if (!state->running) {
            basic_print_ready(state);
        }
    }

    basic_print_newline(state);
    basic_print_string(state, "BYE\n");
}

/*
 * =============================================================================
 * FLOATING POINT CONVERSION UTILITIES
 * =============================================================================
 * These routines convert between C doubles and the 6502 FAC format.
 * They're provided for completeness and potential future use.
 */

/*
 * basic_float_to_fac - Convert C double to FAC format
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   value - Value to convert
 */
void basic_float_to_fac(BasicState *state, double value)
{
    if (state == NULL) {
        return;
    }

    /* Store sign */
    if (value < 0) {
        state->fac.sign = -1;
        value = -value;
    } else {
        state->fac.sign = 0;
    }

    if (value == 0.0) {
        /* Zero is represented by zero exponent */
        state->fac.exponent = 0;
        memset(state->fac.mantissa, 0, sizeof(state->fac.mantissa));
        return;
    }

    /* Calculate exponent (base 2) */
    int exp = 0;
    while (value >= 1.0) {
        value /= 2.0;
        exp++;
    }
    while (value < 0.5) {
        value *= 2.0;
        exp--;
    }

    /* Store biased exponent (excess-128) */
    state->fac.exponent = (uint8_t)(exp + 128);

    /* Store mantissa (MSB first) */
    for (int i = 0; i < 4; i++) {
        value *= 256.0;
        state->fac.mantissa[i] = (uint8_t)value;
        value -= (uint8_t)value;
    }

    state->fac.overflow = 0;
}

/*
 * basic_fac_to_float - Convert FAC to C double
 *
 * Parameters:
 *   state - BASIC interpreter state
 *
 * Returns:
 *   C double value
 */
double basic_fac_to_float(BasicState *state)
{
    if (state == NULL || state->fac.exponent == 0) {
        return 0.0;
    }

    /* Reconstruct mantissa */
    double mantissa = 0.0;
    double factor = 1.0 / 256.0;

    for (int i = 0; i < 4; i++) {
        mantissa += state->fac.mantissa[i] * factor;
        factor /= 256.0;
    }

    /* Apply exponent */
    int exp = (int)state->fac.exponent - 128;
    double result = ldexp(mantissa, exp);

    /* Apply sign */
    if (state->fac.sign != 0) {
        result = -result;
    }

    return result;
}

/*
 * basic_normalize_fac - Normalize the FAC
 *
 * Ensures the mantissa is in proper normalized form.
 *
 * Parameters:
 *   state - BASIC interpreter state
 */
void basic_normalize_fac(BasicState *state)
{
    if (state == NULL) {
        return;
    }

    /* Check for zero */
    bool is_zero = true;
    for (int i = 0; i < 4; i++) {
        if (state->fac.mantissa[i] != 0) {
            is_zero = false;
            break;
        }
    }

    if (is_zero) {
        state->fac.exponent = 0;
        state->fac.sign = 0;
        return;
    }

    /* Shift left until MSB is set */
    while ((state->fac.mantissa[0] & 0x80) == 0 && state->fac.exponent > 0) {
        /* Shift mantissa left */
        uint8_t carry = 0;
        for (int i = 3; i >= 0; i--) {
            uint8_t new_carry = (state->fac.mantissa[i] & 0x80) ? 1 : 0;
            state->fac.mantissa[i] = (state->fac.mantissa[i] << 1) | carry;
            carry = new_carry;
        }
        state->fac.exponent--;
    }
}
