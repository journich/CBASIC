/*
 * CBASIC - A C17 Implementation of Microsoft BASIC 1.1
 *
 * Based on the original 6502 assembly language implementation
 * Copyright (c) 1976-1978 Microsoft Corporation
 * C17 Port Copyright (c) 2025 Tim Buchalka
 *
 * Licensed under the MIT License
 *
 * This header defines the core data structures and constants
 * for the BASIC interpreter, matching the 6502 original.
 */

#ifndef BASIC_H
#define BASIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * =============================================================================
 * VERSION AND CONFIGURATION
 * =============================================================================
 */

#define BASIC_VERSION       "1.1"
#define BASIC_VERSION_MAJOR 1
#define BASIC_VERSION_MINOR 1
#define BASIC_COPYRIGHT     "Copyright (c) 1976-1978 Microsoft Corporation"

/* Feature flags matching 6502 assembly configuration */
#define FEATURE_INTEGER_ARRAYS  1   /* INTPRC - Integer array support */
#define FEATURE_ADD_PRECISION   1   /* ADDPRC - Additional FP precision */
#define FEATURE_LONG_ERRORS     1   /* LNGERR - Long error messages */
#define FEATURE_TIME            0   /* TIME - Clock reading (not in original) */
#define FEATURE_EXT_IO          0   /* EXTIO - External I/O (file support) */
#define FEATURE_DISK            0   /* DISKO - Save/Load disk commands */
#define FEATURE_NULL_CMD        1   /* NULCMD - NULL command support */
#define FEATURE_GET_CMD         1   /* GETCMD - GET command support */

/*
 * =============================================================================
 * MEMORY CONFIGURATION
 * =============================================================================
 * These match the original 6502 memory constraints for compatibility
 */

#define BASIC_MEMORY_SIZE   65536           /* Total addressable memory */
#define BASIC_LINE_MAX      255             /* Max length of input line */
#define BASIC_STRING_MAX    255             /* Max string length */
#define BASIC_LINE_NUM_MAX  63999           /* Maximum line number */
#define BASIC_STACK_SIZE    512             /* Stack size for FOR/GOSUB */
#define BASIC_VAR_NAME_LEN  2               /* Variable name length (first 2 chars) */
#define BASIC_ARRAY_DIMS    11              /* Max array dimensions (0-10 default) */
#define BASIC_TERMINAL_WIDTH 80             /* Default terminal width */
#define BASIC_NULL_COUNT    0               /* NULLs after CR (for slow terminals) */

/*
 * =============================================================================
 * TOKEN DEFINITIONS
 * =============================================================================
 * Tokens for reserved words, matching the 6502 tokenization scheme
 * Tokens start at 0x80 to distinguish from ASCII characters
 */

typedef enum {
    /* Statements - order matches 6502 dispatch table */
    TOK_END = 0x80,
    TOK_FOR,
    TOK_NEXT,
    TOK_DATA,
    TOK_INPUT,
    TOK_DIM,
    TOK_READ,
    TOK_LET,
    TOK_GOTO,
    TOK_RUN,
    TOK_IF,
    TOK_RESTORE,
    TOK_GOSUB,
    TOK_RETURN,
    TOK_REM,
    TOK_STOP,
    TOK_ON,
    TOK_NULL,
    TOK_WAIT,
    TOK_LOAD,
    TOK_SAVE,
    TOK_VERIFY,
    TOK_DEF,
    TOK_POKE,
    TOK_PRINT,
    TOK_CONT,
    TOK_LIST,
    TOK_CLEAR,
    TOK_GET,
    TOK_NEW,

    /* Auxiliary tokens */
    TOK_TAB,        /* TAB( function in PRINT */
    TOK_TO,
    TOK_FN,
    TOK_SPC,        /* SPC( function in PRINT */
    TOK_THEN,
    TOK_NOT,
    TOK_STEP,

    /* Operators */
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,
    TOK_POWER,
    TOK_AND,
    TOK_OR,
    TOK_GT,         /* > */
    TOK_EQ,         /* = */
    TOK_LT,         /* < */

    /* Functions - order matches 6502 function dispatch */
    TOK_SGN,
    TOK_INT,
    TOK_ABS,
    TOK_USR,
    TOK_FRE,
    TOK_POS,
    TOK_SQR,
    TOK_RND,
    TOK_LOG,
    TOK_EXP,
    TOK_COS,
    TOK_SIN,
    TOK_TAN,
    TOK_ATN,
    TOK_PEEK,
    TOK_LEN,
    TOK_STR,        /* STR$ */
    TOK_VAL,
    TOK_ASC,
    TOK_CHR,        /* CHR$ */
    TOK_LEFT,       /* LEFT$ */
    TOK_RIGHT,      /* RIGHT$ */
    TOK_MID,        /* MID$ */

    TOK_LAST        /* Sentinel */
} Token;

