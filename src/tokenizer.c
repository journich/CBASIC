/*
 * CBASIC - Tokenizer Module
 *
 * Implements the tokenization (CRUNCH) and detokenization (LIST) functions
 * for converting BASIC source code to/from compact token format.
 *
 * Based on the 6502 implementation at lines 1785-1970 of m6502.asm
 *
 * Copyright (c) 2025 Tim Buchalka
 * Licensed under the MIT License
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "basic.h"

/*
 * =============================================================================
 * RESERVED WORD TABLE
 * =============================================================================
 * Reserved words in the order they should be checked during tokenization.
 * This order matches the 6502 implementation's RESLST table.
 *
 * Note: Longer words should come before shorter words that are prefixes
 * (e.g., "GOTO" before "GO", "PRINT" before "PR").
 */

typedef struct {
    const char *word;   /* Reserved word text */
    Token token;        /* Corresponding token */
} ReservedWord;

static const ReservedWord reserved_words[] = {
    /* Statements - order matters for tokenization! */
    {"END",     TOK_END},
    {"FOR",     TOK_FOR},
    {"NEXT",    TOK_NEXT},
    {"DATA",    TOK_DATA},
    {"INPUT",   TOK_INPUT},
    {"DIM",     TOK_DIM},
    {"READ",    TOK_READ},
    {"LET",     TOK_LET},
    {"GOTO",    TOK_GOTO},
    {"RUN",     TOK_RUN},
    {"IF",      TOK_IF},
    {"RESTORE", TOK_RESTORE},
    {"GOSUB",   TOK_GOSUB},
    {"RETURN",  TOK_RETURN},
    {"REM",     TOK_REM},
    {"STOP",    TOK_STOP},
    {"ON",      TOK_ON},
    {"NULL",    TOK_NULL},
    {"WAIT",    TOK_WAIT},
    {"LOAD",    TOK_LOAD},
    {"SAVE",    TOK_SAVE},
    {"VERIFY",  TOK_VERIFY},
    {"DEF",     TOK_DEF},
    {"POKE",    TOK_POKE},
    {"PRINT",   TOK_PRINT},
    {"CONT",    TOK_CONT},
    {"LIST",    TOK_LIST},
    {"CLEAR",   TOK_CLEAR},
    {"GET",     TOK_GET},
    {"NEW",     TOK_NEW},

    /* Auxiliary keywords */
    {"TAB(",    TOK_TAB},
    {"TO",      TOK_TO},
    {"FN",      TOK_FN},
    {"SPC(",    TOK_SPC},
    {"THEN",    TOK_THEN},
    {"NOT",     TOK_NOT},
    {"STEP",    TOK_STEP},

    /* Operators (for completeness, though usually kept as characters) */
    {"AND",     TOK_AND},
    {"OR",      TOK_OR},

    /* Functions */
    {"SGN",     TOK_SGN},
    {"INT",     TOK_INT},
    {"ABS",     TOK_ABS},
    {"USR",     TOK_USR},
    {"FRE",     TOK_FRE},
    {"POS",     TOK_POS},
    {"SQR",     TOK_SQR},
    {"RND",     TOK_RND},
    {"LOG",     TOK_LOG},
    {"EXP",     TOK_EXP},
    {"COS",     TOK_COS},
    {"SIN",     TOK_SIN},
    {"TAN",     TOK_TAN},
    {"ATN",     TOK_ATN},
    {"PEEK",    TOK_PEEK},
    {"LEN",     TOK_LEN},
    {"STR$",    TOK_STR},
    {"VAL",     TOK_VAL},
    {"ASC",     TOK_ASC},
    {"CHR$",    TOK_CHR},
    {"LEFT$",   TOK_LEFT},
    {"RIGHT$",  TOK_RIGHT},
    {"MID$",    TOK_MID},

    /* End of table marker */
    {NULL,      TOK_LAST}
};

/*
 * Token to string mapping for detokenization
 */
