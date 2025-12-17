/*
 * CBASIC - Expression Evaluator Module
 *
 * Implements the expression evaluation engine (FRMEVL) using operator
 * precedence parsing, matching the 6502 assembly implementation.
 *
 * Based on 6502 implementation at lines 3187-3597 of m6502.asm
 *
 * Copyright (c) 2025 Tim Buchalka
 * Licensed under the MIT License
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include "basic.h"

/*
 * =============================================================================
 * OPERATOR PRECEDENCE TABLE
 * =============================================================================
 * Precedence values match the 6502 implementation's OPTAB table.
 * Higher values = higher precedence.
 */

typedef enum {
    PREC_NONE       = 0,
    PREC_OR         = 70,   /* OR */
    PREC_AND        = 80,   /* AND */
    PREC_NOT        = 90,   /* NOT (unary) */
    PREC_COMPARE    = 100,  /* =, <>, <, >, <=, >= */
    PREC_ADD        = 121,  /* +, - */
    PREC_MULT       = 123,  /* *, / */
    PREC_NEGATE     = 125,  /* Unary minus */
    PREC_POWER      = 127   /* ^ */
} Precedence;

/*
 * Forward declarations for recursive descent
 */
static ErrorCode eval_expression(BasicState *state, Value *result);
static ErrorCode eval_or(BasicState *state, Value *result);
static ErrorCode eval_and(BasicState *state, Value *result);
static ErrorCode eval_not(BasicState *state, Value *result);
static ErrorCode eval_comparison(BasicState *state, Value *result);
static ErrorCode eval_additive(BasicState *state, Value *result);
static ErrorCode eval_multiplicative(BasicState *state, Value *result);
static ErrorCode eval_power(BasicState *state, Value *result);
static ErrorCode eval_unary(BasicState *state, Value *result);
static ErrorCode eval_primary(BasicState *state, Value *result);
static ErrorCode eval_function(BasicState *state, Token func, Value *result);
static ErrorCode parse_number(BasicState *state, double *result);
static ErrorCode parse_string_literal(BasicState *state, StringDescriptor *result);
static ErrorCode parse_variable(BasicState *state, Value *result, bool *is_array);

/*
 * Helper to check value type
 */
static inline bool is_numeric(const Value *v)
{
    return v->type == VAL_NUMBER || v->type == VAL_INTEGER;
}

static inline double get_numeric(const Value *v)
{
    if (v->type == VAL_INTEGER) {
        return (double)v->v.integer;
    }
    return v->v.number;
}

/*
 * =============================================================================
 * MAIN ENTRY POINTS
 * =============================================================================
 */

/*
 * basic_evaluate - Evaluate an expression and return its value
 *
 * This is the main entry point for expression evaluation, equivalent
 * to the FRMEVL routine in the 6502 assembly.
 *
 * Parameters:
 *   state  - BASIC interpreter state
 *   result - Receives the evaluated value
 *
 * Returns:
 *   ERR_NONE on success, error code on failure
 */
ErrorCode basic_evaluate(BasicState *state, Value *result)
{
    if (state == NULL || result == NULL) {
        return ERR_SN;
    }

    basic_skip_spaces(state);
    return eval_expression(state, result);
}

/*
 * basic_eval_numeric - Evaluate an expression and return numeric result
 *
 * Parameters:
 *   state  - BASIC interpreter state
 *   result - Receives the numeric value
 *
 * Returns:
 *   ERR_NONE on success, ERR_TM if not numeric, other errors as appropriate
 */
ErrorCode basic_eval_numeric(BasicState *state, double *result)
{
    Value val;
    ErrorCode err = basic_evaluate(state, &val);

    if (err != ERR_NONE) {
        return err;
    }

    if (!is_numeric(&val)) {
        return ERR_TM;  /* Type mismatch */
    }

    *result = get_numeric(&val);
    return ERR_NONE;
}

/*
 * basic_eval_integer - Evaluate expression and return integer result
 *
 * The result is truncated toward zero, matching INT() behavior.
 *
 * Parameters:
 *   state  - BASIC interpreter state
 *   result - Receives the integer value
 *
 * Returns:
 *   ERR_NONE on success, ERR_FC if out of range, other errors as appropriate
 */
