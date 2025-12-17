/*
 * CBASIC - Expression Evaluator Unit Tests
 *
 * Tests for the expression evaluation module.
 *
 * Copyright (c) 2025 Tim Buchalka
 * Licensed under the MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "basic.h"

/* Test framework functions */
extern void test_suite(const char *name);
extern void test_assert(int condition, const char *test_name, const char *file, int line);
extern void test_assert_int_eq(int expected, int actual, const char *test_name, const char *file, int line);
extern void test_assert_double_eq(double expected, double actual, double tolerance,
                                   const char *test_name, const char *file, int line);

#define ASSERT(cond) test_assert((cond), #cond, __FILE__, __LINE__)
#define ASSERT_INT_EQ(exp, act) test_assert_int_eq((exp), (act), #act " == " #exp, __FILE__, __LINE__)
#define ASSERT_DOUBLE_EQ(exp, act) test_assert_double_eq((exp), (act), 1e-9, #act " == " #exp, __FILE__, __LINE__)
#define ASSERT_DOUBLE_NEAR(exp, act, tol) test_assert_double_eq((exp), (act), (tol), #act " ~= " #exp, __FILE__, __LINE__)
#define ASSERT_NOT_NULL(ptr) test_assert((ptr) != NULL, #ptr " is not NULL", __FILE__, __LINE__)

/* Helper to evaluate numeric expression */
static double eval_numeric(BasicState *state, const char *expr)
{
    size_t len;
    char *tokenized = basic_tokenize(expr, &len);
    if (tokenized == NULL) return NAN;

    state->text_ptr = tokenized;
    double result;
    ErrorCode err = basic_eval_numeric(state, &result);
    free(tokenized);

    if (err != ERR_NONE) return NAN;
    return result;
}

/*
 * Test basic arithmetic
 */
static void test_basic_arithmetic(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Addition */
    ASSERT_DOUBLE_EQ(5.0, eval_numeric(state, "2+3"));
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "5+-5"));

    /* Subtraction */
    ASSERT_DOUBLE_EQ(2.0, eval_numeric(state, "5-3"));
    ASSERT_DOUBLE_EQ(-3.0, eval_numeric(state, "2-5"));

    /* Multiplication */
    ASSERT_DOUBLE_EQ(6.0, eval_numeric(state, "2*3"));
    ASSERT_DOUBLE_EQ(-15.0, eval_numeric(state, "3*-5"));

    /* Division */
    ASSERT_DOUBLE_EQ(2.0, eval_numeric(state, "6/3"));
    ASSERT_DOUBLE_EQ(0.5, eval_numeric(state, "1/2"));

    /* Exponentiation */
    ASSERT_DOUBLE_EQ(8.0, eval_numeric(state, "2^3"));
    ASSERT_DOUBLE_EQ(0.5, eval_numeric(state, "2^-1"));

    basic_cleanup(state);
}

/*
 * Test operator precedence
 */
static void test_operator_precedence(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Multiplication before addition */
    ASSERT_DOUBLE_EQ(7.0, eval_numeric(state, "1+2*3"));
    ASSERT_DOUBLE_EQ(9.0, eval_numeric(state, "1*2+3+4"));

    /* Exponentiation before multiplication */
    ASSERT_DOUBLE_EQ(16.0, eval_numeric(state, "2*2^3"));
    ASSERT_DOUBLE_EQ(32.0, eval_numeric(state, "2^3*4"));

    /* Parentheses override precedence */
    ASSERT_DOUBLE_EQ(9.0, eval_numeric(state, "(1+2)*3"));
    ASSERT_DOUBLE_EQ(64.0, eval_numeric(state, "(2*2)^3"));

    /* Complex expression */
    ASSERT_DOUBLE_EQ(14.0, eval_numeric(state, "2+3*4"));
    ASSERT_DOUBLE_EQ(20.0, eval_numeric(state, "(2+3)*4"));
    ASSERT_DOUBLE_EQ(11.0, eval_numeric(state, "2*3+5"));
    ASSERT_DOUBLE_EQ(17.0, eval_numeric(state, "2+3*5"));

    basic_cleanup(state);
}

/*
 * Test unary operators
 */
static void test_unary_operators(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Unary minus */
    ASSERT_DOUBLE_EQ(-5.0, eval_numeric(state, "-5"));
    ASSERT_DOUBLE_EQ(-5.0, eval_numeric(state, "-(2+3)"));
    ASSERT_DOUBLE_EQ(3.0, eval_numeric(state, "--3"));

    /* Unary plus */
    ASSERT_DOUBLE_EQ(5.0, eval_numeric(state, "+5"));

    basic_cleanup(state);
}

/*
 * Test comparison operators
 */
static void test_comparison_operators(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Equal */
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "5=5"));
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "5=3"));

    /* Not equal */
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "5<>3"));
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "5<>5"));

    /* Less than */
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "3<5"));
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "5<3"));
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "5<5"));

    /* Greater than */
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "5>3"));
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "3>5"));
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "5>5"));

    /* Less than or equal */
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "3<=5"));
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "5<=5"));
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "5<=3"));

    /* Greater than or equal */
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "5>=3"));
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "5>=5"));
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "3>=5"));

    basic_cleanup(state);
}

