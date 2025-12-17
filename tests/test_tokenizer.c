/*
 * CBASIC - Tokenizer Unit Tests
 *
 * Tests for the tokenizer/lexer module.
 *
 * Copyright (c) 2025 Tim Buchalka
 * Licensed under the MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "basic.h"

/* Test framework functions */
extern void test_suite(const char *name);
extern void test_assert(int condition, const char *test_name, const char *file, int line);
extern void test_assert_str_eq(const char *expected, const char *actual, const char *test_name,
                               const char *file, int line);

#define ASSERT(cond) test_assert((cond), #cond, __FILE__, __LINE__)
#define ASSERT_STR_EQ(exp, act) test_assert_str_eq((exp), (act), #act " == " #exp, __FILE__, __LINE__)
#define ASSERT_NOT_NULL(ptr) test_assert((ptr) != NULL, #ptr " is not NULL", __FILE__, __LINE__)

/*
 * Test tokenization of reserved words
 */
static void test_tokenize_keywords(void)
{
    size_t len;
    char *tokenized;
    char *detokenized;

    /* Test PRINT */
    tokenized = basic_tokenize("PRINT", &len);
    ASSERT_NOT_NULL(tokenized);
    ASSERT(len == 1);
    ASSERT((unsigned char)tokenized[0] == TOK_PRINT);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("PRINT", detokenized);
    free(tokenized);
    free(detokenized);

    /* Test GOTO */
    tokenized = basic_tokenize("GOTO 100", &len);
    ASSERT_NOT_NULL(tokenized);
    ASSERT((unsigned char)tokenized[0] == TOK_GOTO);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("GOTO 100", detokenized);
    free(tokenized);
    free(detokenized);

    /* Test FOR...NEXT */
    tokenized = basic_tokenize("FOR I=1 TO 10 STEP 2", &len);
    ASSERT_NOT_NULL(tokenized);
    /* Should contain FOR, =, TO, STEP tokens */
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("FOR I=1 TO 10 STEP 2", detokenized);
    free(tokenized);
    free(detokenized);

    /* Test IF...THEN */
    tokenized = basic_tokenize("IF X>5 THEN 200", &len);
    ASSERT_NOT_NULL(tokenized);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("IF X>5 THEN 200", detokenized);
    free(tokenized);
    free(detokenized);
}

/*
 * Test that strings are preserved during tokenization
 */
static void test_tokenize_strings(void)
{
    size_t len;
    char *tokenized;
    char *detokenized;

    /* String with keywords inside - should NOT be tokenized */
    tokenized = basic_tokenize("PRINT \"HELLO WORLD\"", &len);
    ASSERT_NOT_NULL(tokenized);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("PRINT \"HELLO WORLD\"", detokenized);
    free(tokenized);
    free(detokenized);

    /* String containing FOR and NEXT keywords */
    tokenized = basic_tokenize("A$=\"FOR NEXT GOTO\"", &len);
    ASSERT_NOT_NULL(tokenized);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("A$=\"FOR NEXT GOTO\"", detokenized);
    free(tokenized);
    free(detokenized);
}

/*
 * Test DATA statement (contents should not be tokenized)
 */
static void test_tokenize_data(void)
{
    size_t len;
    char *tokenized;
    char *detokenized;

    /* DATA with various values */
    tokenized = basic_tokenize("DATA 1,2,3,\"HELLO\"", &len);
    ASSERT_NOT_NULL(tokenized);
    /* DATA token at start, rest preserved */
    ASSERT((unsigned char)tokenized[0] == TOK_DATA);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("DATA 1,2,3,\"HELLO\"", detokenized);
    free(tokenized);
    free(detokenized);
}

/*
 * Test REM statement (comment - should not be tokenized after REM)
 */
static void test_tokenize_rem(void)
{
    size_t len;
    char *tokenized;
    char *detokenized;

    /* REM with text */
    tokenized = basic_tokenize("REM THIS IS A COMMENT", &len);
    ASSERT_NOT_NULL(tokenized);
    ASSERT((unsigned char)tokenized[0] == TOK_REM);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("REM THIS IS A COMMENT", detokenized);
    free(tokenized);
    free(detokenized);

    /* REM with keywords that should not be tokenized */
    tokenized = basic_tokenize("REM PRINT GOTO FOR", &len);
    ASSERT_NOT_NULL(tokenized);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("REM PRINT GOTO FOR", detokenized);
    free(tokenized);
    free(detokenized);
}

