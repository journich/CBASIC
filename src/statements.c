/*
 * CBASIC - Statement Execution Module
 *
 * Implements all BASIC statement handlers (FOR, NEXT, GOTO, GOSUB, etc.)
 * matching the 6502 assembly implementation.
 *
 * Based on 6502 implementation at lines 2064-3180 of m6502.asm
 *
 * Copyright (c) 2025 Tim Buchalka
 * Licensed under the MIT License
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "basic.h"

/* External declarations from variables.c */
extern void basic_clear_variables(BasicState *state);

/*
 * Forward declarations
 */
static ErrorCode stmt_end(BasicState *state);
static ErrorCode stmt_for(BasicState *state);
static ErrorCode stmt_next(BasicState *state);
static ErrorCode stmt_data(BasicState *state);
static ErrorCode stmt_input(BasicState *state);
static ErrorCode stmt_dim(BasicState *state);
static ErrorCode stmt_read(BasicState *state);
static ErrorCode stmt_let(BasicState *state);
static ErrorCode stmt_goto(BasicState *state);
static ErrorCode stmt_run(BasicState *state);
static ErrorCode stmt_if(BasicState *state);
static ErrorCode stmt_restore(BasicState *state);
static ErrorCode stmt_gosub(BasicState *state);
static ErrorCode stmt_return(BasicState *state);
static ErrorCode stmt_rem(BasicState *state);
static ErrorCode stmt_stop(BasicState *state);
static ErrorCode stmt_on(BasicState *state);
static ErrorCode stmt_wait(BasicState *state);
static ErrorCode stmt_poke(BasicState *state);
static ErrorCode stmt_print(BasicState *state);
static ErrorCode stmt_cont(BasicState *state);
static ErrorCode stmt_list(BasicState *state);
static ErrorCode stmt_clear(BasicState *state);
static ErrorCode stmt_get(BasicState *state);
static ErrorCode stmt_new(BasicState *state);
static ErrorCode stmt_def(BasicState *state);
static ErrorCode stmt_null(BasicState *state);

/* Helper: Parse a variable reference for assignment */
static ErrorCode parse_lvalue(BasicState *state, Variable **var_out, Array **arr_out,
                              int *indices, int *num_indices);

/*
 * =============================================================================
 * STATEMENT DISPATCHER
 * =============================================================================
 */

/*
 * basic_execute_statement - Execute the current statement
 *
 * This is the main statement dispatcher, equivalent to GONE in the 6502 code.
 *
 * Parameters:
 *   state - BASIC interpreter state
 *
 * Returns:
 *   ERR_NONE on success, error code on failure
 */
ErrorCode basic_execute_statement(BasicState *state)
{
    if (state == NULL || state->text_ptr == NULL) {
        return ERR_SN;
    }

    basic_skip_spaces(state);

    /* Check for end of statement/line */
    if (BASIC_IS_EOS(basic_peek_char(state))) {
        return ERR_NONE;
    }

    /* Get first character/token */
    unsigned char c = (unsigned char)*state->text_ptr;

    /* Check for statement token */
    if (BASIC_IS_TOKEN(c)) {
        Token tok = (Token)c;
        state->text_ptr++;

        switch (tok) {
            case TOK_END:     return stmt_end(state);
            case TOK_FOR:     return stmt_for(state);
            case TOK_NEXT:    return stmt_next(state);
            case TOK_DATA:    return stmt_data(state);
            case TOK_INPUT:   return stmt_input(state);
            case TOK_DIM:     return stmt_dim(state);
            case TOK_READ:    return stmt_read(state);
            case TOK_LET:     return stmt_let(state);
            case TOK_GOTO:    return stmt_goto(state);
            case TOK_RUN:     return stmt_run(state);
            case TOK_IF:      return stmt_if(state);
            case TOK_RESTORE: return stmt_restore(state);
            case TOK_GOSUB:   return stmt_gosub(state);
            case TOK_RETURN:  return stmt_return(state);
            case TOK_REM:     return stmt_rem(state);
            case TOK_STOP:    return stmt_stop(state);
            case TOK_ON:      return stmt_on(state);
            case TOK_NULL:    return stmt_null(state);
            case TOK_WAIT:    return stmt_wait(state);
            case TOK_DEF:     return stmt_def(state);
            case TOK_POKE:    return stmt_poke(state);
            case TOK_PRINT:   return stmt_print(state);
            case TOK_CONT:    return stmt_cont(state);
            case TOK_LIST:    return stmt_list(state);
            case TOK_CLEAR:   return stmt_clear(state);
            case TOK_GET:     return stmt_get(state);
            case TOK_NEW:     return stmt_new(state);
            case TOK_LOAD:
            case TOK_SAVE:
            case TOK_VERIFY:
                /* Not implemented in this version */
                return ERR_SN;
            default:
                /* Unknown token */
                return ERR_SN;
        }
    }

    /* Check for implicit LET (variable assignment) */
    if (BASIC_IS_LETTER(c)) {
        return stmt_let(state);
    }

    /* Check for "?" as shorthand for PRINT */
    if (c == '?') {
        state->text_ptr++;
        return stmt_print(state);
    }

    return ERR_SN;  /* Syntax error */
}

/*
 * =============================================================================
 * STATEMENT IMPLEMENTATIONS
 * =============================================================================
 */

/*
 * stmt_end - END statement
 *
 * Terminates program execution.
 */
static ErrorCode stmt_end(BasicState *state)
{
    state->running = false;
    state->can_continue = false;
    return ERR_NONE;
}

/*
 * stmt_for - FOR statement
 *
 * Syntax: FOR var = start TO limit [STEP increment]
 *
 * Creates a FOR loop entry on the stack.
 */