ErrorCode basic_eval_integer(BasicState *state, int32_t *result)
{
    double num;
    ErrorCode err = basic_eval_numeric(state, &num);

    if (err != ERR_NONE) {
        return err;
    }

    /* Check range */
    if (num < -2147483648.0 || num > 2147483647.0) {
        return ERR_FC;  /* Illegal function call (overflow) */
    }

    *result = (int32_t)num;
    return ERR_NONE;
}

/*
 * basic_eval_string - Evaluate expression and return string result
 *
 * Parameters:
 *   state  - BASIC interpreter state
 *   result - Receives the string descriptor
 *
 * Returns:
 *   ERR_NONE on success, ERR_TM if not a string, other errors as appropriate
 */
ErrorCode basic_eval_string(BasicState *state, StringDescriptor *result)
{
    Value val;
    ErrorCode err = basic_evaluate(state, &val);

    if (err != ERR_NONE) {
        return err;
    }

    if (val.type != VAL_STRING) {
        return ERR_TM;  /* Type mismatch */
    }

    *result = val.v.string;
    return ERR_NONE;
}

/*
 * =============================================================================
 * RECURSIVE DESCENT PARSER
 * =============================================================================
 * Expression grammar (in order of precedence, lowest to highest):
 *
 * expression     -> or_expr
 * or_expr        -> and_expr ( OR and_expr )*
 * and_expr       -> not_expr ( AND not_expr )*
 * not_expr       -> NOT not_expr | comparison
 * comparison     -> additive ( ( = | <> | < | > | <= | >= ) additive )*
 * additive       -> multiplicative ( ( + | - ) multiplicative )*
 * multiplicative -> power ( ( * | / ) power )*
 * power          -> unary ( ^ power )?       (right associative)
 * unary          -> - unary | + unary | primary
 * primary        -> NUMBER | STRING | VARIABLE | FUNCTION | ( expression )
 */

static ErrorCode eval_expression(BasicState *state, Value *result)
{
    return eval_or(state, result);
}

/*
 * eval_or - Handle OR operations
 */
static ErrorCode eval_or(BasicState *state, Value *result)
{
    ErrorCode err = eval_and(state, result);
    if (err != ERR_NONE) return err;

    basic_skip_spaces(state);

    while (basic_peek_char(state) == (char)TOK_OR ||
           (BASIC_TOUPPER(*state->text_ptr) == 'O' &&
            BASIC_TOUPPER(*(state->text_ptr + 1)) == 'R')) {

        if (basic_peek_char(state) == (char)TOK_OR) {
            state->text_ptr++;
        } else {
            state->text_ptr += 2;  /* Skip "OR" */
        }

        if (!is_numeric(result)) {
            return ERR_TM;
        }

        Value right;
        err = eval_and(state, &right);
        if (err != ERR_NONE) return err;

        if (!is_numeric(&right)) {
            return ERR_TM;
        }

        /* Logical OR - treat non-zero as true */
        int32_t lval = (int32_t)get_numeric(result);
        int32_t rval = (int32_t)get_numeric(&right);
        result->type = VAL_NUMBER;
        result->v.number = (double)(lval | rval);

        basic_skip_spaces(state);
    }

    return ERR_NONE;
}

/*
 * eval_and - Handle AND operations
 */
static ErrorCode eval_and(BasicState *state, Value *result)
{
    ErrorCode err = eval_not(state, result);
    if (err != ERR_NONE) return err;

    basic_skip_spaces(state);

    while (basic_peek_char(state) == (char)TOK_AND ||
           (BASIC_TOUPPER(*state->text_ptr) == 'A' &&
            BASIC_TOUPPER(*(state->text_ptr + 1)) == 'N' &&
            BASIC_TOUPPER(*(state->text_ptr + 2)) == 'D')) {

        if (basic_peek_char(state) == (char)TOK_AND) {
            state->text_ptr++;
        } else {
            state->text_ptr += 3;  /* Skip "AND" */
        }

        if (!is_numeric(result)) {
            return ERR_TM;
        }

        Value right;
        err = eval_not(state, &right);
        if (err != ERR_NONE) return err;

        if (!is_numeric(&right)) {
            return ERR_TM;
        }

        /* Bitwise AND */
        int32_t lval = (int32_t)get_numeric(result);
        int32_t rval = (int32_t)get_numeric(&right);
        result->type = VAL_NUMBER;
        result->v.number = (double)(lval & rval);

        basic_skip_spaces(state);
    }

    return ERR_NONE;
}