/*
 * =============================================================================
 * ERROR CODES
 * =============================================================================
 * Error codes matching the 6502 implementation's 2-character codes
 */

typedef enum {
    ERR_NONE = 0,
    ERR_NF,         /* NEXT without FOR */
    ERR_SN,         /* Syntax error */
    ERR_RG,         /* RETURN without GOSUB */
    ERR_OD,         /* Out of DATA */
    ERR_FC,         /* Illegal function call (bad quantity) */
    ERR_OV,         /* Overflow */
    ERR_OM,         /* Out of memory */
    ERR_US,         /* Undefined statement (line not found) */
    ERR_BS,         /* Bad subscript */
    ERR_DD,         /* Redimensioned array */
    ERR_DZ,         /* Division by zero */
    ERR_ID,         /* Illegal direct mode */
    ERR_TM,         /* Type mismatch */
    ERR_LS,         /* String too long */
    ERR_FD,         /* File data error */
    ERR_ST,         /* String formula too complex */
    ERR_CN,         /* Can't continue */
    ERR_UF,         /* Undefined function */
    ERR_BREAK,      /* CTRL-C break */
    ERR_COUNT       /* Number of error codes */
} ErrorCode;

/*
 * =============================================================================
 * FLOATING POINT FORMAT
 * =============================================================================
 * Microsoft BASIC uses a 32-bit (4-byte) floating point format:
 * - 1 byte exponent (excess-128 format)
 * - 3 bytes mantissa (with implied leading 1, sign in MSB)
 *
 * For additional precision mode (ADDPRC), 5 bytes are used:
 * - 1 byte exponent
 * - 4 bytes mantissa
 *
 * We use C's double for internal calculations but convert for compatibility
 */

typedef struct {
    uint8_t exponent;       /* Biased exponent (0 = zero value) */
    uint8_t mantissa[4];    /* Mantissa bytes (MSB contains sign) */
} BasicFloat;

/* Floating Point Accumulator - matches 6502 FAC */
typedef struct {
    uint8_t exponent;       /* FACEXP - Exponent */
    uint8_t mantissa[4];    /* FACHO, FACMOH, FACMO, FACLO */
    int8_t sign;            /* FACSGN - Sign (0=positive, 0xFF=negative) */
    uint8_t overflow;       /* FACOV - Overflow/rounding byte */
} FloatAccumulator;

/* Argument Register - secondary accumulator for operations */
typedef FloatAccumulator ArgRegister;

/*
 * =============================================================================
 * VALUE TYPES
 * =============================================================================
 */

typedef enum {
    VAL_NUMBER,     /* Numeric value (floating point) */
    VAL_STRING,     /* String value */
    VAL_INTEGER     /* Integer value (for array subscripts, etc.) */
} ValueType;

/* String descriptor - matches 6502 format (length + pointer) */
typedef struct {
    uint8_t length;         /* String length (0-255) */
    char *data;             /* Pointer to string data */
    bool is_temp;           /* True if in temporary string space */
} StringDescriptor;

/* Generic value type */
typedef struct {
    ValueType type;
    union {
        double number;          /* Numeric value */
        int32_t integer;        /* Integer value */
        StringDescriptor string; /* String value */
    } v;
} Value;

/*
 * =============================================================================
 * VARIABLE STRUCTURES
 * =============================================================================
 */