static ErrorCode stmt_for(BasicState *state)
{
    basic_skip_spaces(state);

    /* Get loop variable name */
    if (!BASIC_IS_LETTER(basic_peek_char(state))) {
        return ERR_SN;
    }

    char var_name[32];
    int name_pos = 0;
    while (BASIC_IS_LETTER(basic_peek_char(state)) || BASIC_IS_DIGIT(basic_peek_char(state))) {
        if (name_pos < 31) {
            var_name[name_pos++] = BASIC_TOUPPER(*state->text_ptr);
        }
        state->text_ptr++;
    }
    var_name[name_pos] = '\0';

    /* Must be followed by = */
    basic_skip_spaces(state);
    if (*state->text_ptr != '=' && *state->text_ptr != (char)TOK_EQ) {
        return ERR_SN;
    }
    state->text_ptr++;

    /* Get start value */
    double start;
    ErrorCode err = basic_eval_numeric(state, &start);
    if (err != ERR_NONE) return err;

    /* Must be followed by TO */
    basic_skip_spaces(state);
    if (*state->text_ptr != (char)TOK_TO) {
        /* Check for non-tokenized TO */
        if (BASIC_TOUPPER(state->text_ptr[0]) != 'T' ||
            BASIC_TOUPPER(state->text_ptr[1]) != 'O') {
            return ERR_SN;
        }
        state->text_ptr += 2;
    } else {
        state->text_ptr++;
    }

    /* Get limit value */
    double limit;
    err = basic_eval_numeric(state, &limit);
    if (err != ERR_NONE) return err;

    /* Optional STEP */
    double step = 1.0;
    basic_skip_spaces(state);
    if (*state->text_ptr == (char)TOK_STEP) {
        state->text_ptr++;
        err = basic_eval_numeric(state, &step);
        if (err != ERR_NONE) return err;
    } else if (BASIC_TOUPPER(state->text_ptr[0]) == 'S' &&
               BASIC_TOUPPER(state->text_ptr[1]) == 'T' &&
               BASIC_TOUPPER(state->text_ptr[2]) == 'E' &&
               BASIC_TOUPPER(state->text_ptr[3]) == 'P') {
        state->text_ptr += 4;
        err = basic_eval_numeric(state, &step);
        if (err != ERR_NONE) return err;
    }

    /* Set loop variable */
    Value val;
    val.type = VAL_NUMBER;
    val.v.number = start;
    err = basic_set_variable(state, var_name, &val);
    if (err != ERR_NONE) return err;

    /* Remove any existing FOR with same variable */
    Variable *loop_var = basic_get_variable(state, var_name);
    for (int i = state->stack_ptr - 1; i >= 0; i--) {
        if (state->stack[i].type == STACK_FOR &&
            state->stack[i].data.for_loop.loop_var == loop_var) {
            /* Remove this and all entries above it */
            state->stack_ptr = i;
            break;
        }
    }

    /* Push FOR entry */
    if (state->stack_ptr >= state->stack_size) {
        return ERR_OM;  /* Stack overflow */
    }

    StackEntry *entry = &state->stack[state->stack_ptr++];
    entry->type = STACK_FOR;
    entry->data.for_loop.loop_var = loop_var;
    entry->data.for_loop.step = step;
    entry->data.for_loop.limit = limit;
    entry->data.for_loop.line_number = state->current_line_num;
    entry->data.for_loop.text_ptr = state->text_ptr;
    entry->data.for_loop.line = state->current_line;

    return ERR_NONE;
}

/*
 * stmt_next - NEXT statement
 *
 * Syntax: NEXT [var]
 *
 * Continues or terminates a FOR loop.
 */
static ErrorCode stmt_next(BasicState *state)
{
    Variable *var = NULL;

    basic_skip_spaces(state);

    /* Optional variable name */
    if (BASIC_IS_LETTER(basic_peek_char(state))) {
        char var_name[32];
        int name_pos = 0;
        while (BASIC_IS_LETTER(basic_peek_char(state)) || BASIC_IS_DIGIT(basic_peek_char(state))) {
            if (name_pos < 31) {
                var_name[name_pos++] = BASIC_TOUPPER(*state->text_ptr);
            }
            state->text_ptr++;
        }
        var_name[name_pos] = '\0';
        var = basic_get_variable(state, var_name);
    }

    /* Find matching FOR on stack */
    int for_idx = -1;
    for (int i = state->stack_ptr - 1; i >= 0; i--) {
        if (state->stack[i].type == STACK_FOR) {
            if (var == NULL || state->stack[i].data.for_loop.loop_var == var) {
                for_idx = i;
                break;
            }
        }
    }

    if (for_idx < 0) {
        return ERR_NF;  /* NEXT without FOR */
    }

    ForLoopEntry *loop = &state->stack[for_idx].data.for_loop;

    /* Increment loop variable */
    double current = loop->loop_var->value.v.number;
    current += loop->step;
    loop->loop_var->value.v.number = current;

    /* Check if loop should continue */
    bool done;
    if (loop->step >= 0) {
        done = (current > loop->limit);
    } else {
        done = (current < loop->limit);
    }

    if (done) {
        /* Remove FOR entry and any nested entries */
        state->stack_ptr = for_idx;
    } else {
        /* Continue loop - go back to start */
        state->current_line = loop->line;
        state->current_line_num = loop->line_number;
        state->text_ptr = loop->text_ptr;
    }

    return ERR_NONE;
}

/*
 * stmt_data - DATA statement
 *
 * DATA statements are not executed directly; they're parsed by READ.
 * Just skip to end of statement.
 */
static ErrorCode stmt_data(BasicState *state)
{
    /* Skip to end of statement */
    while (!BASIC_IS_EOS(*state->text_ptr)) {
        state->text_ptr++;
    }
    return ERR_NONE;
}