/*
 * eval_not - Handle NOT operations (unary)
 */
static ErrorCode eval_not(BasicState *state, Value *result)
{
    basic_skip_spaces(state);

    if (basic_peek_char(state) == (char)TOK_NOT ||
        (BASIC_TOUPPER(*state->text_ptr) == 'N' &&
         BASIC_TOUPPER(*(state->text_ptr + 1)) == 'O' &&
         BASIC_TOUPPER(*(state->text_ptr + 2)) == 'T')) {

        if (basic_peek_char(state) == (char)TOK_NOT) {
            state->text_ptr++;
        } else {
            state->text_ptr += 3;  /* Skip "NOT" */
        }

        ErrorCode err = eval_not(state, result);
        if (err != ERR_NONE) return err;

        if (!is_numeric(result)) {
            return ERR_TM;
        }

        /* Bitwise NOT */
        int32_t val = (int32_t)get_numeric(result);
        result->type = VAL_NUMBER;
        result->v.number = (double)(~val);

        return ERR_NONE;
    }

    return eval_comparison(state, result);
}

/*
 * eval_comparison - Handle comparison operators (=, <>, <, >, <=, >=)
 */
static ErrorCode eval_comparison(BasicState *state, Value *result)
{
    ErrorCode err = eval_additive(state, result);
    if (err != ERR_NONE) return err;

    basic_skip_spaces(state);

    while (1) {
        char c = basic_peek_char(state);
        int op = 0;  /* Comparison type: 1=<, 2==, 4=> */

        if (c == '<' || c == (char)TOK_LT) {
            op = 1;
            state->text_ptr++;
            c = basic_peek_char(state);
            if (c == '>' || c == (char)TOK_GT) {
                op = 1 | 4;  /* <> (not equal) */
                state->text_ptr++;
            } else if (c == '=' || c == (char)TOK_EQ) {
                op = 1 | 2;  /* <= */
                state->text_ptr++;
            }
        } else if (c == '>' || c == (char)TOK_GT) {
            op = 4;
            state->text_ptr++;
            c = basic_peek_char(state);
            if (c == '=' || c == (char)TOK_EQ) {
                op = 4 | 2;  /* >= */
                state->text_ptr++;
            } else if (c == '<' || c == (char)TOK_LT) {
                op = 4 | 1;  /* >< (same as <>) */
                state->text_ptr++;
            }
        } else if (c == '=' || c == (char)TOK_EQ) {
            op = 2;
            state->text_ptr++;
            c = basic_peek_char(state);
            if (c == '<' || c == (char)TOK_LT) {
                op = 2 | 1;  /* =< (same as <=) */
                state->text_ptr++;
            } else if (c == '>' || c == (char)TOK_GT) {
                op = 2 | 4;  /* => (same as >=) */
                state->text_ptr++;
            }
        } else {
            break;  /* No comparison operator */
        }

        Value right;
        err = eval_additive(state, &right);
        if (err != ERR_NONE) return err;

        /* Perform comparison */
        int cmp_result;

        if (result->type == VAL_STRING && right.type == VAL_STRING) {
            /* String comparison */
            int len1 = result->v.string.length;
            int len2 = right.v.string.length;
            int min_len = (len1 < len2) ? len1 : len2;

            cmp_result = 0;
            for (int i = 0; i < min_len && cmp_result == 0; i++) {
                cmp_result = (unsigned char)result->v.string.data[i] -
                            (unsigned char)right.v.string.data[i];
            }
            if (cmp_result == 0) {
                cmp_result = len1 - len2;
            }
        } else if (is_numeric(result) && is_numeric(&right)) {
            /* Numeric comparison */
            double lval = get_numeric(result);
            double rval = get_numeric(&right);

            if (lval < rval) cmp_result = -1;
            else if (lval > rval) cmp_result = 1;
            else cmp_result = 0;
        } else {
            return ERR_TM;  /* Type mismatch */
        }

        /* Convert comparison result to BASIC truth value */
        /* In MS BASIC: true = -1, false = 0 */
        bool is_true = false;
        if ((op & 1) && cmp_result < 0) is_true = true;   /* < */
        if ((op & 2) && cmp_result == 0) is_true = true;  /* = */
        if ((op & 4) && cmp_result > 0) is_true = true;   /* > */

        result->type = VAL_NUMBER;
        result->v.number = is_true ? -1.0 : 0.0;

        basic_skip_spaces(state);
    }

    return ERR_NONE;
}

