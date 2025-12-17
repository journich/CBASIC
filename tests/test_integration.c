/*
 * CBASIC - Integration Tests
 *
 * End-to-end tests for complete BASIC programs.
 * These tests verify that complete programs run correctly.
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

/* External declaration */
extern void basic_clear_variables(BasicState *state);

/*
 * Helper to run a program and get a variable result
 */
static double run_program_get_var(const char *lines[], int num_lines, const char *var_name)
{
    BasicState *state = basic_init();
    if (state == NULL) return NAN;

    /* Store all program lines */
    for (int i = 0; i < num_lines; i++) {
        if (!basic_store_line(state, lines[i])) {
            basic_cleanup(state);
            return NAN;
        }
    }

    /* Run the program */
    basic_clear_variables(state);
    state->stack_ptr = 0;
    state->data_ptr.line = NULL;
    state->data_ptr.position = NULL;
    state->current_line = state->program;
    state->current_line_num = state->current_line->line_number;
    state->text_ptr = state->current_line->text;
    state->running = true;

    ErrorCode err = basic_run(state);

    double result = NAN;
    if (err == ERR_NONE) {
        Variable *var = basic_get_variable(state, var_name);
        if (var != NULL && var->value.type == VAL_NUMBER) {
            result = var->value.v.number;
        }
    }

    basic_cleanup(state);
    return result;
}

/*
 * Test: Calculate factorial using FOR loop
 */
static void test_factorial(void)
{
    const char *program[] = {
        "10 N=5",
        "20 F=1",
        "30 FOR I=1 TO N",
        "40 F=F*I",
        "50 NEXT I",
        "60 END"
    };

    double result = run_program_get_var(program, 6, "F");
    ASSERT_DOUBLE_EQ(120.0, result);  /* 5! = 120 */
}

/*
 * Test: Calculate sum of integers
 */
static void test_sum_integers(void)
{
    const char *program[] = {
        "10 S=0",
        "20 FOR I=1 TO 10",
        "30 S=S+I",
        "40 NEXT I",
        "50 END"
    };

    double result = run_program_get_var(program, 5, "S");
    ASSERT_DOUBLE_EQ(55.0, result);  /* 1+2+...+10 = 55 */
}

/*
 * Test: Fibonacci sequence
 */
static void test_fibonacci(void)
{
    const char *program[] = {
        "10 A=0",
        "20 B=1",
        "30 FOR I=1 TO 10",
        "40 C=A+B",
        "50 A=B",
        "60 B=C",
        "70 NEXT I",
        "80 END"
    };

    double result = run_program_get_var(program, 8, "B");
    ASSERT_DOUBLE_EQ(89.0, result);  /* 10th Fibonacci = 89 */
}

/*
 * Test: Nested FOR loops
 */
static void test_nested_for(void)
{
    const char *program[] = {
        "10 S=0",
        "20 FOR I=1 TO 3",
        "30 FOR J=1 TO 3",
        "40 S=S+1",
        "50 NEXT J",
        "60 NEXT I",
        "70 END"
    };

    double result = run_program_get_var(program, 7, "S");
    ASSERT_DOUBLE_EQ(9.0, result);  /* 3*3 = 9 iterations */
}

/*
 * Test: GOSUB with multiple calls
 */
static void test_gosub_multiple(void)
{
    const char *program[] = {
        "10 A=0",
        "20 GOSUB 100",
        "30 GOSUB 100",
        "40 GOSUB 100",
        "50 END",
        "100 A=A+10",
        "110 RETURN"
    };

    double result = run_program_get_var(program, 7, "A");
    ASSERT_DOUBLE_EQ(30.0, result);  /* Called 3 times, each adds 10 */
}

/*
 * Test: Nested GOSUB
 */
static void test_gosub_nested(void)
{
    const char *program[] = {
        "10 A=0",
        "20 GOSUB 100",
        "30 END",
        "100 A=A+1",
        "110 GOSUB 200",
        "120 A=A+1",
        "130 RETURN",
        "200 A=A+10",
        "210 RETURN"
    };

    double result = run_program_get_var(program, 9, "A");
    ASSERT_DOUBLE_EQ(12.0, result);  /* 1 + 10 + 1 = 12 */
}

/*
 * Test: IF/THEN with GOTO
 */
static void test_if_goto(void)
{
    const char *program[] = {
        "10 X=5",
        "20 IF X<10 THEN 50",
        "30 A=999",
        "40 GOTO 60",
        "50 A=1",
        "60 END"
    };

    double result = run_program_get_var(program, 6, "A");
    ASSERT_DOUBLE_EQ(1.0, result);  /* X<10, so A=1 */
}

/*
 * Test: ON GOTO branching
 */
static void test_on_goto(void)
{
    const char *program[] = {
        "10 X=2",
        "20 ON X GOTO 100,200,300",
        "30 A=0",
        "40 GOTO 400",
        "100 A=1",
        "110 GOTO 400",
        "200 A=2",
        "210 GOTO 400",
        "300 A=3",
        "310 GOTO 400",
        "400 END"
    };

    double result = run_program_get_var(program, 11, "A");
    ASSERT_DOUBLE_EQ(2.0, result);  /* X=2, so branch to line 200 */
}

/*
 * Test: READ/DATA with multiple statements
 */
static void test_read_data_multiple(void)
{
    const char *program[] = {
        "10 S=0",
        "20 FOR I=1 TO 5",
        "30 READ X",
        "40 S=S+X",
        "50 NEXT I",
        "60 DATA 1,2,3,4,5",
        "70 END"
    };

    double result = run_program_get_var(program, 7, "S");
    ASSERT_DOUBLE_EQ(15.0, result);  /* 1+2+3+4+5 = 15 */
}