/*
 * stmt_input - INPUT statement
 *
 * Syntax: INPUT ["prompt";] var [,var...]
 *
 * Reads values from the user.
 */
static ErrorCode stmt_input(BasicState *state)
{
    basic_skip_spaces(state);

    /* Optional prompt string */
    char prompt[256] = "? ";

    if (*state->text_ptr == '"') {
        state->text_ptr++;
        int plen = 0;
        while (*state->text_ptr != '"' && *state->text_ptr != '\0' && plen < 254) {
            prompt[plen++] = *state->text_ptr++;
        }
        prompt[plen] = '\0';

        if (*state->text_ptr == '"') {
            state->text_ptr++;
        }

        basic_skip_spaces(state);
        if (*state->text_ptr == ';') {
            state->text_ptr++;
        } else if (*state->text_ptr == ',') {
            state->text_ptr++;
            /* Add "? " after comma */
            if (plen < 253) {
                prompt[plen++] = '?';
                prompt[plen++] = ' ';
                prompt[plen] = '\0';
            }
        }
    }

    /* Read variables */
    do {
        basic_skip_spaces(state);

        /* Parse variable (lvalue) */
        Variable *var = NULL;
        Array *arr = NULL;
        int indices[BASIC_ARRAY_DIMS];
        int num_indices = 0;

        ErrorCode err = parse_lvalue(state, &var, &arr, indices, &num_indices);
        if (err != ERR_NONE) return err;

        /* Determine type */
        bool is_string = (var != NULL && var->name.is_string) ||
                        (arr != NULL && arr->name.is_string);

        /* Prompt and read */
        char input_buf[256];
        if (!basic_input_line(state, prompt, input_buf, sizeof(input_buf))) {
            return ERR_BREAK;  /* User break */
        }

        /* Parse value */
        Value val;
        if (is_string) {
            val.type = VAL_STRING;
            size_t len = strlen(input_buf);
            val.v.string = basic_copy_string(state, input_buf, len);
        } else {
            val.type = VAL_NUMBER;
            val.v.number = basic_fn_val(input_buf);
        }

        /* Store value */
        if (arr != NULL) {
            err = basic_set_array_element(state, arr, indices, &val);
        } else {
            var->value = val;
        }
        if (err != ERR_NONE) return err;

        /* Check for more variables */
        basic_skip_spaces(state);
        if (*state->text_ptr == ',') {
            state->text_ptr++;
            strcpy(prompt, "?? ");  /* Prompt for additional inputs */
        } else {
            break;
        }
    } while (1);

    return ERR_NONE;
}

/*
 * stmt_dim - DIM statement
 *
 * Syntax: DIM var(dims) [,var(dims)...]
 */
static ErrorCode stmt_dim(BasicState *state)
{
    do {
        basic_skip_spaces(state);

        /* Get array name */
        if (!BASIC_IS_LETTER(basic_peek_char(state))) {
            return ERR_SN;
        }

        char name[32];
        int name_pos = 0;
        while (BASIC_IS_LETTER(basic_peek_char(state)) || BASIC_IS_DIGIT(basic_peek_char(state))) {
            if (name_pos < 31) {
                name[name_pos++] = BASIC_TOUPPER(*state->text_ptr);
            }
            state->text_ptr++;
        }

        /* Check for type suffix */
        if (*state->text_ptr == '$' || *state->text_ptr == '%') {
            if (name_pos < 31) {
                name[name_pos++] = *state->text_ptr;
            }
            state->text_ptr++;
        }
        name[name_pos] = '\0';

        /* Expect ( */
        basic_skip_spaces(state);
        if (*state->text_ptr != '(') {
            return ERR_SN;
        }
        state->text_ptr++;

        /* Get dimensions */
        int dims[BASIC_ARRAY_DIMS];
        int num_dims = 0;

        do {
            double d;
            ErrorCode err = basic_eval_numeric(state, &d);
            if (err != ERR_NONE) return err;

            if (d < 0 || d > 32767) {
                return ERR_FC;
            }

            dims[num_dims++] = (int)d;

            basic_skip_spaces(state);
            if (*state->text_ptr == ',') {
                state->text_ptr++;
                if (num_dims >= BASIC_ARRAY_DIMS) {
                    return ERR_FC;  /* Too many dimensions */
                }
            } else {
                break;
            }
        } while (1);

        /* Expect ) */
        if (*state->text_ptr != ')') {
            return ERR_SN;
        }
        state->text_ptr++;

        /* Create array */
        ErrorCode err = basic_dim_array(state, name, dims, num_dims);
        if (err != ERR_NONE) return err;

        /* Check for more arrays */
        basic_skip_spaces(state);
        if (*state->text_ptr == ',') {
            state->text_ptr++;
        } else {
            break;
        }
    } while (1);

    return ERR_NONE;
}

/*
 * stmt_read - READ statement
 *
 * Syntax: READ var [,var...]
 *
 * Reads values from DATA statements.
 */