/* Variable name encoding (first 2 significant chars + type indicator) */
typedef struct {
    char name[3];           /* 2-character name + null terminator */
    bool is_string;         /* True if string variable (name ends with $) */
    bool is_integer;        /* True if integer variable (name ends with %) */
} VariableName;

/* Simple variable entry */
typedef struct Variable {
    VariableName name;
    Value value;
    struct Variable *next;  /* Linked list pointer */
} Variable;

/* Array dimension info */
typedef struct {
    int32_t size;           /* Size of this dimension (subscript + 1) */
} ArrayDimension;

/* Array variable entry */
typedef struct Array {
    VariableName name;
    int num_dims;           /* Number of dimensions */
    ArrayDimension dims[BASIC_ARRAY_DIMS]; /* Dimension sizes */
    Value *data;            /* Array data (row-major order) */
    size_t total_elements;  /* Total number of elements */
    struct Array *next;     /* Linked list pointer */
} Array;

/* User-defined function (DEF FN) */
typedef struct UserFunction {
    char name;              /* Single letter after FN (A-Z) */
    char param;             /* Parameter variable (single letter) */
    char *definition;       /* Function definition text */
    int def_line;           /* Line number where defined */
    int def_pos;            /* Position in line where definition starts */
    struct UserFunction *next;
} UserFunction;

/*
 * =============================================================================
 * PROGRAM STORAGE
 * =============================================================================
 * Program lines are stored in a linked list, sorted by line number
 */

typedef struct ProgramLine {
    int32_t line_number;        /* Line number (1-63999) */
    char *text;                 /* Tokenized line text */
    size_t length;              /* Length of tokenized text */
    struct ProgramLine *next;   /* Next line (or NULL) */
} ProgramLine;

/*
 * =============================================================================
 * RUNTIME STACK
 * =============================================================================
 * Used for FOR loops, GOSUB returns, and expression evaluation
 */

typedef enum {
    STACK_FOR,          /* FOR loop context */
    STACK_GOSUB,        /* GOSUB return address */
    STACK_EXPR          /* Expression evaluation */
} StackEntryType;

/* FOR loop stack entry - matches 6502 16-18 byte format */
typedef struct {
    Variable *loop_var;     /* Loop variable pointer */
    double step;            /* STEP value */
    double limit;           /* TO limit value */
    int32_t line_number;    /* Line number of FOR */
    char *text_ptr;         /* Position after FOR statement */
    struct ProgramLine *line; /* Program line of FOR (for looping back) */
} ForLoopEntry;

/* GOSUB stack entry - matches 6502 5-byte format */
typedef struct {
    int32_t line_number;    /* Return line number */
    char *text_ptr;         /* Return text position */
} GosubEntry;

/* Generic stack entry */
typedef struct {
    StackEntryType type;
    union {
        ForLoopEntry for_loop;
        GosubEntry gosub;
    } data;
} StackEntry;

/*
 * =============================================================================
 * DATA STATEMENT TRACKING
 * =============================================================================
 */

typedef struct DataPointer {
    ProgramLine *line;      /* Current DATA line */
    char *position;         /* Position within DATA line */
} DataPointer;

/*
 * =============================================================================
 * INTERPRETER STATE
 * =============================================================================
 * Main state structure for the BASIC interpreter
 */