/*
 * eval_additive - Handle + and - operations
 */
static ErrorCode eval_additive(BasicState *state, Value *result)
{
    ErrorCode err = eval_multiplicative(state, result);
    if (err != ERR_NONE) return err;

    basic_skip_spaces(state);

    while (1) {
        char c = basic_peek_char(state);
        bool is_add = (c == '+' || c == (char)TOK_PLUS);
        bool is_sub = (c == '-' || c == (char)TOK_MINUS);

        if (!is_add && !is_sub) break;

        state->text_ptr++;

        /* String concatenation */
        if (is_add && result->type == VAL_STRING) {
            Value right;
            err = eval_multiplicative(state, &right);
            if (err != ERR_NONE) return err;

            if (right.type != VAL_STRING) {
                return ERR_TM;
            }

            /* Concatenate strings */
            size_t new_len = result->v.string.length + right.v.string.length;
            if (new_len > BASIC_STRING_MAX) {
                return ERR_LS;  /* String too long */
            }

            char *new_str = basic_alloc_string(state, new_len);
            if (new_str == NULL) {
                return ERR_OM;
            }

            memcpy(new_str, result->v.string.data, result->v.string.length);
            memcpy(new_str + result->v.string.length, right.v.string.data, right.v.string.length);

            result->v.string.data = new_str;
            result->v.string.length = (uint8_t)new_len;
            result->v.string.is_temp = true;

            basic_skip_spaces(state);
            continue;
        }

        if (!is_numeric(result)) {
            return ERR_TM;
        }

        Value right;
        err = eval_multiplicative(state, &right);
        if (err != ERR_NONE) return err;

        if (!is_numeric(&right)) {
            return ERR_TM;
        }

        double lval = get_numeric(result);
        double rval = get_numeric(&right);

        result->type = VAL_NUMBER;
        if (is_add) {
            result->v.number = lval + rval;
        } else {
            result->v.number = lval - rval;
        }

        /* Check for overflow */
        if (!isfinite(result->v.number)) {
            return ERR_OV;
        }

        basic_skip_spaces(state);
    }

    return ERR_NONE;
}

/*
 * eval_multiplicative - Handle * and / operations
 */
static ErrorCode eval_multiplicative(BasicState *state, Value *result)
{
    ErrorCode err = eval_power(state, result);
    if (err != ERR_NONE) return err;

    basic_skip_spaces(state);

    while (1) {
        char c = basic_peek_char(state);
        bool is_mult = (c == '*' || c == (char)TOK_MULTIPLY);
        bool is_div = (c == '/' || c == (char)TOK_DIVIDE);

        if (!is_mult && !is_div) break;

        state->text_ptr++;

        if (!is_numeric(result)) {
            return ERR_TM;
        }

        Value right;
        err = eval_power(state, &right);
        if (err != ERR_NONE) return err;

        if (!is_numeric(&right)) {
            return ERR_TM;
        }

        double lval = get_numeric(result);
        double rval = get_numeric(&right);

        result->type = VAL_NUMBER;
        if (is_mult) {
            result->v.number = lval * rval;
        } else {
            /* Division by zero check */
            if (rval == 0.0) {
                return ERR_DZ;
            }
            result->v.number = lval / rval;
        }

        /* Check for overflow */
        if (!isfinite(result->v.number)) {
            return ERR_OV;
        }

        basic_skip_spaces(state);
    }

    return ERR_NONE;
}

/*
 * eval_power - Handle ^ (exponentiation) - right associative
 */