/*
 * Test function tokenization
 */
static void test_tokenize_functions(void)
{
    size_t len;
    char *tokenized;
    char *detokenized;

    /* Math functions */
    tokenized = basic_tokenize("X=SIN(Y)", &len);
    ASSERT_NOT_NULL(tokenized);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("X=SIN(Y)", detokenized);
    free(tokenized);
    free(detokenized);

    /* String functions */
    tokenized = basic_tokenize("A$=LEFT$(B$,5)", &len);
    ASSERT_NOT_NULL(tokenized);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("A$=LEFT$(B$,5)", detokenized);
    free(tokenized);
    free(detokenized);

    /* Multiple functions */
    tokenized = basic_tokenize("PRINT ABS(X);SQR(Y);INT(Z)", &len);
    ASSERT_NOT_NULL(tokenized);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("PRINT ABS(X);SQR(Y);INT(Z)", detokenized);
    free(tokenized);
    free(detokenized);
}

/*
 * Test case insensitivity
 */
static void test_tokenize_case_insensitive(void)
{
    size_t len1, len2;
    char *tok1, *tok2;

    /* Uppercase and lowercase should produce same tokens */
    tok1 = basic_tokenize("PRINT", &len1);
    tok2 = basic_tokenize("print", &len2);
    ASSERT_NOT_NULL(tok1);
    ASSERT_NOT_NULL(tok2);
    ASSERT(len1 == len2);
    ASSERT((unsigned char)tok1[0] == TOK_PRINT);
    ASSERT((unsigned char)tok2[0] == TOK_PRINT);
    free(tok1);
    free(tok2);

    /* Mixed case */
    tok1 = basic_tokenize("GoTo", &len1);
    ASSERT_NOT_NULL(tok1);
    ASSERT((unsigned char)tok1[0] == TOK_GOTO);
    free(tok1);
}

/*
 * Test complex expressions
 */
static void test_tokenize_expressions(void)
{
    size_t len;
    char *tokenized;
    char *detokenized;

    /* Complex arithmetic */
    tokenized = basic_tokenize("X=(A+B)*C/D^E", &len);
    ASSERT_NOT_NULL(tokenized);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("X=(A+B)*C/D^E", detokenized);
    free(tokenized);
    free(detokenized);

    /* Comparison with AND/OR */
    tokenized = basic_tokenize("IF X>5 AND Y<10 THEN 100", &len);
    ASSERT_NOT_NULL(tokenized);
    detokenized = basic_detokenize(tokenized, len);
    ASSERT_STR_EQ("IF X>5 AND Y<10 THEN 100", detokenized);
    free(tokenized);
    free(detokenized);
}

/*
 * Test token_name function
 */
static void test_token_names(void)
{
    ASSERT_STR_EQ("PRINT", basic_token_name(TOK_PRINT));
    ASSERT_STR_EQ("GOTO", basic_token_name(TOK_GOTO));
    ASSERT_STR_EQ("FOR", basic_token_name(TOK_FOR));
    ASSERT_STR_EQ("NEXT", basic_token_name(TOK_NEXT));
    ASSERT_STR_EQ("IF", basic_token_name(TOK_IF));
    ASSERT_STR_EQ("THEN", basic_token_name(TOK_THEN));
    ASSERT_STR_EQ("SIN", basic_token_name(TOK_SIN));
    ASSERT_STR_EQ("LEFT$", basic_token_name(TOK_LEFT));
}

/*
 * Run all tokenizer tests
 */
void test_tokenizer(void)
{
    test_suite("Tokenizer");

    test_tokenize_keywords();
    test_tokenize_strings();
    test_tokenize_data();
    test_tokenize_rem();
    test_tokenize_functions();
    test_tokenize_case_insensitive();
    test_tokenize_expressions();
    test_token_names();
}
