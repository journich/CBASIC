/*
 * CBASIC - Built-in Functions Unit Tests
 *
 * Tests for mathematical and string functions.
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
extern void test_assert_str_eq(const char *expected, const char *actual, const char *test_name,
                               const char *file, int line);

#define ASSERT(cond) test_assert((cond), #cond, __FILE__, __LINE__)
#define ASSERT_INT_EQ(exp, act) test_assert_int_eq((exp), (act), #act " == " #exp, __FILE__, __LINE__)
#define ASSERT_DOUBLE_EQ(exp, act) test_assert_double_eq((exp), (act), 1e-9, #act " == " #exp, __FILE__, __LINE__)
#define ASSERT_DOUBLE_NEAR(exp, act, tol) test_assert_double_eq((exp), (act), (tol), #act " ~= " #exp, __FILE__, __LINE__)
#define ASSERT_NOT_NULL(ptr) test_assert((ptr) != NULL, #ptr " is not NULL", __FILE__, __LINE__)

/*
 * Test SGN function
 */
static void test_sgn(void)
{
    ASSERT_DOUBLE_EQ(1.0, basic_fn_sgn(5.0));
    ASSERT_DOUBLE_EQ(1.0, basic_fn_sgn(0.001));
    ASSERT_DOUBLE_EQ(-1.0, basic_fn_sgn(-5.0));
    ASSERT_DOUBLE_EQ(-1.0, basic_fn_sgn(-0.001));
    ASSERT_DOUBLE_EQ(0.0, basic_fn_sgn(0.0));
}

/*
 * Test INT function
 */
static void test_int(void)
{
    ASSERT_DOUBLE_EQ(3.0, basic_fn_int(3.0));
    ASSERT_DOUBLE_EQ(3.0, basic_fn_int(3.5));
    ASSERT_DOUBLE_EQ(3.0, basic_fn_int(3.9));
    ASSERT_DOUBLE_EQ(-4.0, basic_fn_int(-3.5));  /* Rounds toward -infinity */
    ASSERT_DOUBLE_EQ(-4.0, basic_fn_int(-3.1));
    ASSERT_DOUBLE_EQ(0.0, basic_fn_int(0.9));
    ASSERT_DOUBLE_EQ(-1.0, basic_fn_int(-0.1));
}

/*
 * Test ABS function
 */
static void test_abs(void)
{
    ASSERT_DOUBLE_EQ(5.0, basic_fn_abs(5.0));
    ASSERT_DOUBLE_EQ(5.0, basic_fn_abs(-5.0));
    ASSERT_DOUBLE_EQ(0.0, basic_fn_abs(0.0));
    ASSERT_DOUBLE_EQ(3.14, basic_fn_abs(-3.14));
    ASSERT_DOUBLE_EQ(3.14, basic_fn_abs(3.14));
}

/*
 * Test SQR function
 */
static void test_sqr(void)
{
    ASSERT_DOUBLE_EQ(3.0, basic_fn_sqr(9.0));
    ASSERT_DOUBLE_EQ(2.0, basic_fn_sqr(4.0));
    ASSERT_DOUBLE_EQ(0.0, basic_fn_sqr(0.0));
    ASSERT_DOUBLE_EQ(1.0, basic_fn_sqr(1.0));
    ASSERT_DOUBLE_NEAR(1.41421356, basic_fn_sqr(2.0), 1e-6);
    ASSERT_DOUBLE_EQ(10.0, basic_fn_sqr(100.0));
}

/*
 * Test LOG function
 */
static void test_log(void)
{
    ASSERT_DOUBLE_EQ(0.0, basic_fn_log(1.0));
    ASSERT_DOUBLE_EQ(1.0, basic_fn_log(exp(1.0)));
    ASSERT_DOUBLE_NEAR(2.302585, basic_fn_log(10.0), 1e-5);
    ASSERT_DOUBLE_NEAR(0.693147, basic_fn_log(2.0), 1e-5);
}

/*
 * Test EXP function
 */
static void test_exp(void)
{
    ASSERT_DOUBLE_EQ(1.0, basic_fn_exp(0.0));
    ASSERT_DOUBLE_NEAR(2.718281828, basic_fn_exp(1.0), 1e-6);
    ASSERT_DOUBLE_NEAR(7.389056, basic_fn_exp(2.0), 1e-5);
    ASSERT_DOUBLE_NEAR(0.367879, basic_fn_exp(-1.0), 1e-5);
}

/*
 * Test trigonometric functions
 */