static const char *token_strings[] = {
    /* Statements */
    "END",          /* TOK_END */
    "FOR",          /* TOK_FOR */
    "NEXT",         /* TOK_NEXT */
    "DATA",         /* TOK_DATA */
    "INPUT",        /* TOK_INPUT */
    "DIM",          /* TOK_DIM */
    "READ",         /* TOK_READ */
    "LET",          /* TOK_LET */
    "GOTO",         /* TOK_GOTO */
    "RUN",          /* TOK_RUN */
    "IF",           /* TOK_IF */
    "RESTORE",      /* TOK_RESTORE */
    "GOSUB",        /* TOK_GOSUB */
    "RETURN",       /* TOK_RETURN */
    "REM",          /* TOK_REM */
    "STOP",         /* TOK_STOP */
    "ON",           /* TOK_ON */
    "NULL",         /* TOK_NULL */
    "WAIT",         /* TOK_WAIT */
    "LOAD",         /* TOK_LOAD */
    "SAVE",         /* TOK_SAVE */
    "VERIFY",       /* TOK_VERIFY */
    "DEF",          /* TOK_DEF */
    "POKE",         /* TOK_POKE */
    "PRINT",        /* TOK_PRINT */
    "CONT",         /* TOK_CONT */
    "LIST",         /* TOK_LIST */
    "CLEAR",        /* TOK_CLEAR */
    "GET",          /* TOK_GET */
    "NEW",          /* TOK_NEW */

    /* Auxiliary */
    "TAB(",         /* TOK_TAB */
    "TO",           /* TOK_TO */
    "FN",           /* TOK_FN */
    "SPC(",         /* TOK_SPC */
    "THEN",         /* TOK_THEN */
    "NOT",          /* TOK_NOT */
    "STEP",         /* TOK_STEP */

    /* Operators */
    "+",            /* TOK_PLUS */
    "-",            /* TOK_MINUS */
    "*",            /* TOK_MULTIPLY */
    "/",            /* TOK_DIVIDE */
    "^",            /* TOK_POWER */
    "AND",          /* TOK_AND */
    "OR",           /* TOK_OR */
    ">",            /* TOK_GT */
    "=",            /* TOK_EQ */
    "<",            /* TOK_LT */

    /* Functions */
    "SGN",          /* TOK_SGN */
    "INT",          /* TOK_INT */
    "ABS",          /* TOK_ABS */
    "USR",          /* TOK_USR */
    "FRE",          /* TOK_FRE */
    "POS",          /* TOK_POS */
    "SQR",          /* TOK_SQR */
    "RND",          /* TOK_RND */
    "LOG",          /* TOK_LOG */
    "EXP",          /* TOK_EXP */
    "COS",          /* TOK_COS */
    "SIN",          /* TOK_SIN */
    "TAN",          /* TOK_TAN */
    "ATN",          /* TOK_ATN */
    "PEEK",         /* TOK_PEEK */
    "LEN",          /* TOK_LEN */
    "STR$",         /* TOK_STR */
    "VAL",          /* TOK_VAL */
    "ASC",          /* TOK_ASC */
    "CHR$",         /* TOK_CHR */
    "LEFT$",        /* TOK_LEFT */
    "RIGHT$",       /* TOK_RIGHT */
    "MID$",         /* TOK_MID */
};

/*
 * basic_tokenize - Convert BASIC source line to tokenized format
 *
 * This implements the CRUNCH routine from the 6502 assembly.
 * Reserved words are converted to single-byte tokens.
 * String literals are preserved unchanged.
 *
 * Parameters:
 *   line    - Source line to tokenize
 *   out_len - Receives the length of tokenized output
 *
 * Returns:
 *   Newly allocated string containing tokenized line, or NULL on error.
 *   Caller is responsible for freeing the returned string.
 */