static ErrorCode stmt_read(BasicState *state)
{
    do {
        basic_skip_spaces(state);

        /* Parse variable (lvalue) */
        Variable *var = NULL;
        Array *arr = NULL;
        int indices[BASIC_ARRAY_DIMS];
        int num_indices = 0;

        ErrorCode err = parse_lvalue(state, &var, &arr, indices, &num_indices);
        if (err != ERR_NONE) return err;

        /* Find next DATA item */
    find_data:
        /* First check if we have more data at current position */
        if (state->data_ptr.position != NULL) {
            /* Skip spaces and commas from previous read */
            while (*state->data_ptr.position == ' ' || *state->data_ptr.position == ',') {
                state->data_ptr.position++;
            }

            /* Check if there's data here */
            if (*state->data_ptr.position != '\0' && *state->data_ptr.position != ':') {
                goto have_data;  /* Good, continue reading */
            }

            /* End of current DATA - need to find next DATA statement */
            state->data_ptr.line = state->data_ptr.line->next;
            state->data_ptr.position = NULL;
        }

        /* If no current DATA position, start from beginning */
        if (state->data_ptr.line == NULL) {
            state->data_ptr.line = state->program;
        }

        /* Find DATA token in current or subsequent lines */
        while (state->data_ptr.line != NULL) {
            const char *pos = state->data_ptr.line->text;

            /* Scan for DATA token */
            while (*pos != '\0') {
                if ((unsigned char)*pos == TOK_DATA) {
                    state->data_ptr.position = (char *)(pos + 1);
                    goto find_data;  /* Check if there's data after token */
                }
                /* Skip over strings */
                if (*pos == '"') {
                    pos++;
                    while (*pos != '"' && *pos != '\0') {
                        pos++;
                    }
                    if (*pos == '"') {
                        pos++;
                    }
                } else {
                    pos++;
                }
            }

            /* Move to next line */
            state->data_ptr.line = state->data_ptr.line->next;
        }

        /* No more DATA */
        return ERR_OD;

    have_data:
        ;  /* Empty statement to satisfy C17 label requirement */

        /* Determine type */
        bool is_string = (var != NULL && var->name.is_string) ||
                        (arr != NULL && arr->name.is_string);

        Value val;
        if (is_string) {
            /* Read string value */
            char str_buf[256];
            int str_len = 0;

            if (*state->data_ptr.position == '"') {
                /* Quoted string */
                state->data_ptr.position++;
                while (*state->data_ptr.position != '"' &&
                       *state->data_ptr.position != '\0' && str_len < 255) {
                    str_buf[str_len++] = *state->data_ptr.position++;
                }
                if (*state->data_ptr.position == '"') {
                    state->data_ptr.position++;
                }
            } else {
                /* Unquoted - read until comma or end */
                while (*state->data_ptr.position != ',' &&
                       *state->data_ptr.position != ':' &&
                       *state->data_ptr.position != '\0' && str_len < 255) {
                    str_buf[str_len++] = *state->data_ptr.position++;
                }
                /* Trim trailing spaces */
                while (str_len > 0 && str_buf[str_len - 1] == ' ') {
                    str_len--;
                }
            }

            val.type = VAL_STRING;
            val.v.string = basic_copy_string(state, str_buf, (size_t)str_len);
        } else {
            /* Read numeric value */
            char num_buf[64];
            int num_len = 0;

            /* Skip leading spaces */
            while (*state->data_ptr.position == ' ') {
                state->data_ptr.position++;
            }

            /* Read number */
            while ((*state->data_ptr.position >= '0' && *state->data_ptr.position <= '9') ||
                   *state->data_ptr.position == '.' ||
                   *state->data_ptr.position == '-' ||
                   *state->data_ptr.position == '+' ||
                   *state->data_ptr.position == 'E' ||
                   *state->data_ptr.position == 'e') {
                if (num_len < 63) {
                    num_buf[num_len++] = *state->data_ptr.position;
                }
                state->data_ptr.position++;
            }
            num_buf[num_len] = '\0';

            val.type = VAL_NUMBER;
            val.v.number = basic_fn_val(num_buf);
        }

        /* Store value */
        if (arr != NULL) {
            err = basic_set_array_element(state, arr, indices, &val);
        } else {
            var->value = val;
        }
        if (err != ERR_NONE) return err;

        /* Skip comma/spaces after value */
        while (*state->data_ptr.position == ' ') {
            state->data_ptr.position++;
        }
        if (*state->data_ptr.position == ',') {
            state->data_ptr.position++;
        }

        /* Check for more variables */
        basic_skip_spaces(state);
        if (*state->text_ptr == ',') {
            state->text_ptr++;
        } else {
            break;
        }
    } while (1);

    return ERR_NONE;
}

/*
 * stmt_let - LET statement (also handles implicit assignment)
 *
 * Syntax: [LET] var = expression
 */
static ErrorCode stmt_let(BasicState *state)
{
    basic_skip_spaces(state);

    /* Parse variable (lvalue) */
    Variable *var = NULL;
    Array *arr = NULL;
    int indices[BASIC_ARRAY_DIMS];
    int num_indices = 0;

    ErrorCode err = parse_lvalue(state, &var, &arr, indices, &num_indices);
    if (err != ERR_NONE) return err;

    /* Expect = */
    basic_skip_spaces(state);
    if (*state->text_ptr != '=' && *state->text_ptr != (char)TOK_EQ) {
        return ERR_SN;
    }
    state->text_ptr++;

    /* Evaluate expression */
    Value val;
    err = basic_evaluate(state, &val);
    if (err != ERR_NONE) return err;

    /* Store value */
    if (arr != NULL) {
        return basic_set_array_element(state, arr, indices, &val);
    } else {
        /* Type check */
        if (var->name.is_string && val.type != VAL_STRING) {
            return ERR_TM;
        }
        if (!var->name.is_string && val.type == VAL_STRING) {
            return ERR_TM;
        }

        if (val.type == VAL_STRING) {
            char *str = basic_alloc_string(state, val.v.string.length);
            if (str == NULL && val.v.string.length > 0) {
                return ERR_OM;
            }
            if (val.v.string.length > 0) {
                memcpy(str, val.v.string.data, val.v.string.length);
            }
            var->value.v.string.data = str;
            var->value.v.string.length = val.v.string.length;
            var->value.v.string.is_temp = false;
        } else {
            var->value.v.number = val.v.number;
        }
        return ERR_NONE;
    }
}

/*
 * stmt_goto - GOTO statement
 *
 * Syntax: GOTO line_number
 */