typedef struct {
    /* Program storage */
    ProgramLine *program;       /* First line of program */
    ProgramLine *current_line;  /* Currently executing line */
    char *text_ptr;             /* Current position in line (TXTPTR) */

    /* Variable storage */
    Variable *variables;        /* Simple variables (VARTAB) */
    Array *arrays;              /* Array variables (ARYTAB) */
    UserFunction *functions;    /* User-defined functions */

    /* String management */
    char *string_space;         /* String storage area */
    size_t string_space_size;   /* Size of string space */
    char *string_ptr;           /* Current position in string space (FRETOP) */

    /* Runtime stack */
    StackEntry *stack;          /* Runtime stack */
    int stack_ptr;              /* Stack pointer */
    int stack_size;             /* Stack capacity */

    /* Accumulators */
    FloatAccumulator fac;       /* Primary floating accumulator */
    ArgRegister arg;            /* Argument register */

    /* DATA statement pointer */
    DataPointer data_ptr;       /* Current DATA read position */

    /* Execution state */
    bool running;               /* True if program is running */
    bool direct_mode;           /* True if in direct/immediate mode */
    int32_t current_line_num;   /* Current line number (0 for direct) */
    ErrorCode last_error;       /* Last error code */
    int32_t error_line;         /* Line where error occurred */

    /* For CONT command */
    ProgramLine *cont_line;     /* Line to continue from */
    char *cont_ptr;             /* Position to continue from */
    bool can_continue;          /* True if CONT is allowed */

    /* Terminal state */
    int terminal_width;         /* Terminal width (LINWID) */
    int terminal_pos;           /* Current terminal position (TRMPOS) */
    int null_count;             /* NULLs to print after CR */

    /* Input buffer */
    char input_buffer[BASIC_LINE_MAX + 1];
    size_t input_pos;

    /* Random number generator state */
    uint32_t rnd_seed;          /* RND seed (RNDX in 6502) */

    /* Memory simulation for PEEK/POKE */
    uint8_t *memory;            /* Simulated memory space */
    size_t memory_size;

    /* Flags */
    bool trace_mode;            /* Trace execution (not in all versions) */
    bool suppress_prompt;       /* Suppress input prompt */

} BasicState;

/*
 * =============================================================================
 * FUNCTION PROTOTYPES - CORE INTERPRETER
 * =============================================================================
 */

/* Initialization and cleanup */
BasicState *basic_init(void);
void basic_cleanup(BasicState *state);
void basic_reset(BasicState *state);

/* Main interpreter loop */
int basic_run(BasicState *state);
void basic_repl(BasicState *state);

/* Line management */
bool basic_store_line(BasicState *state, const char *line);
void basic_delete_line(BasicState *state, int32_t line_num);
ProgramLine *basic_find_line(BasicState *state, int32_t line_num);
void basic_list(BasicState *state, int32_t start, int32_t end);
void basic_new(BasicState *state);

/* Execution */
ErrorCode basic_execute_line(BasicState *state, const char *line);
ErrorCode basic_execute_statement(BasicState *state);
void basic_goto_line(BasicState *state, int32_t line_num);

/*
 * =============================================================================
 * FUNCTION PROTOTYPES - TOKENIZER
 * =============================================================================
 */

/* Tokenization */
char *basic_tokenize(const char *line, size_t *out_len);
char *basic_detokenize(const char *tokenized, size_t len);
const char *basic_token_name(Token tok);

/*
 * =============================================================================
 * FUNCTION PROTOTYPES - EXPRESSION EVALUATOR
 * =============================================================================
 */

/* Expression evaluation (FRMEVL) */
ErrorCode basic_evaluate(BasicState *state, Value *result);
ErrorCode basic_eval_numeric(BasicState *state, double *result);
ErrorCode basic_eval_integer(BasicState *state, int32_t *result);
ErrorCode basic_eval_string(BasicState *state, StringDescriptor *result);

/*
 * =============================================================================
 * FUNCTION PROTOTYPES - VARIABLES
 * =============================================================================
 */

/* Variable management */
Variable *basic_get_variable(BasicState *state, const char *name);
Variable *basic_create_variable(BasicState *state, const char *name);
ErrorCode basic_set_variable(BasicState *state, const char *name, Value *value);

/* Array management */
Array *basic_get_array(BasicState *state, const char *name);
ErrorCode basic_dim_array(BasicState *state, const char *name, int dims[], int num_dims);
ErrorCode basic_get_array_element(BasicState *state, Array *arr, int indices[], Value *result);
ErrorCode basic_set_array_element(BasicState *state, Array *arr, int indices[], Value *value);

/* User functions */
ErrorCode basic_def_function(BasicState *state, char name, char param, const char *def);
ErrorCode basic_call_function(BasicState *state, char name, double arg, double *result);

/* Variable cleanup */
void basic_clear_variables(BasicState *state);

/*
 * =============================================================================
 * FUNCTION PROTOTYPES - BUILT-IN FUNCTIONS
 * =============================================================================
 */