static void test_trig(void)
{
    double pi = 3.14159265358979;

    /* SIN */
    ASSERT_DOUBLE_EQ(0.0, basic_fn_sin(0.0));
    ASSERT_DOUBLE_NEAR(1.0, basic_fn_sin(pi / 2), 1e-9);
    ASSERT_DOUBLE_NEAR(0.0, basic_fn_sin(pi), 1e-9);
    ASSERT_DOUBLE_NEAR(-1.0, basic_fn_sin(3 * pi / 2), 1e-9);

    /* COS */
    ASSERT_DOUBLE_EQ(1.0, basic_fn_cos(0.0));
    ASSERT_DOUBLE_NEAR(0.0, basic_fn_cos(pi / 2), 1e-9);
    ASSERT_DOUBLE_NEAR(-1.0, basic_fn_cos(pi), 1e-9);
    ASSERT_DOUBLE_NEAR(0.0, basic_fn_cos(3 * pi / 2), 1e-9);

    /* TAN */
    ASSERT_DOUBLE_EQ(0.0, basic_fn_tan(0.0));
    ASSERT_DOUBLE_NEAR(1.0, basic_fn_tan(pi / 4), 1e-9);

    /* ATN */
    ASSERT_DOUBLE_EQ(0.0, basic_fn_atn(0.0));
    ASSERT_DOUBLE_NEAR(pi / 4, basic_fn_atn(1.0), 1e-9);
    ASSERT_DOUBLE_NEAR(-pi / 4, basic_fn_atn(-1.0), 1e-9);
}

/*
 * Test RND function
 */
static void test_rnd(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* RND(1) should return values in [0, 1) */
    for (int i = 0; i < 100; i++) {
        double r = basic_fn_rnd(state, 1.0);
        ASSERT(r >= 0.0 && r < 1.0);
    }

    /* RND(-1) should seed and return new value */
    double r1 = basic_fn_rnd(state, -12345.0);
    ASSERT(r1 >= 0.0 && r1 < 1.0);

    /* Same seed should give same sequence */
    double r2 = basic_fn_rnd(state, -12345.0);
    ASSERT_DOUBLE_EQ(r1, r2);

    /* RND(0) should return last value */
    double last = basic_fn_rnd(state, 1.0);
    double same = basic_fn_rnd(state, 0.0);
    ASSERT_DOUBLE_EQ(last, same);

    basic_cleanup(state);
}

/*
 * Test VAL function
 */
static void test_val(void)
{
    ASSERT_DOUBLE_EQ(123.0, basic_fn_val("123"));
    ASSERT_DOUBLE_EQ(-456.0, basic_fn_val("-456"));
    ASSERT_DOUBLE_NEAR(3.14159, basic_fn_val("3.14159"), 1e-5);
    ASSERT_DOUBLE_EQ(0.0, basic_fn_val("ABC"));  /* Non-numeric returns 0 */
    ASSERT_DOUBLE_EQ(123.0, basic_fn_val("  123"));  /* Leading spaces */
    ASSERT_DOUBLE_EQ(123.0, basic_fn_val("123ABC"));  /* Stops at non-numeric */
    ASSERT_DOUBLE_EQ(1000.0, basic_fn_val("1E3"));  /* Scientific notation */
}

/*
 * Test LEN function
 */
static void test_len(void)
{
    StringDescriptor s;

    s.length = 0;
    s.data = "";
    ASSERT_INT_EQ(0, basic_fn_len(&s));

    s.length = 5;
    s.data = "HELLO";
    ASSERT_INT_EQ(5, basic_fn_len(&s));

    s.length = 13;
    s.data = "HELLO, WORLD!";
    ASSERT_INT_EQ(13, basic_fn_len(&s));
}

/*
 * Test ASC function
 */
static void test_asc(void)
{
    StringDescriptor s;

    s.length = 1;
    s.data = "A";
    ASSERT_INT_EQ(65, basic_fn_asc(&s));

    s.data = "a";
    ASSERT_INT_EQ(97, basic_fn_asc(&s));

    s.data = "0";
    ASSERT_INT_EQ(48, basic_fn_asc(&s));

    s.length = 5;
    s.data = "HELLO";
    ASSERT_INT_EQ(72, basic_fn_asc(&s));  /* 'H' */
}

/*
 * Test CHR$ function
 */
static void test_chr(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    StringDescriptor s;

    s = basic_fn_chr(state, 65);
    ASSERT_INT_EQ(1, s.length);
    ASSERT(s.data[0] == 'A');

    s = basic_fn_chr(state, 97);
    ASSERT_INT_EQ(1, s.length);
    ASSERT(s.data[0] == 'a');

    s = basic_fn_chr(state, 48);
    ASSERT_INT_EQ(1, s.length);
    ASSERT(s.data[0] == '0');

    basic_cleanup(state);
}