static ErrorCode eval_power(BasicState *state, Value *result)
{
    ErrorCode err = eval_unary(state, result);
    if (err != ERR_NONE) return err;

    basic_skip_spaces(state);

    char c = basic_peek_char(state);
    if (c == '^' || c == (char)TOK_POWER) {
        state->text_ptr++;

        if (!is_numeric(result)) {
            return ERR_TM;
        }

        Value right;
        err = eval_power(state, &right);  /* Right associative */
        if (err != ERR_NONE) return err;

        if (!is_numeric(&right)) {
            return ERR_TM;
        }

        double base = get_numeric(result);
        double exp = get_numeric(&right);

        /* Handle special cases like original BASIC */
        if (base < 0.0 && floor(exp) != exp) {
            return ERR_FC;  /* Can't raise negative to non-integer power */
        }

        result->type = VAL_NUMBER;
        result->v.number = pow(base, exp);

        /* Check for overflow/domain error */
        if (!isfinite(result->v.number)) {
            return ERR_OV;
        }
    }

    return ERR_NONE;
}

/*
 * eval_unary - Handle unary + and - operators
 */
static ErrorCode eval_unary(BasicState *state, Value *result)
{
    basic_skip_spaces(state);

    char c = basic_peek_char(state);

    if (c == '-' || c == (char)TOK_MINUS) {
        state->text_ptr++;

        ErrorCode err = eval_unary(state, result);
        if (err != ERR_NONE) return err;

        if (!is_numeric(result)) {
            return ERR_TM;
        }

        result->v.number = -get_numeric(result);
        result->type = VAL_NUMBER;
        return ERR_NONE;
    }

    if (c == '+' || c == (char)TOK_PLUS) {
        state->text_ptr++;

        ErrorCode err = eval_unary(state, result);
        if (err != ERR_NONE) return err;

        if (!is_numeric(result)) {
            return ERR_TM;
        }

        result->v.number = get_numeric(result);
        result->type = VAL_NUMBER;
        return ERR_NONE;
    }

    return eval_primary(state, result);
}

/*
 * eval_primary - Handle primary expressions (numbers, strings, variables, functions, parentheses)
 */
static ErrorCode eval_primary(BasicState *state, Value *result)
{
    basic_skip_spaces(state);

    char c = basic_peek_char(state);

    /* Parenthesized expression */
    if (c == '(') {
        state->text_ptr++;
        ErrorCode err = eval_expression(state, result);
        if (err != ERR_NONE) return err;

        basic_skip_spaces(state);
        if (basic_peek_char(state) != ')') {
            return ERR_SN;  /* Syntax error - missing ) */
        }
        state->text_ptr++;
        return ERR_NONE;
    }

    /* String literal */
    if (c == '"') {
        result->type = VAL_STRING;
        return parse_string_literal(state, &result->v.string);
    }

    /* Number */
    if (BASIC_IS_DIGIT(c) || c == '.') {
        result->type = VAL_NUMBER;
        return parse_number(state, &result->v.number);
    }

    /* Token (function or keyword) */
    if (BASIC_IS_TOKEN(c)) {
        Token tok = (Token)(unsigned char)c;

        /* Check for functions */
        if (BASIC_IS_FUNCTION(tok)) {
            state->text_ptr++;
            return eval_function(state, tok, result);
        }

        /* FN for user-defined function */
        if (tok == TOK_FN) {
            state->text_ptr++;
            basic_skip_spaces(state);

            /* Get function name (single letter) */
            char fn_name = BASIC_TOUPPER(basic_peek_char(state));
            if (!BASIC_IS_LETTER(fn_name)) {
                return ERR_SN;
            }
            state->text_ptr++;

            /* Expect ( */
            basic_skip_spaces(state);
            if (basic_peek_char(state) != '(') {
                return ERR_SN;
            }
            state->text_ptr++;

            /* Evaluate argument */
            double arg;
            ErrorCode err = basic_eval_numeric(state, &arg);
            if (err != ERR_NONE) return err;

            /* Expect ) */
            basic_skip_spaces(state);
            if (basic_peek_char(state) != ')') {
                return ERR_SN;
            }
            state->text_ptr++;

            /* Call user function */
            double fn_result;
            err = basic_call_function(state, fn_name, arg, &fn_result);
            if (err != ERR_NONE) return err;

            result->type = VAL_NUMBER;
            result->v.number = fn_result;
            return ERR_NONE;
        }

        return ERR_SN;  /* Unexpected token */
    }

    /* Variable or array */
    if (BASIC_IS_LETTER(c)) {
        bool is_array;
        return parse_variable(state, result, &is_array);
    }

    return ERR_SN;  /* Syntax error */
}

