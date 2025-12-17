/*
 * CBASIC - Main Entry Point
 *
 * Microsoft BASIC 1.1 Compatible Interpreter
 * C17 Implementation
 *
 * This is a faithful recreation of Microsoft BASIC 1.1 for 6502
 * microprocessors, implemented in portable C17 code.
 *
 * Usage:
 *   cbasic              - Start interactive REPL
 *   cbasic file.bas     - Load and run a BASIC program
 *   cbasic -h           - Show help
 *
 * Copyright (c) 2025 Tim Buchalka
 * Licensed under the MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "basic.h"

/* External declarations */
extern void basic_print_banner(BasicState *state);
extern void basic_print_ready(BasicState *state);

/*
 * print_help - Display usage information
 */
static void print_help(const char *program_name)
{
    printf("CBASIC - Microsoft BASIC 1.1 Compatible Interpreter\n");
    printf("C17 Implementation\n\n");
    printf("Usage:\n");
    printf("  %s              Start interactive BASIC interpreter\n", program_name);
    printf("  %s file.bas     Load and run a BASIC program\n", program_name);
    printf("  %s -h, --help   Show this help message\n", program_name);
    printf("  %s -v, --version Show version information\n\n", program_name);
    printf("Interactive Commands:\n");
    printf("  NEW             Clear program and variables\n");
    printf("  LIST [n-m]      List program (optionally lines n to m)\n");
    printf("  RUN [n]         Run program (optionally from line n)\n");
    printf("  CONT            Continue after STOP or error\n");
    printf("  CLEAR           Clear all variables\n");
    printf("  QUIT/EXIT       Exit the interpreter\n\n");
    printf("For more information, see the README.md file.\n");
}

/*
 * print_version - Display version information
 */
static void print_version(void)
{
    printf("CBASIC Version %s\n", BASIC_VERSION);
    printf("Microsoft BASIC 1.1 Compatible\n");
    printf("C17 Port %s\n", BASIC_COPYRIGHT);
    printf("C17 Port Copyright (c) 2025 Tim Buchalka\n");
}

/*
 * load_program - Load a BASIC program from a file
 *
 * Parameters:
 *   state    - BASIC interpreter state
 *   filename - Name of file to load
 *
 * Returns:
 *   true on success, false on error
 */
static bool load_program(BasicState *state, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return false;
    }

    char line_buffer[BASIC_LINE_MAX + 1];
    int line_count = 0;
    bool has_errors = false;

    while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
        /* Remove trailing newline */
        size_t len = strlen(line_buffer);
        while (len > 0 && (line_buffer[len - 1] == '\n' || line_buffer[len - 1] == '\r')) {
            line_buffer[--len] = '\0';
        }

        /* Skip empty lines and comment-only lines starting with # */
        char *p = line_buffer;
        while (*p == ' ') p++;
        if (*p == '\0' || *p == '#') {
            continue;
        }

        /* Check if line starts with a number (program line) */
        if (BASIC_IS_DIGIT(*p)) {
            if (!basic_store_line(state, line_buffer)) {
                fprintf(stderr, "Warning: Error storing line: %s\n", line_buffer);
                has_errors = true;
            } else {
                line_count++;
            }
        }
    }

    fclose(fp);

    if (has_errors) {
        fprintf(stderr, "Warning: Some lines could not be loaded\n");
    }

    return line_count > 0;
}

/*
 * run_interactive - Run in interactive REPL mode
 *
 * Parameters:
 *   state - BASIC interpreter state
 */
static void run_interactive(BasicState *state)
{
    basic_repl(state);
}

/*
 * run_program - Run a loaded program
 *
 * Parameters:
 *   state - BASIC interpreter state
 */
static void run_program(BasicState *state)
{
    if (state->program == NULL) {
        fprintf(stderr, "Error: No program loaded\n");
        return;
    }

    /* Initialize for RUN */
    basic_clear_variables(state);
    state->stack_ptr = 0;
    state->current_line = state->program;
    state->current_line_num = state->current_line->line_number;
    state->text_ptr = state->current_line->text;
    state->running = true;
    state->can_continue = true;

    /* Run the program */
    int result = basic_run(state);

    /* Display error if any */
    if (result != ERR_NONE) {
        basic_error(state, result);
    }
}

/*
 * main - Program entry point
 */
int main(int argc, char *argv[])
{
    /* Parse command line arguments */
    const char *filename = NULL;
    bool show_help = false;
    bool show_version = false;
    bool run_file = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            show_version = true;
        } else if (argv[i][0] != '-') {
            filename = argv[i];
            run_file = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_help(argv[0]);
            return 1;
        }
    }

    if (show_help) {
        print_help(argv[0]);
        return 0;
    }

    if (show_version) {
        print_version();
        return 0;
    }

    /* Initialize the interpreter */
    BasicState *state = basic_init();
    if (state == NULL) {
        fprintf(stderr, "Error: Failed to initialize BASIC interpreter\n");
        return 1;
    }

    if (run_file) {
        /* Load and run a BASIC program file */
        if (!load_program(state, filename)) {
            fprintf(stderr, "Error: Failed to load program '%s'\n", filename);
            basic_cleanup(state);
            return 1;
        }

        /* Print minimal banner for file execution */
        printf("CBASIC %s\n\n", BASIC_VERSION);

        run_program(state);
    } else {
        /* Run in interactive mode */
        run_interactive(state);
    }

    /* Clean up */
    basic_cleanup(state);

    return 0;
}