/*
 * Test LEFT$ function
 */
static void test_left(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    StringDescriptor src, result;
    src.length = 5;
    src.data = "HELLO";

    result = basic_fn_left(state, &src, 3);
    ASSERT_INT_EQ(3, result.length);
    ASSERT(strncmp(result.data, "HEL", 3) == 0);

    result = basic_fn_left(state, &src, 5);
    ASSERT_INT_EQ(5, result.length);
    ASSERT(strncmp(result.data, "HELLO", 5) == 0);

    result = basic_fn_left(state, &src, 10);  /* More than length */
    ASSERT_INT_EQ(5, result.length);

    result = basic_fn_left(state, &src, 0);
    ASSERT_INT_EQ(0, result.length);

    basic_cleanup(state);
}

/*
 * Test RIGHT$ function
 */
static void test_right(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    StringDescriptor src, result;
    src.length = 5;
    src.data = "HELLO";

    result = basic_fn_right(state, &src, 3);
    ASSERT_INT_EQ(3, result.length);
    ASSERT(strncmp(result.data, "LLO", 3) == 0);

    result = basic_fn_right(state, &src, 5);
    ASSERT_INT_EQ(5, result.length);
    ASSERT(strncmp(result.data, "HELLO", 5) == 0);

    result = basic_fn_right(state, &src, 10);  /* More than length */
    ASSERT_INT_EQ(5, result.length);

    result = basic_fn_right(state, &src, 0);
    ASSERT_INT_EQ(0, result.length);

    basic_cleanup(state);
}

/*
 * Test MID$ function
 */
static void test_mid(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    StringDescriptor src, result;
    src.length = 5;
    src.data = "HELLO";

    /* MID$(s, 2, 3) = "ELL" */
    result = basic_fn_mid(state, &src, 2, 3);
    ASSERT_INT_EQ(3, result.length);
    ASSERT(strncmp(result.data, "ELL", 3) == 0);

    /* MID$(s, 1, 5) = "HELLO" */
    result = basic_fn_mid(state, &src, 1, 5);
    ASSERT_INT_EQ(5, result.length);
    ASSERT(strncmp(result.data, "HELLO", 5) == 0);

    /* MID$(s, 3, 255) = "LLO" (rest of string) */
    result = basic_fn_mid(state, &src, 3, 255);
    ASSERT_INT_EQ(3, result.length);
    ASSERT(strncmp(result.data, "LLO", 3) == 0);

    /* MID$(s, 6, 1) = "" (past end) */
    result = basic_fn_mid(state, &src, 6, 1);
    ASSERT_INT_EQ(0, result.length);

    basic_cleanup(state);
}

/*
 * Test STR$ function
 */
static void test_str(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    StringDescriptor s;

    /* Positive integer */
    s = basic_fn_str(state, 123.0);
    ASSERT(s.length > 0);
    /* Should have leading space for positive numbers */
    ASSERT(s.data[0] == ' ');

    /* Negative number */
    s = basic_fn_str(state, -456.0);
    ASSERT(s.length > 0);
    ASSERT(s.data[0] == '-');

    /* Zero */
    s = basic_fn_str(state, 0.0);
    ASSERT(s.length > 0);

    basic_cleanup(state);
}

/*
 * Test PEEK and POKE
 */
static void test_peek_poke(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* POKE a value */
    state->memory[1000] = 42;
    ASSERT_INT_EQ(42, basic_fn_peek(state, 1000));

    /* POKE another value */
    state->memory[2000] = 255;
    ASSERT_INT_EQ(255, basic_fn_peek(state, 2000));

    /* Out of range should return 0 */
    ASSERT_INT_EQ(0, basic_fn_peek(state, -1));

    basic_cleanup(state);
}

/*
 * Test FRE and POS functions
 */
static void test_fre_pos(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* FRE should return a positive value */
    int32_t free_mem = basic_fn_fre(state, 0.0);
    ASSERT(free_mem > 0);

    /* POS should return cursor position (1-based) */
    state->terminal_pos = 0;
    ASSERT_INT_EQ(1, basic_fn_pos(state, 0.0));

    state->terminal_pos = 10;
    ASSERT_INT_EQ(11, basic_fn_pos(state, 0.0));

    basic_cleanup(state);
}

/*
 * Run all function tests
 */
void test_functions(void)
{
    test_suite("Built-in Functions");

    test_sgn();
    test_int();
    test_abs();
    test_sqr();
    test_log();
    test_exp();
    test_trig();
    test_rnd();
    test_val();
    test_len();
    test_asc();
    test_chr();
    test_left();
    test_right();
    test_mid();
    test_str();
    test_peek_poke();
    test_fre_pos();
}