/*
 * =============================================================================
 * FUNCTION EVALUATION
 * =============================================================================
 */

static ErrorCode eval_function(BasicState *state, Token func, Value *result)
{
    basic_skip_spaces(state);

    /* Most functions require parentheses */
    bool need_parens = (func != TOK_RND);  /* RND can be without parens */

    if (basic_peek_char(state) == '(') {
        state->text_ptr++;
    } else if (need_parens) {
        return ERR_SN;
    }

    ErrorCode err;

    switch (func) {
        /* Numeric functions with one argument */
        case TOK_SGN:
        case TOK_INT:
        case TOK_ABS:
        case TOK_SQR:
        case TOK_LOG:
        case TOK_EXP:
        case TOK_SIN:
        case TOK_COS:
        case TOK_TAN:
        case TOK_ATN:
        case TOK_PEEK:
        case TOK_FRE:
        case TOK_POS:
        case TOK_RND:
        {
            double arg = 0;

            /* RND can have optional argument */
            if (func == TOK_RND) {
                if (basic_peek_char(state) != ')' && basic_peek_char(state) != '\0' &&
                    !BASIC_IS_EOS(basic_peek_char(state))) {
                    err = basic_eval_numeric(state, &arg);
                    if (err != ERR_NONE) return err;
                } else {
                    arg = 1;  /* Default argument */
                }
            } else {
                err = basic_eval_numeric(state, &arg);
                if (err != ERR_NONE) return err;
            }

            result->type = VAL_NUMBER;

            switch (func) {
                case TOK_SGN:
                    result->v.number = basic_fn_sgn(arg);
                    break;
                case TOK_INT:
                    result->v.number = basic_fn_int(arg);
                    break;
                case TOK_ABS:
                    result->v.number = basic_fn_abs(arg);
                    break;
                case TOK_SQR:
                    if (arg < 0) return ERR_FC;
                    result->v.number = basic_fn_sqr(arg);
                    break;
                case TOK_LOG:
                    if (arg <= 0) return ERR_FC;
                    result->v.number = basic_fn_log(arg);
                    break;
                case TOK_EXP:
                    result->v.number = basic_fn_exp(arg);
                    if (!isfinite(result->v.number)) return ERR_OV;
                    break;
                case TOK_SIN:
                    result->v.number = basic_fn_sin(arg);
                    break;
                case TOK_COS:
                    result->v.number = basic_fn_cos(arg);
                    break;
                case TOK_TAN:
                    result->v.number = basic_fn_tan(arg);
                    if (!isfinite(result->v.number)) return ERR_OV;
                    break;
                case TOK_ATN:
                    result->v.number = basic_fn_atn(arg);
                    break;
                case TOK_PEEK:
                    result->v.number = basic_fn_peek(state, (int32_t)arg);
                    break;
                case TOK_FRE:
                    result->v.number = basic_fn_fre(state, arg);
                    break;
                case TOK_POS:
                    result->v.number = basic_fn_pos(state, arg);
                    break;
                case TOK_RND:
                    result->v.number = basic_fn_rnd(state, arg);
                    break;
                default:
                    return ERR_SN;
            }
            break;
        }

        /* USR function - call machine code (simulated) */
        case TOK_USR:
        {
            double arg;
            err = basic_eval_numeric(state, &arg);
            if (err != ERR_NONE) return err;

            /* USR is not meaningfully implementable in C - return argument */
            result->type = VAL_NUMBER;
            result->v.number = arg;
            break;
        }

        /* String functions returning numbers */
        case TOK_LEN:
        case TOK_ASC:
        case TOK_VAL:
        {
            if (func == TOK_VAL) {
                StringDescriptor str;
                err = basic_eval_string(state, &str);
                if (err != ERR_NONE) return err;

                result->type = VAL_NUMBER;
                /* Null-terminate temporarily for parsing */
                char temp[256];
                size_t len = (str.length < 255) ? str.length : 255;
                memcpy(temp, str.data, len);
                temp[len] = '\0';
                result->v.number = basic_fn_val(temp);
            } else {
                StringDescriptor str;
                err = basic_eval_string(state, &str);
                if (err != ERR_NONE) return err;

                result->type = VAL_NUMBER;
                if (func == TOK_LEN) {
                    result->v.number = basic_fn_len(&str);
                } else {  /* ASC */
                    if (str.length == 0) return ERR_FC;
                    result->v.number = basic_fn_asc(&str);
                }
            }
            break;
        }

        /* Functions returning strings */
        case TOK_STR:
        {
            double arg;
            err = basic_eval_numeric(state, &arg);
            if (err != ERR_NONE) return err;

            result->type = VAL_STRING;
            result->v.string = basic_fn_str(state, arg);
            break;
        }

        case TOK_CHR:
        {
            double arg;
            err = basic_eval_numeric(state, &arg);
            if (err != ERR_NONE) return err;

            if (arg < 0 || arg > 255) return ERR_FC;

            result->type = VAL_STRING;
            result->v.string = basic_fn_chr(state, (int32_t)arg);
            break;
        }

        /* LEFT$, RIGHT$, MID$ - two or three arguments */
        case TOK_LEFT:
        case TOK_RIGHT:
        {
            StringDescriptor str;
            err = basic_eval_string(state, &str);
            if (err != ERR_NONE) return err;

            basic_skip_spaces(state);
            if (basic_peek_char(state) != ',') return ERR_SN;
            state->text_ptr++;

            double n;
            err = basic_eval_numeric(state, &n);
            if (err != ERR_NONE) return err;

            if (n < 0 || n > 255) return ERR_FC;

            result->type = VAL_STRING;
            if (func == TOK_LEFT) {
                result->v.string = basic_fn_left(state, &str, (int32_t)n);
            } else {
                result->v.string = basic_fn_right(state, &str, (int32_t)n);
            }
            break;
        }

        case TOK_MID:
        {
            StringDescriptor str;
            err = basic_eval_string(state, &str);
            if (err != ERR_NONE) return err;

            basic_skip_spaces(state);
            if (basic_peek_char(state) != ',') return ERR_SN;
            state->text_ptr++;

            double start;
            err = basic_eval_numeric(state, &start);
            if (err != ERR_NONE) return err;

            if (start < 1 || start > 255) return ERR_FC;

            /* Optional third argument (length) */
            int32_t length = 255;  /* Default: rest of string */
            basic_skip_spaces(state);
            if (basic_peek_char(state) == ',') {
                state->text_ptr++;
                double len;
                err = basic_eval_numeric(state, &len);
                if (err != ERR_NONE) return err;
                if (len < 0 || len > 255) return ERR_FC;
                length = (int32_t)len;
            }

            result->type = VAL_STRING;
            result->v.string = basic_fn_mid(state, &str, (int32_t)start, length);
            break;
        }

        default:
            return ERR_SN;
    }

    /* Expect closing parenthesis if we had opening */
    basic_skip_spaces(state);
    if (basic_peek_char(state) == ')') {
        state->text_ptr++;
    }

    return ERR_NONE;
}