char *basic_tokenize(const char *line, size_t *out_len)
{
    if (line == NULL || out_len == NULL) {
        return NULL;
    }

    /* Get input length and allocate buffers */
    size_t input_len = strlen(line);
    size_t max_len = input_len + 1;

    /* Make a safe copy of input to avoid issues with string literal memory */
    char *input_copy = malloc(max_len);
    if (input_copy == NULL) {
        return NULL;
    }
    memcpy(input_copy, line, max_len);

    char *output = malloc(max_len);
    if (output == NULL) {
        free(input_copy);
        return NULL;
    }

    const char *src = input_copy;
    char *dst = output;
    bool in_string = false;     /* Inside quoted string */
    bool in_data = false;       /* Inside DATA statement (don't tokenize) */
    bool in_rem = false;        /* Inside REM (comment) */

    while (*src != '\0') {
        /* Handle string literals - copy verbatim */
        if (*src == '"') {
            in_string = !in_string;
            *dst++ = *src++;
            continue;
        }

        /* Inside string literal - copy as-is */
        if (in_string) {
            *dst++ = *src++;
            continue;
        }

        /* Inside REM - copy rest of line as-is */
        if (in_rem) {
            *dst++ = *src++;
            continue;
        }

        /* Inside DATA - copy until colon or end of line */
        if (in_data) {
            if (*src == ':') {
                in_data = false;
            }
            *dst++ = *src++;
            continue;
        }

        /* Skip spaces (they're stripped in tokenized form) */
        /* Note: Original BASIC actually keeps some spaces for readability */
        if (*src == ' ') {
            *dst++ = *src++;
            continue;
        }

        /* Check for colon (statement separator) */
        if (*src == ':') {
            *dst++ = *src++;
            continue;
        }

        /* Check for reserved words */
        bool found_word = false;
        size_t remaining = strlen(src);  /* Length of remaining string to tokenize */

        for (const ReservedWord *rw = reserved_words; rw->word != NULL; rw++) {
            size_t len = strlen(rw->word);

            /* Skip if remaining string is shorter than keyword */
            if (remaining < len) {
                continue;
            }

            /* Case-insensitive comparison */
            bool match = true;
            for (size_t i = 0; i < len; i++) {
                char c1 = BASIC_TOUPPER(src[i]);
                char c2 = rw->word[i];
                if (c1 != c2) {
                    match = false;
                    break;
                }
            }

            if (match) {
                /* Make sure it's not part of a longer identifier */
                char next = src[len];
                bool is_word_boundary = !BASIC_IS_LETTER(next) && !BASIC_IS_DIGIT(next);

                /* Special case: words ending in ( don't need boundary check */
                if (rw->word[len - 1] == '(') {
                    is_word_boundary = true;
                }

                /* Special case: FN is always tokenized (user functions are FNA, FNB, etc.) */
                if (rw->token == TOK_FN) {
                    is_word_boundary = true;
                }

                if (is_word_boundary) {
                    /* Write token */
                    *dst++ = (char)rw->token;
                    src += len;
                    found_word = true;

                    /* Check for special statement modes */
                    if (rw->token == TOK_REM) {
                        in_rem = true;
                    } else if (rw->token == TOK_DATA) {
                        in_data = true;
                    }

                    break;
                }
            }
        }

        if (found_word) {
            continue;
        }

        /* Not a reserved word - copy character as-is */
        /* Convert to uppercase for consistency */
        char ch = *src++;
        *dst++ = BASIC_TOUPPER(ch);
    }

    /* Null-terminate */
    *dst = '\0';
    *out_len = (size_t)(dst - output);

    /* Free the input copy */
    free(input_copy);

    return output;
}

/*
 * basic_detokenize - Convert tokenized line back to readable text
 *
 * This implements the LIST routine's detokenization from 6502 assembly.
 *
 * Parameters:
 *   tokenized - Tokenized line data
 *   len       - Length of tokenized data
 *
 * Returns:
 *   Newly allocated string containing readable text, or NULL on error.
 *   Caller is responsible for freeing the returned string.
 */