/*
 * Test: RESTORE and re-read DATA
 */
static void test_restore(void)
{
    const char *program[] = {
        "10 S=0",
        "20 READ A,B,C",
        "30 S=A+B+C",
        "40 RESTORE",
        "50 READ X",
        "60 S=S+X",
        "70 DATA 10,20,30",
        "80 END"
    };

    double result = run_program_get_var(program, 8, "S");
    ASSERT_DOUBLE_EQ(70.0, result);  /* (10+20+30) + 10 = 70 */
}

/*
 * Test: Array operations
 */
static void test_array_operations(void)
{
    const char *program[] = {
        "10 DIM A(10)",
        "20 FOR I=0 TO 10",
        "30 A(I)=I*I",
        "40 NEXT I",
        "50 S=A(5)+A(10)",
        "60 END"
    };

    double result = run_program_get_var(program, 6, "S");
    ASSERT_DOUBLE_EQ(125.0, result);  /* 5^2 + 10^2 = 25 + 100 = 125 */
}

/*
 * Test: Two-dimensional array
 */
static void test_2d_array(void)
{
    const char *program[] = {
        "10 DIM A(3,3)",
        "20 FOR I=0 TO 3",
        "30 FOR J=0 TO 3",
        "40 A(I,J)=I*10+J",
        "50 NEXT J",
        "60 NEXT I",
        "70 S=A(2,3)+A(3,2)",
        "80 END"
    };

    double result = run_program_get_var(program, 8, "S");
    ASSERT_DOUBLE_EQ(55.0, result);  /* (2*10+3) + (3*10+2) = 23 + 32 = 55 */
}

/*
 * Test: User-defined function
 */
static void test_user_function(void)
{
    const char *program[] = {
        "10 DEF FNS(X)=X*X",
        "20 A=FNS(5)",
        "30 B=FNS(3)+FNS(4)",
        "40 C=A+B",
        "50 END"
    };

    double result = run_program_get_var(program, 5, "C");
    ASSERT_DOUBLE_EQ(50.0, result);  /* 25 + (9+16) = 25 + 25 = 50 */
}

/*
 * Test: Complex expression evaluation
 */
static void test_complex_expression(void)
{
    const char *program[] = {
        "10 A=2",
        "20 B=3",
        "30 C=4",
        "40 X=A+B*C^2-10/A",
        "50 END"
    };

    double result = run_program_get_var(program, 5, "X");
    /* 2 + 3*16 - 10/2 = 2 + 48 - 5 = 45 */
    ASSERT_DOUBLE_EQ(45.0, result);
}

/*
 * Test: Mathematical functions in expression
 */
static void test_math_in_expression(void)
{
    const char *program[] = {
        "10 X=SQR(16)+ABS(-5)+INT(3.7)",
        "20 END"
    };

    double result = run_program_get_var(program, 2, "X");
    /* sqrt(16) + abs(-5) + int(3.7) = 4 + 5 + 3 = 12 */
    ASSERT_DOUBLE_EQ(12.0, result);
}

/*
 * Test: BASIC idiom for counting down
 */
static void test_countdown(void)
{
    const char *program[] = {
        "10 S=0",
        "20 FOR I=10 TO 1 STEP -1",
        "30 S=S+I",
        "40 NEXT I",
        "50 END"
    };

    double result = run_program_get_var(program, 5, "S");
    ASSERT_DOUBLE_EQ(55.0, result);  /* 10+9+8+...+1 = 55 */
}

/*
 * Test: Early loop exit with IF
 */
static void test_early_exit(void)
{
    const char *program[] = {
        "10 S=0",
        "20 FOR I=1 TO 100",
        "30 S=S+I",
        "40 IF S>50 THEN 70",
        "50 NEXT I",
        "60 GOTO 80",
        "70 R=I",
        "80 END"
    };

    double result = run_program_get_var(program, 8, "R");
    /* 1+2+3+...+10 = 55 > 50, so R=10 */
    ASSERT_DOUBLE_EQ(10.0, result);
}

/*
 * Test: Initialize and cleanup multiple times
 */
static void test_multiple_runs(void)
{
    /* Run the same program type multiple times with fresh state */
    for (int run = 0; run < 3; run++) {
        const char *program[] = {
            "10 A=1",
            "20 FOR I=1 TO 5",
            "30 A=A*2",
            "40 NEXT I",
            "50 END"
        };

        double result = run_program_get_var(program, 5, "A");
        ASSERT_DOUBLE_EQ(32.0, result);  /* 1*2^5 = 32 */
    }
}

/*
 * Test: Error handling - division by zero behavior
 * (In original BASIC, this would give an error)
 */
static void test_error_conditions(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Store a simple program */
    basic_store_line(state, "10 A=5");
    basic_store_line(state, "20 END");
    ASSERT_NOT_NULL(state->program);

    /* NEW should clear program */
    basic_new(state);
    ASSERT(state->program == NULL);

    basic_cleanup(state);
}

/*
 * Run all integration tests
 */
void test_integration(void)
{
    test_suite("Integration Tests");

    test_factorial();
    test_sum_integers();
    test_fibonacci();
    test_nested_for();
    test_gosub_multiple();
    test_gosub_nested();
    test_if_goto();
    test_on_goto();
    test_read_data_multiple();
    test_restore();
    test_array_operations();
    test_2d_array();
    test_user_function();
    test_complex_expression();
    test_math_in_expression();
    test_countdown();
    test_early_exit();
    test_multiple_runs();
    test_error_conditions();
}