/*
 * =============================================================================
 * PARSING HELPERS
 * =============================================================================
 */

/*
 * parse_number - Parse a numeric constant
 *
 * Handles integers, decimals, and exponential notation.
 */
static ErrorCode parse_number(BasicState *state, double *result)
{
    char buffer[64];
    int buf_pos = 0;
    bool has_digit = false;
    bool has_decimal = false;
    bool has_exp = false;

    /* Collect number characters */
    while (buf_pos < 63) {
        char c = basic_peek_char(state);

        if (BASIC_IS_DIGIT(c)) {
            buffer[buf_pos++] = c;
            state->text_ptr++;
            has_digit = true;
        } else if (c == '.' && !has_decimal && !has_exp) {
            buffer[buf_pos++] = c;
            state->text_ptr++;
            has_decimal = true;
        } else if ((c == 'E' || c == 'e') && !has_exp && has_digit) {
            buffer[buf_pos++] = 'E';
            state->text_ptr++;
            has_exp = true;

            /* Optional sign after E */
            c = basic_peek_char(state);
            if (c == '+' || c == '-') {
                buffer[buf_pos++] = c;
                state->text_ptr++;
            }
        } else {
            break;
        }
    }

    if (!has_digit) {
        return ERR_SN;
    }

    buffer[buf_pos] = '\0';

    /* Parse the number */
    char *endptr;
    errno = 0;
    *result = strtod(buffer, &endptr);

    if (errno == ERANGE) {
        return ERR_OV;
    }

    return ERR_NONE;
}