static ErrorCode stmt_goto(BasicState *state)
{
    double line;
    ErrorCode err = basic_eval_numeric(state, &line);
    if (err != ERR_NONE) return err;

    if (line < 0 || line > BASIC_LINE_NUM_MAX) {
        return ERR_US;
    }

    basic_goto_line(state, (int32_t)line);
    if (state->current_line == NULL) {
        return ERR_US;  /* Undefined statement */
    }

    return ERR_NONE;
}

/*
 * stmt_run - RUN statement
 *
 * Syntax: RUN [line_number]
 */
static ErrorCode stmt_run(BasicState *state)
{
    basic_skip_spaces(state);

    /* Clear variables */
    basic_clear_variables(state);

    /* Reset stack */
    state->stack_ptr = 0;

    /* Optional line number */
    if (BASIC_IS_DIGIT(basic_peek_char(state))) {
        double line;
        ErrorCode err = basic_eval_numeric(state, &line);
        if (err != ERR_NONE) return err;

        basic_goto_line(state, (int32_t)line);
        if (state->current_line == NULL) {
            return ERR_US;
        }
    } else {
        /* Start from beginning */
        state->current_line = state->program;
        if (state->current_line != NULL) {
            state->text_ptr = state->current_line->text;
            state->current_line_num = state->current_line->line_number;
        }
    }

    state->running = true;
    state->can_continue = true;
    return ERR_NONE;
}

/*
 * stmt_if - IF statement
 *
 * Syntax: IF expression THEN statement | line_number
 *         IF expression GOTO line_number
 */
static ErrorCode stmt_if(BasicState *state)
{
    /* Evaluate condition */
    Value cond;
    ErrorCode err = basic_evaluate(state, &cond);
    if (err != ERR_NONE) return err;

    /* Get truth value */
    bool is_true;
    if (cond.type == VAL_STRING) {
        is_true = (cond.v.string.length > 0);
    } else {
        is_true = (cond.v.number != 0.0);
    }

    basic_skip_spaces(state);

    /* Look for THEN or GOTO */
    bool has_then = false;
    bool has_goto = false;

    if (*state->text_ptr == (char)TOK_THEN) {
        state->text_ptr++;
        has_then = true;
    } else if (*state->text_ptr == (char)TOK_GOTO) {
        state->text_ptr++;
        has_goto = true;
    } else if (BASIC_TOUPPER(state->text_ptr[0]) == 'T' &&
               BASIC_TOUPPER(state->text_ptr[1]) == 'H' &&
               BASIC_TOUPPER(state->text_ptr[2]) == 'E' &&
               BASIC_TOUPPER(state->text_ptr[3]) == 'N') {
        state->text_ptr += 4;
        has_then = true;
    } else if (BASIC_TOUPPER(state->text_ptr[0]) == 'G' &&
               BASIC_TOUPPER(state->text_ptr[1]) == 'O' &&
               BASIC_TOUPPER(state->text_ptr[2]) == 'T' &&
               BASIC_TOUPPER(state->text_ptr[3]) == 'O') {
        state->text_ptr += 4;
        has_goto = true;
    }

    if (!has_then && !has_goto) {
        return ERR_SN;
    }

    if (!is_true) {
        /* Skip to end of line */
        while (!BASIC_IS_EOL(*state->text_ptr)) {
            state->text_ptr++;
        }
        return ERR_NONE;
    }

    /* Condition is true */
    basic_skip_spaces(state);

    /* Check for line number */
    if (BASIC_IS_DIGIT(basic_peek_char(state))) {
        double line;
        err = basic_eval_numeric(state, &line);
        if (err != ERR_NONE) return err;

        basic_goto_line(state, (int32_t)line);
        if (state->current_line == NULL) {
            return ERR_US;
        }
        return ERR_NONE;
    }

    /* Execute statement */
    return basic_execute_statement(state);
}

/*
 * stmt_restore - RESTORE statement
 *
 * Syntax: RESTORE [line_number]
 */
static ErrorCode stmt_restore(BasicState *state)
{
    basic_skip_spaces(state);

    if (BASIC_IS_DIGIT(basic_peek_char(state))) {
        double line;
        ErrorCode err = basic_eval_numeric(state, &line);
        if (err != ERR_NONE) return err;

        /* Find the line */
        ProgramLine *pl = basic_find_line(state, (int32_t)line);
        if (pl == NULL) {
            return ERR_US;
        }

        state->data_ptr.line = pl;
        state->data_ptr.position = NULL;
    } else {
        /* Reset to beginning */
        state->data_ptr.line = NULL;
        state->data_ptr.position = NULL;
    }

    return ERR_NONE;
}

/*
 * stmt_gosub - GOSUB statement
 *
 * Syntax: GOSUB line_number
 */
static ErrorCode stmt_gosub(BasicState *state)
{
    double line;
    ErrorCode err = basic_eval_numeric(state, &line);
    if (err != ERR_NONE) return err;

    /* Push return address */
    if (state->stack_ptr >= state->stack_size) {
        return ERR_OM;
    }

    StackEntry *entry = &state->stack[state->stack_ptr++];
    entry->type = STACK_GOSUB;
    entry->data.gosub.line_number = state->current_line_num;
    entry->data.gosub.text_ptr = state->text_ptr;

    /* Jump to subroutine */
    basic_goto_line(state, (int32_t)line);
    if (state->current_line == NULL) {
        state->stack_ptr--;  /* Pop the return address we just pushed */
        return ERR_US;
    }

    return ERR_NONE;
}

/*
 * stmt_return - RETURN statement
 *
 * Syntax: RETURN
 */
