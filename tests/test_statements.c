/*
 * CBASIC - Statement Unit Tests
 *
 * Tests for BASIC statement execution.
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
extern void test_assert_int_eq(int expected, int actual, const char *test_name, const char *file, int line);
extern void test_assert_double_eq(double expected, double actual, double tolerance,
                                   const char *test_name, const char *file, int line);

#define ASSERT(cond) test_assert((cond), #cond, __FILE__, __LINE__)
#define ASSERT_INT_EQ(exp, act) test_assert_int_eq((exp), (act), #act " == " #exp, __FILE__, __LINE__)
#define ASSERT_DOUBLE_EQ(exp, act) test_assert_double_eq((exp), (act), 1e-9, #act " == " #exp, __FILE__, __LINE__)
#define ASSERT_NOT_NULL(ptr) test_assert((ptr) != NULL, #ptr " is not NULL", __FILE__, __LINE__)

/* External declaration */
extern void basic_clear_variables(BasicState *state);

/*
 * Helper to execute a line and check result
 */
static ErrorCode exec_line(BasicState *state, const char *line)
{
    return basic_execute_line(state, line);
}

/*
 * Helper to get variable value
 */
static double get_var(BasicState *state, const char *name)
{
    Variable *var = basic_get_variable(state, name);
    if (var == NULL) return 0.0;
    return var->value.v.number;
}

/*
 * Test LET statement
 */
static void test_let_statement(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Explicit LET */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "LET A=5"));
    ASSERT_DOUBLE_EQ(5.0, get_var(state, "A"));

    /* Implicit LET */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "B=10"));
    ASSERT_DOUBLE_EQ(10.0, get_var(state, "B"));

    /* Expression assignment */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "C=A+B"));
    ASSERT_DOUBLE_EQ(15.0, get_var(state, "C"));

    /* Multiple character variable (only first 2 significant) */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "SCORE=100"));
    ASSERT_DOUBLE_EQ(100.0, get_var(state, "SC"));
    ASSERT_DOUBLE_EQ(100.0, get_var(state, "SCORE"));
    ASSERT_DOUBLE_EQ(100.0, get_var(state, "SCOREBOARD"));  /* Same as SC */

    basic_cleanup(state);
}

/*
 * Test program line storage
 */
static void test_line_storage(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Store lines */
    ASSERT(basic_store_line(state, "10 PRINT \"HELLO\""));
    ASSERT(basic_store_line(state, "20 PRINT \"WORLD\""));
    ASSERT(basic_store_line(state, "30 END"));

    /* Verify lines are stored */
    ASSERT_NOT_NULL(basic_find_line(state, 10));
    ASSERT_NOT_NULL(basic_find_line(state, 20));
    ASSERT_NOT_NULL(basic_find_line(state, 30));
    ASSERT(basic_find_line(state, 15) == NULL);

    /* Delete a line */
    basic_delete_line(state, 20);
    ASSERT(basic_find_line(state, 20) == NULL);
    ASSERT_NOT_NULL(basic_find_line(state, 10));
    ASSERT_NOT_NULL(basic_find_line(state, 30));

    /* Replace a line */
    ASSERT(basic_store_line(state, "10 REM REPLACED"));
    ProgramLine *line = basic_find_line(state, 10);
    ASSERT_NOT_NULL(line);
    /* The content should be tokenized, so we can't easily verify it here */

    basic_cleanup(state);
}

/*
 * Test DIM statement
 */
static void test_dim_statement(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Simple array */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "DIM A(10)"));
    Array *arr = basic_get_array(state, "A");
    ASSERT_NOT_NULL(arr);
    ASSERT_INT_EQ(1, arr->num_dims);
    ASSERT_INT_EQ(11, arr->dims[0].size);  /* 0-10 = 11 elements */

    /* Multi-dimensional array */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "DIM B(5,5)"));
    arr = basic_get_array(state, "B");
    ASSERT_NOT_NULL(arr);
    ASSERT_INT_EQ(2, arr->num_dims);
    ASSERT_INT_EQ(6, arr->dims[0].size);
    ASSERT_INT_EQ(6, arr->dims[1].size);

    /* String array */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "DIM C$(20)"));
    arr = basic_get_array(state, "C$");
    ASSERT_NOT_NULL(arr);
    ASSERT(arr->name.is_string);

    /* Redimension should fail */
    ASSERT_INT_EQ(ERR_DD, exec_line(state, "DIM A(20)"));

    basic_cleanup(state);
}