/*
 * parse_string_literal - Parse a quoted string
 */
static ErrorCode parse_string_literal(BasicState *state, StringDescriptor *result)
{
    if (basic_peek_char(state) != '"') {
        return ERR_SN;
    }
    state->text_ptr++;

    const char *start = state->text_ptr;
    size_t len = 0;

    /* Find end of string */
    while (*state->text_ptr != '"' && *state->text_ptr != '\0' && *state->text_ptr != '\n') {
        state->text_ptr++;
        len++;
    }

    if (len > BASIC_STRING_MAX) {
        return ERR_LS;
    }

    /* Copy the string */
    *result = basic_copy_string(state, start, len);

    /* Skip closing quote if present */
    if (*state->text_ptr == '"') {
        state->text_ptr++;
    }

    return ERR_NONE;
}

/*
 * parse_variable - Parse a variable reference or array access
 */
static ErrorCode parse_variable(BasicState *state, Value *result, bool *is_array)
{
    *is_array = false;

    char name[32];
    int name_pos = 0;

    /* Get variable name (up to 2 significant characters in original BASIC) */
    while (BASIC_IS_LETTER(basic_peek_char(state)) || BASIC_IS_DIGIT(basic_peek_char(state))) {
        if (name_pos < 31) {
            name[name_pos++] = BASIC_TOUPPER(*state->text_ptr);
        }
        state->text_ptr++;
    }

    /* Check for type suffix */
    char c = basic_peek_char(state);
    bool is_string = false;

    if (c == '$') {
        is_string = true;
        if (name_pos < 31) name[name_pos++] = '$';
        state->text_ptr++;
    } else if (c == '%') {
        /* Integer suffix - tracked in name */
        if (name_pos < 31) name[name_pos++] = '%';
        state->text_ptr++;
    }

    name[name_pos] = '\0';

    /* Check for array subscript */
    basic_skip_spaces(state);
    if (basic_peek_char(state) == '(') {
        *is_array = true;
        state->text_ptr++;

        /* Get array */
        Array *arr = basic_get_array(state, name);
        if (arr == NULL) {
            /* Auto-dimension with default size (0-10) */
            int dims[1] = {10};
            ErrorCode err = basic_dim_array(state, name, dims, 1);
            if (err != ERR_NONE) return err;
            arr = basic_get_array(state, name);
        }

        /* Parse subscripts */
        int indices[BASIC_ARRAY_DIMS];
        int num_indices = 0;

        do {
            if (num_indices > 0) {
                basic_skip_spaces(state);
                if (basic_peek_char(state) != ',') break;
                state->text_ptr++;
            }

            if (num_indices >= arr->num_dims) {
                return ERR_BS;  /* Too many subscripts */
            }

            double idx;
            ErrorCode err = basic_eval_numeric(state, &idx);
            if (err != ERR_NONE) return err;

            indices[num_indices++] = (int)idx;
        } while (1);

        basic_skip_spaces(state);
        if (basic_peek_char(state) != ')') {
            return ERR_SN;
        }
        state->text_ptr++;

        if (num_indices != arr->num_dims) {
            return ERR_BS;  /* Wrong number of subscripts */
        }

        /* Get array element */
        return basic_get_array_element(state, arr, indices, result);
    }

    /* Simple variable */
    Variable *var = basic_get_variable(state, name);
    if (var == NULL) {
        /* Create with default value */
        var = basic_create_variable(state, name);
        if (var == NULL) {
            return ERR_OM;
        }

        if (is_string) {
            var->value.type = VAL_STRING;
            var->value.v.string.length = 0;
            var->value.v.string.data = "";
            var->value.v.string.is_temp = false;
        } else {
            var->value.type = VAL_NUMBER;
            var->value.v.number = 0.0;
        }
    }

    *result = var->value;
    return ERR_NONE;
}