static ErrorCode stmt_return(BasicState *state)
{
    /* Find GOSUB entry on stack */
    int gosub_idx = -1;
    for (int i = state->stack_ptr - 1; i >= 0; i--) {
        if (state->stack[i].type == STACK_GOSUB) {
            gosub_idx = i;
            break;
        }
    }

    if (gosub_idx < 0) {
        return ERR_RG;  /* RETURN without GOSUB */
    }

    GosubEntry *ret = &state->stack[gosub_idx].data.gosub;

    /* Restore position */
    if (ret->line_number == 0) {
        /* Was in direct mode */
        state->running = false;
    } else {
        ProgramLine *pl = basic_find_line(state, ret->line_number);
        if (pl != NULL) {
            state->current_line = pl;
            state->current_line_num = pl->line_number;
            state->text_ptr = ret->text_ptr;
        }
    }

    /* Pop stack */
    state->stack_ptr = gosub_idx;

    return ERR_NONE;
}

/*
 * stmt_rem - REM statement (remark/comment)
 *
 * Syntax: REM anything
 */
static ErrorCode stmt_rem(BasicState *state)
{
    /* Skip to end of line */
    while (!BASIC_IS_EOL(*state->text_ptr)) {
        state->text_ptr++;
    }
    return ERR_NONE;
}

/*
 * stmt_stop - STOP statement
 *
 * Syntax: STOP
 */
static ErrorCode stmt_stop(BasicState *state)
{
    state->running = false;
    state->can_continue = true;
    state->cont_line = state->current_line;
    state->cont_ptr = state->text_ptr;

    basic_print_string(state, "\nBREAK");
    if (state->current_line_num > 0) {
        char msg[32];
        snprintf(msg, sizeof(msg), " IN %d", state->current_line_num);
        basic_print_string(state, msg);
    }
    basic_print_newline(state);

    return ERR_NONE;
}

/*
 * stmt_on - ON...GOTO or ON...GOSUB statement
 *
 * Syntax: ON expression GOTO line1,line2,...
 *         ON expression GOSUB line1,line2,...
 */
static ErrorCode stmt_on(BasicState *state)
{
    /* Evaluate index */
    double idx;
    ErrorCode err = basic_eval_numeric(state, &idx);
    if (err != ERR_NONE) return err;

    int index = (int)idx;

    /* Look for GOTO or GOSUB */
    basic_skip_spaces(state);
    bool is_gosub = false;

    if (*state->text_ptr == (char)TOK_GOTO) {
        state->text_ptr++;
    } else if (*state->text_ptr == (char)TOK_GOSUB) {
        state->text_ptr++;
        is_gosub = true;
    } else {
        return ERR_SN;
    }

    /* Find the nth line number */
    int count = 0;
    int32_t target_line = 0;

    while (1) {
        basic_skip_spaces(state);

        if (!BASIC_IS_DIGIT(basic_peek_char(state))) {
            break;
        }

        double line;
        err = basic_eval_numeric(state, &line);
        if (err != ERR_NONE) return err;

        count++;
        if (count == index) {
            target_line = (int32_t)line;
        }

        basic_skip_spaces(state);
        if (*state->text_ptr == ',') {
            state->text_ptr++;
        } else {
            break;
        }
    }

    /* If index out of range, continue to next statement */
    if (index < 1 || index > count) {
        return ERR_NONE;
    }

    /* Execute GOTO or GOSUB */
    if (is_gosub) {
        if (state->stack_ptr >= state->stack_size) {
            return ERR_OM;
        }

        StackEntry *entry = &state->stack[state->stack_ptr++];
        entry->type = STACK_GOSUB;
        entry->data.gosub.line_number = state->current_line_num;
        entry->data.gosub.text_ptr = state->text_ptr;
    }

    basic_goto_line(state, target_line);
    if (state->current_line == NULL) {
        if (is_gosub) {
            state->stack_ptr--;
        }
        return ERR_US;
    }

    return ERR_NONE;
}

/*
 * stmt_wait - WAIT statement
 *
 * Syntax: WAIT address, mask [, xor_value]
 *
 * Waits until (PEEK(address) XOR xor_value) AND mask is non-zero.
 * In this implementation, we just skip it (simulated hardware).
 */
static ErrorCode stmt_wait(BasicState *state)
{
    /* Parse but ignore arguments */
    double addr;
    ErrorCode err = basic_eval_numeric(state, &addr);
    if (err != ERR_NONE) return err;

    basic_skip_spaces(state);
    if (*state->text_ptr != ',') {
        return ERR_SN;
    }
    state->text_ptr++;

    double mask;
    err = basic_eval_numeric(state, &mask);
    if (err != ERR_NONE) return err;

    /* Optional XOR value */
    basic_skip_spaces(state);
    if (*state->text_ptr == ',') {
        state->text_ptr++;
        double xor_val;
        err = basic_eval_numeric(state, &xor_val);
        if (err != ERR_NONE) return err;
    }

    /* In real hardware, this would poll memory. We just continue. */
    return ERR_NONE;
}

/*
 * stmt_poke - POKE statement
 *
 * Syntax: POKE address, value
 */
static ErrorCode stmt_poke(BasicState *state)
{
    double addr;
    ErrorCode err = basic_eval_numeric(state, &addr);
    if (err != ERR_NONE) return err;

    basic_skip_spaces(state);
    if (*state->text_ptr != ',') {
        return ERR_SN;
    }
    state->text_ptr++;

    double value;
    err = basic_eval_numeric(state, &value);
    if (err != ERR_NONE) return err;

    /* Store in simulated memory */
    int32_t a = (int32_t)addr;
    int32_t v = (int32_t)value;

    if (a >= 0 && (size_t)a < state->memory_size && v >= 0 && v <= 255) {
        state->memory[a] = (uint8_t)v;
    }

    return ERR_NONE;
}

/*
 * stmt_print - PRINT statement
 *
 * Syntax: PRINT [expression [; | ,] ...]
 */