/*
 * Test array element access
 */
static void test_array_access(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Create and access array */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "DIM A(10)"));
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "A(5)=42"));
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "X=A(5)"));
    ASSERT_DOUBLE_EQ(42.0, get_var(state, "X"));

    /* Array in expression */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "A(1)=10"));
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "A(2)=20"));
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "Y=A(1)+A(2)"));
    ASSERT_DOUBLE_EQ(30.0, get_var(state, "Y"));

    /* Multi-dimensional array */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "DIM B(3,3)"));
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "B(1,2)=99"));
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "Z=B(1,2)"));
    ASSERT_DOUBLE_EQ(99.0, get_var(state, "Z"));

    /* Auto-dimensioned array (default 0-10) */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "C(5)=55"));
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "W=C(5)"));
    ASSERT_DOUBLE_EQ(55.0, get_var(state, "W"));

    basic_cleanup(state);
}

/*
 * Test FOR/NEXT loops
 */
static void test_for_next(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Simple loop that sums numbers */
    ASSERT(basic_store_line(state, "10 S=0"));
    ASSERT(basic_store_line(state, "20 FOR I=1 TO 5"));
    ASSERT(basic_store_line(state, "30 S=S+I"));
    ASSERT(basic_store_line(state, "40 NEXT I"));
    ASSERT(basic_store_line(state, "50 END"));

    basic_clear_variables(state);
    state->stack_ptr = 0;
    state->current_line = state->program;
    state->current_line_num = state->current_line->line_number;
    state->text_ptr = state->current_line->text;
    state->running = true;

    ErrorCode err = basic_run(state);
    ASSERT_INT_EQ(ERR_NONE, err);
    ASSERT_DOUBLE_EQ(15.0, get_var(state, "S"));  /* 1+2+3+4+5 = 15 */
    ASSERT_DOUBLE_EQ(6.0, get_var(state, "I"));   /* Loop variable ends at 6 */

    basic_cleanup(state);
}

/*
 * Test FOR/NEXT with STEP
 */
static void test_for_next_step(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Loop with step 2 */
    ASSERT(basic_store_line(state, "10 S=0"));
    ASSERT(basic_store_line(state, "20 FOR I=1 TO 10 STEP 2"));
    ASSERT(basic_store_line(state, "30 S=S+1"));
    ASSERT(basic_store_line(state, "40 NEXT I"));
    ASSERT(basic_store_line(state, "50 END"));

    basic_clear_variables(state);
    state->stack_ptr = 0;
    state->current_line = state->program;
    state->current_line_num = state->current_line->line_number;
    state->text_ptr = state->current_line->text;
    state->running = true;

    ErrorCode err = basic_run(state);
    ASSERT_INT_EQ(ERR_NONE, err);
    ASSERT_DOUBLE_EQ(5.0, get_var(state, "S"));  /* 1,3,5,7,9 = 5 iterations */

    basic_cleanup(state);
}

/*
 * Test GOTO statement
 */
static void test_goto(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* GOTO should skip line 20 */
    ASSERT(basic_store_line(state, "10 A=1"));
    ASSERT(basic_store_line(state, "15 GOTO 30"));
    ASSERT(basic_store_line(state, "20 A=999"));
    ASSERT(basic_store_line(state, "30 END"));

    basic_clear_variables(state);
    state->stack_ptr = 0;
    state->current_line = state->program;
    state->current_line_num = state->current_line->line_number;
    state->text_ptr = state->current_line->text;
    state->running = true;

    ErrorCode err = basic_run(state);
    ASSERT_INT_EQ(ERR_NONE, err);
    ASSERT_DOUBLE_EQ(1.0, get_var(state, "A"));  /* Should be 1, not 999 */

    basic_cleanup(state);
}

/*
 * Test GOSUB/RETURN
 */