char *basic_detokenize(const char *tokenized, size_t len)
{
    if (tokenized == NULL) {
        return NULL;
    }

    /* Allocate output buffer (tokens expand, so may need more space) */
    size_t max_len = len * 8 + 1;   /* Conservative estimate */
    char *output = malloc(max_len);
    if (output == NULL) {
        return NULL;
    }

    const char *src = tokenized;
    const char *end = tokenized + len;
    char *dst = output;
    bool in_string = false;

    while (src < end && *src != '\0') {
        unsigned char c = (unsigned char)*src;

        /* Handle string literals */
        if (c == '"') {
            in_string = !in_string;
            *dst++ = (char)c;
            src++;
            continue;
        }

        /* Inside string - copy as-is */
        if (in_string) {
            *dst++ = (char)c;
            src++;
            continue;
        }

        /* Check if it's a token */
        if (BASIC_IS_TOKEN(c)) {
            int token_index = c - TOK_END;

            /* Bounds check */
            if (token_index >= 0 && token_index < (int)(sizeof(token_strings) / sizeof(token_strings[0]))) {
                const char *word = token_strings[token_index];

                /* Copy the word */
                while (*word != '\0') {
                    *dst++ = *word++;
                }
            } else {
                /* Unknown token - output as hex */
                *dst++ = '?';
            }
            src++;
        } else {
            /* Regular character */
            *dst++ = (char)c;
            src++;
        }
    }

    *dst = '\0';
    return output;
}

/*
 * basic_token_name - Get the string name of a token
 *
 * Parameters:
 *   tok - Token value
 *
 * Returns:
 *   Pointer to static string with token name, or "?" for unknown tokens.
 */
const char *basic_token_name(Token tok)
{
    int index = (int)tok - TOK_END;

    if (index >= 0 && index < (int)(sizeof(token_strings) / sizeof(token_strings[0]))) {
        return token_strings[index];
    }

    return "?";
}

/*
 * =============================================================================
 * HELPER FUNCTIONS FOR PARSING
 * =============================================================================
 */

/*
 * basic_chrget - Get next character and advance text pointer
 *
 * This is the C equivalent of the 6502 CHRGET routine.
 * It skips spaces and returns the next significant character.
 *
 * Parameters:
 *   state - BASIC interpreter state
 *
 * Returns:
 *   Next character (or '\0' at end of line)
 */
char basic_chrget(BasicState *state)
{
    if (state == NULL || state->text_ptr == NULL) {
        return '\0';
    }

    /* Skip spaces */
    while (*state->text_ptr == ' ') {
        state->text_ptr++;
    }

    return *state->text_ptr++;
}

/*
 * basic_peek_char - Peek at next character without advancing
 *
 * Parameters:
 *   state - BASIC interpreter state
 *
 * Returns:
 *   Next character (or '\0' at end of line)
 */
char basic_peek_char(BasicState *state)
{
    if (state == NULL || state->text_ptr == NULL) {
        return '\0';
    }

    /* Skip spaces */
    const char *p = state->text_ptr;
    while (*p == ' ') {
        p++;
    }

    return *p;
}

/*
 * basic_skip_spaces - Skip whitespace characters
 *
 * Parameters:
 *   state - BASIC interpreter state
 */
void basic_skip_spaces(BasicState *state)
{
    if (state == NULL || state->text_ptr == NULL) {
        return;
    }

    while (*state->text_ptr == ' ' || *state->text_ptr == '\t') {
        state->text_ptr++;
    }
}

/*
 * basic_is_digit - Check if character is a digit
 *
 * Parameters:
 *   c - Character to check
 *
 * Returns:
 *   true if digit, false otherwise
 */
bool basic_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/*
 * basic_is_letter - Check if character is a letter
 *
 * Parameters:
 *   c - Character to check
 *
 * Returns:
 *   true if letter, false otherwise
 */
bool basic_is_letter(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/*
 * basic_is_end_of_statement - Check if character ends a statement
 *
 * Parameters:
 *   c - Character to check
 *
 * Returns:
 *   true if end of statement (colon, newline, or null), false otherwise
 */
bool basic_is_end_of_statement(char c)
{
    return c == ':' || c == '\0' || c == '\n' || c == '\r';
}