static ErrorCode stmt_print(BasicState *state)
{
    bool need_newline = true;

    while (1) {
        basic_skip_spaces(state);

        char c = basic_peek_char(state);

        /* End of statement */
        if (BASIC_IS_EOS(c)) {
            break;
        }

        /* TAB function */
        if (c == (char)TOK_TAB) {
            state->text_ptr++;
            /* Skip ( if present */
            basic_skip_spaces(state);
            if (*state->text_ptr == '(') {
                state->text_ptr++;
            }

            double pos;
            ErrorCode err = basic_eval_numeric(state, &pos);
            if (err != ERR_NONE) return err;

            /* Skip ) if present */
            basic_skip_spaces(state);
            if (*state->text_ptr == ')') {
                state->text_ptr++;
            }

            basic_print_tab(state, (int)pos);
            need_newline = true;
            continue;
        }

        /* SPC function */
        if (c == (char)TOK_SPC) {
            state->text_ptr++;
            basic_skip_spaces(state);
            if (*state->text_ptr == '(') {
                state->text_ptr++;
            }

            double count;
            ErrorCode err = basic_eval_numeric(state, &count);
            if (err != ERR_NONE) return err;

            basic_skip_spaces(state);
            if (*state->text_ptr == ')') {
                state->text_ptr++;
            }

            for (int i = 0; i < (int)count; i++) {
                basic_print_char(state, ' ');
            }
            need_newline = true;
            continue;
        }

        /* Semicolon - stay on same position */
        if (c == ';') {
            state->text_ptr++;
            need_newline = false;
            continue;
        }

        /* Comma - tab to next zone */
        if (c == ',') {
            state->text_ptr++;
            /* Tab to next zone (every 14 characters in original BASIC) */
            int zone_width = 14;
            int spaces = zone_width - (state->terminal_pos % zone_width);
            for (int i = 0; i < spaces; i++) {
                basic_print_char(state, ' ');
            }
            need_newline = false;
            continue;
        }

        /* Expression */
        Value val;
        ErrorCode err = basic_evaluate(state, &val);
        if (err != ERR_NONE) return err;

        if (val.type == VAL_STRING) {
            /* Print string directly */
            for (int i = 0; i < val.v.string.length; i++) {
                basic_print_char(state, val.v.string.data[i]);
            }
        } else {
            /* Print number */
            basic_print_number(state, val.v.number);
        }

        need_newline = true;
    }

    if (need_newline) {
        basic_print_newline(state);
    }

    return ERR_NONE;
}

/*
 * stmt_cont - CONT statement (continue)
 *
 * Syntax: CONT
 */
static ErrorCode stmt_cont(BasicState *state)
{
    if (!state->can_continue || state->cont_line == NULL) {
        return ERR_CN;
    }

    state->current_line = state->cont_line;
    state->current_line_num = state->cont_line->line_number;
    state->text_ptr = state->cont_ptr;
    state->running = true;

    return ERR_NONE;
}

/*
 * stmt_list - LIST statement
 *
 * Syntax: LIST [start_line] [-] [end_line]
 */
static ErrorCode stmt_list(BasicState *state)
{
    basic_skip_spaces(state);

    int32_t start = 0;
    int32_t end = BASIC_LINE_NUM_MAX;

    /* Parse optional start line */
    if (BASIC_IS_DIGIT(basic_peek_char(state))) {
        double line;
        ErrorCode err = basic_eval_numeric(state, &line);
        if (err != ERR_NONE) return err;
        start = (int32_t)line;
        end = start;  /* Default: single line */
    }

    basic_skip_spaces(state);

    /* Check for range */
    if (*state->text_ptr == '-') {
        state->text_ptr++;
        basic_skip_spaces(state);

        end = BASIC_LINE_NUM_MAX;  /* Default end */

        if (BASIC_IS_DIGIT(basic_peek_char(state))) {
            double line;
            ErrorCode err = basic_eval_numeric(state, &line);
            if (err != ERR_NONE) return err;
            end = (int32_t)line;
        }
    }

    basic_list(state, start, end);
    return ERR_NONE;
}

/*
 * stmt_clear - CLEAR statement
 *
 * Syntax: CLEAR
 */
static ErrorCode stmt_clear(BasicState *state)
{
    basic_clear_variables(state);
    state->stack_ptr = 0;
    return ERR_NONE;
}

/*
 * stmt_get - GET statement
 *
 * Syntax: GET var
 *
 * Reads a single character without waiting for Enter.
 */
static ErrorCode stmt_get(BasicState *state)
{
#if FEATURE_GET_CMD
    basic_skip_spaces(state);

    /* Parse variable (lvalue) */
    Variable *var = NULL;
    Array *arr = NULL;
    int indices[BASIC_ARRAY_DIMS];
    int num_indices = 0;

    ErrorCode err = parse_lvalue(state, &var, &arr, indices, &num_indices);
    if (err != ERR_NONE) return err;

    /* Read single character (non-blocking in original, but we simulate) */
    char ch = getchar();
    if (ch == EOF) {
        ch = '\0';
    }

    Value val;
    bool is_string = (var != NULL && var->name.is_string) ||
                    (arr != NULL && arr->name.is_string);

    if (is_string) {
        val.type = VAL_STRING;
        if (ch == '\n' || ch == '\r') {
            val.v.string.length = 0;
            val.v.string.data = "";
        } else {
            val.v.string = basic_copy_string(state, &ch, 1);
        }
    } else {
        val.type = VAL_NUMBER;
        val.v.number = (double)(unsigned char)ch;
    }

    if (arr != NULL) {
        return basic_set_array_element(state, arr, indices, &val);
    } else {
        var->value = val;
    }

    return ERR_NONE;
#else
    (void)state;
    return ERR_SN;
#endif
}

/*
 * stmt_new - NEW statement
 *
 * Syntax: NEW
 */