static void test_gosub_return(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* GOSUB should call subroutine and return */
    ASSERT(basic_store_line(state, "10 A=0"));
    ASSERT(basic_store_line(state, "20 GOSUB 100"));
    ASSERT(basic_store_line(state, "30 GOSUB 100"));
    ASSERT(basic_store_line(state, "40 END"));
    ASSERT(basic_store_line(state, "100 A=A+1"));
    ASSERT(basic_store_line(state, "110 RETURN"));

    basic_clear_variables(state);
    state->stack_ptr = 0;
    state->current_line = state->program;
    state->current_line_num = state->current_line->line_number;
    state->text_ptr = state->current_line->text;
    state->running = true;

    ErrorCode err = basic_run(state);
    ASSERT_INT_EQ(ERR_NONE, err);
    ASSERT_DOUBLE_EQ(2.0, get_var(state, "A"));  /* Called twice */

    basic_cleanup(state);
}

/*
 * Test IF/THEN
 */
static void test_if_then(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* IF true THEN should execute */
    ASSERT(basic_store_line(state, "10 X=5"));
    ASSERT(basic_store_line(state, "20 IF X>3 THEN A=1"));
    ASSERT(basic_store_line(state, "30 IF X<3 THEN B=1"));
    ASSERT(basic_store_line(state, "40 END"));

    basic_clear_variables(state);
    state->stack_ptr = 0;
    state->current_line = state->program;
    state->current_line_num = state->current_line->line_number;
    state->text_ptr = state->current_line->text;
    state->running = true;

    ErrorCode err = basic_run(state);
    ASSERT_INT_EQ(ERR_NONE, err);
    ASSERT_DOUBLE_EQ(1.0, get_var(state, "A"));  /* X>3 is true */
    ASSERT_DOUBLE_EQ(0.0, get_var(state, "B"));  /* X<3 is false */

    basic_cleanup(state);
}

/*
 * Test READ/DATA
 */
static void test_read_data(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* READ should get values from DATA */
    ASSERT(basic_store_line(state, "10 READ A,B,C"));
    ASSERT(basic_store_line(state, "20 DATA 1,2,3"));
    ASSERT(basic_store_line(state, "30 END"));

    basic_clear_variables(state);
    state->stack_ptr = 0;
    state->data_ptr.line = NULL;
    state->data_ptr.position = NULL;
    state->current_line = state->program;
    state->current_line_num = state->current_line->line_number;
    state->text_ptr = state->current_line->text;
    state->running = true;

    ErrorCode err = basic_run(state);
    ASSERT_INT_EQ(ERR_NONE, err);
    ASSERT_DOUBLE_EQ(1.0, get_var(state, "A"));
    ASSERT_DOUBLE_EQ(2.0, get_var(state, "B"));
    ASSERT_DOUBLE_EQ(3.0, get_var(state, "C"));

    basic_cleanup(state);
}

/*
 * Test CLEAR statement
 */
static void test_clear(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Set some variables */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "A=5"));
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "B=10"));
    ASSERT_DOUBLE_EQ(5.0, get_var(state, "A"));
    ASSERT_DOUBLE_EQ(10.0, get_var(state, "B"));

    /* CLEAR should reset them */
    ASSERT_INT_EQ(ERR_NONE, exec_line(state, "CLEAR"));
    ASSERT_DOUBLE_EQ(0.0, get_var(state, "A"));
    ASSERT_DOUBLE_EQ(0.0, get_var(state, "B"));

    basic_cleanup(state);
}

/*
 * Test DEF FN
 */
static void test_def_fn(void)
{
    BasicState *state = basic_init();
    ASSERT_NOT_NULL(state);

    /* Define a function */
    ASSERT(basic_store_line(state, "10 DEF FNA(X)=X*X"));
    ASSERT(basic_store_line(state, "20 Y=FNA(5)"));
    ASSERT(basic_store_line(state, "30 END"));

    basic_clear_variables(state);
    state->stack_ptr = 0;
    state->current_line = state->program;
    state->current_line_num = state->current_line->line_number;
    state->text_ptr = state->current_line->text;
    state->running = true;

    ErrorCode err = basic_run(state);
    ASSERT_INT_EQ(ERR_NONE, err);
    ASSERT_DOUBLE_EQ(25.0, get_var(state, "Y"));  /* 5*5 = 25 */

    basic_cleanup(state);
}

/*
 * Run all statement tests
 */
void test_statements(void)
{
    test_suite("Statements");

    test_let_statement();
    test_line_storage();
    test_dim_statement();
    test_array_access();
    test_for_next();
    test_for_next_step();
    test_goto();
    test_gosub_return();
    test_if_then();
    test_read_data();
    test_clear();
    test_def_fn();
}