/*
 * Test logical operators
 */
static void test_logical_operators(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* AND (bitwise in BASIC) */
    /* -1 AND -1 = -1 (true AND true = true) */
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "-1 AND -1"));
    /* -1 AND 0 = 0 (true AND false = false) */
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "-1 AND 0"));
    /* 0 AND 0 = 0 */
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "0 AND 0"));

    /* OR (bitwise in BASIC) */
    /* -1 OR -1 = -1 */
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "-1 OR -1"));
    /* -1 OR 0 = -1 */
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "-1 OR 0"));
    /* 0 OR 0 = 0 */
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "0 OR 0"));

    /* NOT (bitwise complement) */
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "NOT -1"));
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "NOT 0"));

    basic_cleanup(state);
}

/*
 * Test variables in expressions
 */
static void test_variables_in_expressions(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Set up some variables */
    Value val;
    val.type = VAL_NUMBER;

    val.v.number = 10.0;
    basic_set_variable(state, "A", &val);

    val.v.number = 5.0;
    basic_set_variable(state, "B", &val);

    val.v.number = 2.0;
    basic_set_variable(state, "C", &val);

    /* Evaluate expressions with variables */
    ASSERT_DOUBLE_EQ(15.0, eval_numeric(state, "A+B"));
    ASSERT_DOUBLE_EQ(5.0, eval_numeric(state, "A-B"));
    ASSERT_DOUBLE_EQ(50.0, eval_numeric(state, "A*B"));
    ASSERT_DOUBLE_EQ(2.0, eval_numeric(state, "A/B"));
    ASSERT_DOUBLE_EQ(100.0, eval_numeric(state, "A^C"));
    ASSERT_DOUBLE_EQ(17.0, eval_numeric(state, "A+B+C"));
    ASSERT_DOUBLE_EQ(20.0, eval_numeric(state, "A*C"));

    basic_cleanup(state);
}

/*
 * Test numeric constants
 */
static void test_numeric_constants(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Integers */
    ASSERT_DOUBLE_EQ(42.0, eval_numeric(state, "42"));
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "0"));

    /* Decimals */
    ASSERT_DOUBLE_NEAR(3.14159, eval_numeric(state, "3.14159"), 1e-5);
    ASSERT_DOUBLE_EQ(0.5, eval_numeric(state, ".5"));

    /* Scientific notation */
    ASSERT_DOUBLE_EQ(1000.0, eval_numeric(state, "1E3"));
    ASSERT_DOUBLE_EQ(0.001, eval_numeric(state, "1E-3"));
    ASSERT_DOUBLE_EQ(1500.0, eval_numeric(state, "1.5E3"));

    basic_cleanup(state);
}

/*
 * Test parentheses
 */
static void test_parentheses(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Simple parentheses */
    ASSERT_DOUBLE_EQ(5.0, eval_numeric(state, "(5)"));
    ASSERT_DOUBLE_EQ(15.0, eval_numeric(state, "(5+10)"));

    /* Nested parentheses */
    ASSERT_DOUBLE_EQ(20.0, eval_numeric(state, "((2+3)*4)"));
    ASSERT_DOUBLE_EQ(14.0, eval_numeric(state, "(2+(3*4))"));
    ASSERT_DOUBLE_EQ(36.0, eval_numeric(state, "((1+2)*(3+4)+5*3)"));

    basic_cleanup(state);
}

/*
 * Test built-in function calls in expressions
 */
static void test_function_calls(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* ABS */
    ASSERT_DOUBLE_EQ(5.0, eval_numeric(state, "ABS(-5)"));
    ASSERT_DOUBLE_EQ(5.0, eval_numeric(state, "ABS(5)"));

    /* INT */
    ASSERT_DOUBLE_EQ(3.0, eval_numeric(state, "INT(3.7)"));
    ASSERT_DOUBLE_EQ(-4.0, eval_numeric(state, "INT(-3.7)"));

    /* SGN */
    ASSERT_DOUBLE_EQ(1.0, eval_numeric(state, "SGN(5)"));
    ASSERT_DOUBLE_EQ(-1.0, eval_numeric(state, "SGN(-5)"));
    ASSERT_DOUBLE_EQ(0.0, eval_numeric(state, "SGN(0)"));

    /* SQR */
    ASSERT_DOUBLE_EQ(3.0, eval_numeric(state, "SQR(9)"));
    ASSERT_DOUBLE_EQ(2.0, eval_numeric(state, "SQR(4)"));

    /* Nested function calls */
    /* INT rounds towards negative infinity (floor), so INT(-4.5) = -5, ABS(-5) = 5 */
    ASSERT_DOUBLE_EQ(5.0, eval_numeric(state, "ABS(INT(-4.5))"));
    ASSERT_DOUBLE_EQ(3.0, eval_numeric(state, "SQR(ABS(-9))"));

    basic_cleanup(state);
}

/*
 * Run all expression tests
 */
void test_expression(void)
{
    test_suite("Expression Evaluator");

    test_basic_arithmetic();
    test_operator_precedence();
    test_unary_operators();
    test_comparison_operators();
    test_logical_operators();
    test_variables_in_expressions();
    test_numeric_constants();
    test_parentheses();
    test_function_calls();
}