/* Numeric functions */
double basic_fn_sgn(double x);
double basic_fn_int(double x);
double basic_fn_abs(double x);
double basic_fn_sqr(double x);
double basic_fn_rnd(BasicState *state, double x);
double basic_fn_log(double x);
double basic_fn_exp(double x);
double basic_fn_sin(double x);
double basic_fn_cos(double x);
double basic_fn_tan(double x);
double basic_fn_atn(double x);
int32_t basic_fn_fre(BasicState *state, double x);
int32_t basic_fn_pos(BasicState *state, double x);
int32_t basic_fn_peek(BasicState *state, int32_t addr);

/* String functions */
StringDescriptor basic_fn_str(BasicState *state, double x);
double basic_fn_val(const char *s);
int32_t basic_fn_len(StringDescriptor *s);
int32_t basic_fn_asc(StringDescriptor *s);
StringDescriptor basic_fn_chr(BasicState *state, int32_t x);
StringDescriptor basic_fn_left(BasicState *state, StringDescriptor *s, int32_t n);
StringDescriptor basic_fn_right(BasicState *state, StringDescriptor *s, int32_t n);
StringDescriptor basic_fn_mid(BasicState *state, StringDescriptor *s, int32_t start, int32_t len);

/*
 * =============================================================================
 * FUNCTION PROTOTYPES - I/O
 * =============================================================================
 */

/* Output functions */
void basic_print_string(BasicState *state, const char *s);
void basic_print_number(BasicState *state, double n);
void basic_print_newline(BasicState *state);
void basic_print_tab(BasicState *state, int pos);
void basic_print_char(BasicState *state, char c);

/* Input functions */
bool basic_input_line(BasicState *state, const char *prompt, char *buffer, size_t size);
ErrorCode basic_input_value(BasicState *state, const char *prompt, Value *result, ValueType expected);

/*
 * =============================================================================
 * FUNCTION PROTOTYPES - ERRORS
 * =============================================================================
 */

/* Error handling */
void basic_error(BasicState *state, ErrorCode code);
const char *basic_error_message(ErrorCode code);
const char *basic_error_short(ErrorCode code);

/*
 * =============================================================================
 * FUNCTION PROTOTYPES - UTILITIES
 * =============================================================================
 */

/* String utilities */
char *basic_alloc_string(BasicState *state, size_t len);
void basic_free_string(BasicState *state, StringDescriptor *s);
void basic_garbage_collect(BasicState *state);
StringDescriptor basic_copy_string(BasicState *state, const char *s, size_t len);

/* Numeric utilities */
void basic_float_to_fac(BasicState *state, double value);
double basic_fac_to_float(BasicState *state);
void basic_normalize_fac(BasicState *state);

/* Character utilities */
char basic_chrget(BasicState *state);
char basic_peek_char(BasicState *state);
void basic_skip_spaces(BasicState *state);
bool basic_is_digit(char c);
bool basic_is_letter(char c);
bool basic_is_end_of_statement(char c);

/*
 * =============================================================================
 * MACROS AND INLINE UTILITIES
 * =============================================================================
 */

/* Character classification */
#define BASIC_IS_TOKEN(c)   ((unsigned char)(c) >= 0x80)
#define BASIC_IS_DIGIT(c)   ((c) >= '0' && (c) <= '9')
#define BASIC_IS_LETTER(c)  (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#define BASIC_IS_SPACE(c)   ((c) == ' ' || (c) == '\t')
#define BASIC_IS_EOL(c)     ((c) == '\0' || (c) == '\n' || (c) == '\r')
#define BASIC_IS_EOS(c)     ((c) == ':' || BASIC_IS_EOL(c))

/* Token classification */
#define BASIC_IS_STATEMENT(t) ((t) >= TOK_END && (t) <= TOK_NEW)
#define BASIC_IS_FUNCTION(t)  ((t) >= TOK_SGN && (t) <= TOK_MID)
#define BASIC_IS_OPERATOR(t)  ((t) >= TOK_PLUS && (t) <= TOK_LT)

/* Uppercase conversion */
#define BASIC_TOUPPER(c)    (((c) >= 'a' && (c) <= 'z') ? ((c) - 32) : (c))

#endif /* BASIC_H */