static ErrorCode stmt_new(BasicState *state)
{
    basic_new(state);
    return ERR_NONE;
}

/*
 * stmt_def - DEF statement
 *
 * Syntax: DEF FN name(param) = expression
 */
static ErrorCode stmt_def(BasicState *state)
{
    basic_skip_spaces(state);

    /* Expect FN */
    if (*state->text_ptr == (char)TOK_FN) {
        state->text_ptr++;
    } else if (BASIC_TOUPPER(state->text_ptr[0]) == 'F' &&
               BASIC_TOUPPER(state->text_ptr[1]) == 'N') {
        state->text_ptr += 2;
    } else {
        return ERR_SN;
    }

    basic_skip_spaces(state);

    /* Get function name (single letter) */
    char fn_name = BASIC_TOUPPER(basic_peek_char(state));
    if (!BASIC_IS_LETTER(fn_name)) {
        return ERR_SN;
    }
    state->text_ptr++;

    /* Expect ( */
    basic_skip_spaces(state);
    if (*state->text_ptr != '(') {
        return ERR_SN;
    }
    state->text_ptr++;

    /* Get parameter name */
    basic_skip_spaces(state);
    char param = BASIC_TOUPPER(basic_peek_char(state));
    if (!BASIC_IS_LETTER(param)) {
        return ERR_SN;
    }
    state->text_ptr++;

    /* Expect ) */
    basic_skip_spaces(state);
    if (*state->text_ptr != ')') {
        return ERR_SN;
    }
    state->text_ptr++;

    /* Expect = */
    basic_skip_spaces(state);
    if (*state->text_ptr != '=' && *state->text_ptr != (char)TOK_EQ) {
        return ERR_SN;
    }
    state->text_ptr++;

    /* Store the definition (rest of statement) */
    const char *def_start = state->text_ptr;

    /* Skip to end of statement to find definition length */
    while (!BASIC_IS_EOS(*state->text_ptr)) {
        state->text_ptr++;
    }

    size_t def_len = (size_t)(state->text_ptr - def_start);
    char *def = malloc(def_len + 1);
    if (def == NULL) {
        return ERR_OM;
    }
    memcpy(def, def_start, def_len);
    def[def_len] = '\0';

    ErrorCode err = basic_def_function(state, fn_name, param, def);
    free(def);

    return err;
}

/*
 * stmt_null - NULL statement
 *
 * Syntax: NULL count
 *
 * Sets the number of null characters to print after carriage return.
 */
static ErrorCode stmt_null(BasicState *state)
{
#if FEATURE_NULL_CMD
    double count;
    ErrorCode err = basic_eval_numeric(state, &count);
    if (err != ERR_NONE) return err;

    if (count < 0 || count > 255) {
        return ERR_FC;
    }

    state->null_count = (int)count;
    return ERR_NONE;
#else
    (void)state;
    return ERR_SN;
#endif
}

/*
 * =============================================================================
 * HELPER FUNCTIONS
 * =============================================================================
 */

/*
 * parse_lvalue - Parse a variable reference for assignment
 *
 * Handles both simple variables and array elements.
 *
 * Parameters:
 *   state       - BASIC interpreter state
 *   var_out     - Receives simple variable pointer (or NULL if array)
 *   arr_out     - Receives array pointer (or NULL if simple var)
 *   indices     - Receives array indices
 *   num_indices - Receives number of indices
 *
 * Returns:
 *   ERR_NONE on success, error code on failure
 */
static ErrorCode parse_lvalue(BasicState *state, Variable **var_out, Array **arr_out,
                              int *indices, int *num_indices)
{
    *var_out = NULL;
    *arr_out = NULL;
    *num_indices = 0;

    basic_skip_spaces(state);

    if (!BASIC_IS_LETTER(basic_peek_char(state))) {
        return ERR_SN;
    }

    /* Get variable name */
    char name[32];
    int name_pos = 0;
    while (BASIC_IS_LETTER(basic_peek_char(state)) || BASIC_IS_DIGIT(basic_peek_char(state))) {
        if (name_pos < 31) {
            name[name_pos++] = BASIC_TOUPPER(*state->text_ptr);
        }
        state->text_ptr++;
    }

    /* Check for type suffix */
    if (*state->text_ptr == '$' || *state->text_ptr == '%') {
        if (name_pos < 31) {
            name[name_pos++] = *state->text_ptr;
        }
        state->text_ptr++;
    }
    name[name_pos] = '\0';

    /* Check for array subscript */
    basic_skip_spaces(state);
    if (*state->text_ptr == '(') {
        state->text_ptr++;

        /* Get or create array */
        Array *arr = basic_get_array(state, name);
        if (arr == NULL) {
            /* Auto-dimension with default size */
            int dims[1] = {10};
            ErrorCode err = basic_dim_array(state, name, dims, 1);
            if (err != ERR_NONE) return err;
            arr = basic_get_array(state, name);
        }

        /* Parse subscripts */
        do {
            if (*num_indices > 0) {
                basic_skip_spaces(state);
                if (*state->text_ptr != ',') break;
                state->text_ptr++;
            }

            double idx;
            ErrorCode err = basic_eval_numeric(state, &idx);
            if (err != ERR_NONE) return err;

            if (*num_indices >= BASIC_ARRAY_DIMS) {
                return ERR_BS;
            }
            indices[(*num_indices)++] = (int)idx;
        } while (1);

        basic_skip_spaces(state);
        if (*state->text_ptr != ')') {
            return ERR_SN;
        }
        state->text_ptr++;

        *arr_out = arr;
    } else {
        /* Simple variable */
        Variable *var = basic_get_variable(state, name);
        if (var == NULL) {
            var = basic_create_variable(state, name);
            if (var == NULL) {
                return ERR_OM;
            }
        }
        *var_out = var;
    }

    return ERR_NONE;
}
