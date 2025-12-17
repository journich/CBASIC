/*
 * CBASIC - Test Framework Main File
 *
 * Simple test framework for CBASIC unit tests.
 * Runs all test suites and reports results.
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
 * TEST FRAMEWORK
 * =============================================================================
 */

/* Test statistics */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static const char *current_suite = NULL;

/* Forward declarations for test suites */
extern void test_tokenizer(void);
extern void test_expression(void);
extern void test_statements(void);
extern void test_functions(void);
extern void test_integration(void);

/*
 * test_suite - Start a new test suite
 */
void test_suite(const char *name)
{
    current_suite = name;
    printf("\n=== Test Suite: %s ===\n", name);
}

/*
 * test_assert - Assert a condition
 */
void test_assert(int condition, const char *test_name, const char *file, int line)
{
    tests_run++;
    if (condition) {
        tests_passed++;
        printf("  [PASS] %s\n", test_name);
    } else {
        tests_failed++;
        printf("  [FAIL] %s (%s:%d)\n", test_name, file, line);
    }
}

/*
 * test_assert_int_eq - Assert two integers are equal
 */
void test_assert_int_eq(int expected, int actual, const char *test_name, const char *file, int line)
{
    tests_run++;
    if (expected == actual) {
        tests_passed++;
        printf("  [PASS] %s\n", test_name);
    } else {
        tests_failed++;
        printf("  [FAIL] %s: expected %d, got %d (%s:%d)\n", test_name, expected, actual, file, line);
    }
}

/*
 * test_assert_double_eq - Assert two doubles are approximately equal
 */
void test_assert_double_eq(double expected, double actual, double tolerance,
                           const char *test_name, const char *file, int line)
{
    tests_run++;
    if (fabs(expected - actual) <= tolerance) {
        tests_passed++;
        printf("  [PASS] %s\n", test_name);
    } else {
        tests_failed++;
        printf("  [FAIL] %s: expected %.10g, got %.10g (%s:%d)\n", test_name, expected, actual, file, line);
    }
}

/*
 * test_assert_str_eq - Assert two strings are equal
 */
void test_assert_str_eq(const char *expected, const char *actual, const char *test_name,
                        const char *file, int line)
{
    tests_run++;
    if (expected == NULL && actual == NULL) {
        tests_passed++;
        printf("  [PASS] %s\n", test_name);
    } else if (expected == NULL || actual == NULL) {
        tests_failed++;
        printf("  [FAIL] %s: one string is NULL (%s:%d)\n", test_name, file, line);
    } else if (strcmp(expected, actual) == 0) {
        tests_passed++;
        printf("  [PASS] %s\n", test_name);
    } else {
        tests_failed++;
        printf("  [FAIL] %s: expected '%s', got '%s' (%s:%d)\n", test_name, expected, actual, file, line);
    }
}

/* Convenience macros */
#define ASSERT(cond) test_assert((cond), #cond, __FILE__, __LINE__)
#define ASSERT_TRUE(cond) test_assert((cond), #cond " is true", __FILE__, __LINE__)
#define ASSERT_FALSE(cond) test_assert(!(cond), #cond " is false", __FILE__, __LINE__)
#define ASSERT_INT_EQ(exp, act) test_assert_int_eq((exp), (act), #act " == " #exp, __FILE__, __LINE__)
#define ASSERT_DOUBLE_EQ(exp, act) test_assert_double_eq((exp), (act), 1e-9, #act " == " #exp, __FILE__, __LINE__)
#define ASSERT_DOUBLE_NEAR(exp, act, tol) test_assert_double_eq((exp), (act), (tol), #act " ~= " #exp, __FILE__, __LINE__)
#define ASSERT_STR_EQ(exp, act) test_assert_str_eq((exp), (act), #act " == " #exp, __FILE__, __LINE__)
#define ASSERT_NULL(ptr) test_assert((ptr) == NULL, #ptr " is NULL", __FILE__, __LINE__)
#define ASSERT_NOT_NULL(ptr) test_assert((ptr) != NULL, #ptr " is not NULL", __FILE__, __LINE__)

/*
 * =============================================================================
 * MAIN ENTRY POINT
 * =============================================================================
 */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("CBASIC Test Suite\n");
    printf("==================\n");

    /* Run all test suites */
    test_tokenizer();
    test_expression();
    test_statements();
    test_functions();
    test_integration();

    /* Print summary */
    printf("\n==================\n");
    printf("Test Results:\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("==================\n");

    return tests_failed > 0 ? 1 : 0;
}
