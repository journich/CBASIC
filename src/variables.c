/*
 * CBASIC - Variables Module
 *
 * Implements variable storage, array management, and user-defined functions.
 * This matches the 6502 implementation's VARTAB (simple variables),
 * ARYTAB (arrays), and DEF FN functionality.
 *
 * Based on 6502 implementation at lines 3598-3850 of m6502.asm
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
 * VARIABLE NAME HELPERS
 * =============================================================================
 */

/*
 * parse_var_name - Extract and normalize variable name
 *
 * In original BASIC, only the first 2 characters are significant.
 * Names ending in $ are string variables, % are integers (though we treat
 * all numerics as floating point for simplicity).
 *
 * Parameters:
 *   name   - Input variable name
 *   parsed - Output structure with normalized name and type flags
 */
static void parse_var_name(const char *name, VariableName *parsed)
{
    parsed->is_string = false;
    parsed->is_integer = false;

    int len = (int)strlen(name);

    /* Check for type suffix */
    if (len > 0 && name[len - 1] == '$') {
        parsed->is_string = true;
        len--;
    } else if (len > 0 && name[len - 1] == '%') {
        parsed->is_integer = true;
        len--;
    }

    /* Copy first 2 significant characters (uppercase) */
    int copy_len = (len > 2) ? 2 : len;
    for (int i = 0; i < copy_len; i++) {
        parsed->name[i] = BASIC_TOUPPER(name[i]);
    }

    /* Pad with spaces if needed */
    for (int i = copy_len; i < 2; i++) {
        parsed->name[i] = ' ';
    }
    parsed->name[2] = '\0';
}

/*
 * var_names_equal - Compare two variable names
 *
 * Parameters:
 *   a, b - Variable names to compare
 *
 * Returns:
 *   true if names match (including type), false otherwise
 */
static bool var_names_equal(const VariableName *a, const VariableName *b)
{
    return a->name[0] == b->name[0] &&
           a->name[1] == b->name[1] &&
           a->is_string == b->is_string &&
           a->is_integer == b->is_integer;
}

/*
 * =============================================================================
 * SIMPLE VARIABLES
 * =============================================================================
 */

/*
 * basic_get_variable - Find a variable by name
 *
 * This implements the variable lookup from VARTAB in the 6502 code.
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   name  - Variable name to find
 *
 * Returns:
 *   Pointer to variable, or NULL if not found
 */
Variable *basic_get_variable(BasicState *state, const char *name)
{
    if (state == NULL || name == NULL) {
        return NULL;
    }

    VariableName parsed;
    parse_var_name(name, &parsed);

    /* Search variable list */
    Variable *var = state->variables;
    while (var != NULL) {
        if (var_names_equal(&var->name, &parsed)) {
            return var;
        }
        var = var->next;
    }

    return NULL;
}

/*
 * basic_create_variable - Create a new variable
 *
 * Creates a variable with the given name, initialized to 0 or empty string.
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   name  - Variable name
 *
 * Returns:
 *   Pointer to new variable, or NULL on memory error
 */
Variable *basic_create_variable(BasicState *state, const char *name)
{
    if (state == NULL || name == NULL) {
        return NULL;
    }

    /* Allocate new variable */
    Variable *var = malloc(sizeof(Variable));
    if (var == NULL) {
        return NULL;
    }

    /* Parse and store name */
    parse_var_name(name, &var->name);

    /* Initialize value based on type */
    if (var->name.is_string) {
        var->value.type = VAL_STRING;
        var->value.v.string.length = 0;
        var->value.v.string.data = "";
        var->value.v.string.is_temp = false;
    } else {
        var->value.type = VAL_NUMBER;
        var->value.v.number = 0.0;
    }

    /* Add to front of list */
    var->next = state->variables;
    state->variables = var;

    return var;
}

/*
 * basic_set_variable - Set a variable's value
 *
 * Creates the variable if it doesn't exist.
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   name  - Variable name
 *   value - Value to assign
 *
 * Returns:
 *   ERR_NONE on success, error code on failure
 */
ErrorCode basic_set_variable(BasicState *state, const char *name, Value *value)
{
    if (state == NULL || name == NULL || value == NULL) {
        return ERR_SN;
    }

    Variable *var = basic_get_variable(state, name);
    if (var == NULL) {
        var = basic_create_variable(state, name);
        if (var == NULL) {
            return ERR_OM;
        }
    }

    /* Type check */
    if (var->name.is_string && value->type != VAL_STRING) {
        return ERR_TM;
    }
    if (!var->name.is_string && value->type == VAL_STRING) {
        return ERR_TM;
    }

    /* Copy value */
    if (value->type == VAL_STRING) {
        /* Allocate storage for string */
        if (value->v.string.length > 0) {
            char *str = basic_alloc_string(state, value->v.string.length);
            if (str == NULL) {
                return ERR_OM;
            }
            memcpy(str, value->v.string.data, value->v.string.length);
            var->value.v.string.data = str;
            var->value.v.string.length = value->v.string.length;
            var->value.v.string.is_temp = false;
        } else {
            var->value.v.string.data = "";
            var->value.v.string.length = 0;
            var->value.v.string.is_temp = false;
        }
    } else {
        var->value.v.number = value->v.number;
    }

    return ERR_NONE;
}

/*
 * =============================================================================
 * ARRAYS
 * =============================================================================
 */

/*
 * basic_get_array - Find an array by name
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   name  - Array name to find
 *
 * Returns:
 *   Pointer to array, or NULL if not found
 */
Array *basic_get_array(BasicState *state, const char *name)
{
    if (state == NULL || name == NULL) {
        return NULL;
    }

    VariableName parsed;
    parse_var_name(name, &parsed);

    /* Search array list */
    Array *arr = state->arrays;
    while (arr != NULL) {
        if (var_names_equal(&arr->name, &parsed)) {
            return arr;
        }
        arr = arr->next;
    }

    return NULL;
}

/*
 * basic_dim_array - Create/dimension an array
 *
 * This implements the DIM statement functionality.
 * Arrays are 0-based internally but 0-indexed to max in BASIC
 * (e.g., DIM A(10) creates 11 elements: A(0) through A(10))
 *
 * Parameters:
 *   state    - BASIC interpreter state
 *   name     - Array name
 *   dims     - Array of dimension sizes (each is max subscript)
 *   num_dims - Number of dimensions
 *
 * Returns:
 *   ERR_NONE on success, error code on failure
 */
ErrorCode basic_dim_array(BasicState *state, const char *name, int dims[], int num_dims)
{
    if (state == NULL || name == NULL || dims == NULL) {
        return ERR_SN;
    }

    if (num_dims < 1 || num_dims > BASIC_ARRAY_DIMS) {
        return ERR_FC;  /* Too many dimensions */
    }

    /* Check if already exists */
    if (basic_get_array(state, name) != NULL) {
        return ERR_DD;  /* Already dimensioned */
    }

    /* Allocate array structure */
    Array *arr = malloc(sizeof(Array));
    if (arr == NULL) {
        return ERR_OM;
    }

    /* Parse name */
    parse_var_name(name, &arr->name);
    arr->num_dims = num_dims;

    /* Calculate total elements and store dimensions */
    arr->total_elements = 1;
    for (int i = 0; i < num_dims; i++) {
        if (dims[i] < 0 || dims[i] > 32767) {
            free(arr);
            return ERR_FC;
        }
        arr->dims[i].size = dims[i] + 1;  /* +1 because 0-based to max */
        arr->total_elements *= (size_t)arr->dims[i].size;
    }

    /* Allocate data storage */
    arr->data = calloc(arr->total_elements, sizeof(Value));
    if (arr->data == NULL) {
        free(arr);
        return ERR_OM;
    }

    /* Initialize elements */
    for (size_t i = 0; i < arr->total_elements; i++) {
        if (arr->name.is_string) {
            arr->data[i].type = VAL_STRING;
            arr->data[i].v.string.length = 0;
            arr->data[i].v.string.data = "";
            arr->data[i].v.string.is_temp = false;
        } else {
            arr->data[i].type = VAL_NUMBER;
            arr->data[i].v.number = 0.0;
        }
    }

    /* Add to array list */
    arr->next = state->arrays;
    state->arrays = arr;

    return ERR_NONE;
}

/*
 * array_index - Calculate linear index from subscripts
 *
 * Parameters:
 *   arr     - Array
 *   indices - Array of subscript values
 *   index   - Receives calculated linear index
 *
 * Returns:
 *   ERR_NONE on success, ERR_BS if subscript out of range
 */
static ErrorCode array_index(Array *arr, int indices[], size_t *index)
{
    size_t idx = 0;
    size_t multiplier = 1;

    /* Calculate index using row-major order */
    for (int i = arr->num_dims - 1; i >= 0; i--) {
        if (indices[i] < 0 || indices[i] >= arr->dims[i].size) {
            return ERR_BS;  /* Bad subscript */
        }
        idx += (size_t)indices[i] * multiplier;
        multiplier *= (size_t)arr->dims[i].size;
    }

    *index = idx;
    return ERR_NONE;
}

/*
 * basic_get_array_element - Get an array element value
 *
 * Parameters:
 *   state   - BASIC interpreter state
 *   arr     - Array
 *   indices - Array of subscript values
 *   result  - Receives the element value
 *
 * Returns:
 *   ERR_NONE on success, error code on failure
 */
ErrorCode basic_get_array_element(BasicState *state, Array *arr, int indices[], Value *result)
{
    (void)state;  /* Unused but kept for API consistency */

    if (arr == NULL || indices == NULL || result == NULL) {
        return ERR_SN;
    }

    size_t idx;
    ErrorCode err = array_index(arr, indices, &idx);
    if (err != ERR_NONE) {
        return err;
    }

    *result = arr->data[idx];
    return ERR_NONE;
}

/*
 * basic_set_array_element - Set an array element value
 *
 * Parameters:
 *   state   - BASIC interpreter state
 *   arr     - Array
 *   indices - Array of subscript values
 *   value   - Value to assign
 *
 * Returns:
 *   ERR_NONE on success, error code on failure
 */
ErrorCode basic_set_array_element(BasicState *state, Array *arr, int indices[], Value *value)
{
    if (arr == NULL || indices == NULL || value == NULL) {
        return ERR_SN;
    }

    /* Type check */
    if (arr->name.is_string && value->type != VAL_STRING) {
        return ERR_TM;
    }
    if (!arr->name.is_string && value->type == VAL_STRING) {
        return ERR_TM;
    }

    size_t idx;
    ErrorCode err = array_index(arr, indices, &idx);
    if (err != ERR_NONE) {
        return err;
    }

    /* Store value */
    if (value->type == VAL_STRING) {
        if (value->v.string.length > 0) {
            char *str = basic_alloc_string(state, value->v.string.length);
            if (str == NULL) {
                return ERR_OM;
            }
            memcpy(str, value->v.string.data, value->v.string.length);
            arr->data[idx].v.string.data = str;
            arr->data[idx].v.string.length = value->v.string.length;
            arr->data[idx].v.string.is_temp = false;
        } else {
            arr->data[idx].v.string.data = "";
            arr->data[idx].v.string.length = 0;
            arr->data[idx].v.string.is_temp = false;
        }
    } else {
        arr->data[idx].v.number = value->v.number;
    }

    return ERR_NONE;
}

/*
 * =============================================================================
 * USER-DEFINED FUNCTIONS (DEF FN)
 * =============================================================================
 */

/*
 * basic_def_function - Define a user function
 *
 * Implements DEF FNA(X) = expression
 *
 * Parameters:
 *   state - BASIC interpreter state
 *   name  - Function name (single letter A-Z)
 *   param - Parameter name (single letter)
 *   def   - Definition text pointer
 *
 * Returns:
 *   ERR_NONE on success, error code on failure
 */
ErrorCode basic_def_function(BasicState *state, char name, char param, const char *def)
{
    if (state == NULL || def == NULL) {
        return ERR_SN;
    }

    name = BASIC_TOUPPER(name);
    param = BASIC_TOUPPER(param);

    if (!BASIC_IS_LETTER(name) || !BASIC_IS_LETTER(param)) {
        return ERR_SN;
    }

    /* Check if function already exists */
    UserFunction *fn = state->functions;
    while (fn != NULL) {
        if (fn->name == name) {
            /* Redefine */
            fn->param = param;
            free(fn->definition);
            fn->definition = strdup(def);
            fn->def_line = state->current_line_num;
            return ERR_NONE;
        }
        fn = fn->next;
    }

    /* Create new function */
    fn = malloc(sizeof(UserFunction));
    if (fn == NULL) {
        return ERR_OM;
    }

    fn->name = name;
    fn->param = param;
    fn->definition = strdup(def);
    if (fn->definition == NULL) {
        free(fn);
        return ERR_OM;
    }
    fn->def_line = state->current_line_num;

    /* Add to list */
    fn->next = state->functions;
    state->functions = fn;

    return ERR_NONE;
}

/*
 * basic_call_function - Call a user-defined function
 *
 * Parameters:
 *   state  - BASIC interpreter state
 *   name   - Function name (single letter)
 *   arg    - Argument value
 *   result - Receives function result
 *
 * Returns:
 *   ERR_NONE on success, error code on failure
 */
ErrorCode basic_call_function(BasicState *state, char name, double arg, double *result)
{
    if (state == NULL || result == NULL) {
        return ERR_SN;
    }

    name = BASIC_TOUPPER(name);

    /* Find function */
    UserFunction *fn = state->functions;
    while (fn != NULL && fn->name != name) {
        fn = fn->next;
    }

    if (fn == NULL) {
        return ERR_UF;  /* Undefined function */
    }

    /* Save current parameter variable value */
    char param_name[2] = {fn->param, '\0'};
    Variable *param_var = basic_get_variable(state, param_name);
    double old_value = 0;
    bool had_value = (param_var != NULL);

    if (had_value) {
        old_value = param_var->value.v.number;
    }

    /* Set parameter to argument value */
    Value param_value;
    param_value.type = VAL_NUMBER;
    param_value.v.number = arg;
    basic_set_variable(state, param_name, &param_value);

    /* Evaluate function definition */
    char *saved_ptr = state->text_ptr;
    state->text_ptr = fn->definition;

    ErrorCode err = basic_eval_numeric(state, result);

    /* Restore text pointer */
    state->text_ptr = saved_ptr;

    /* Restore original parameter value */
    if (had_value) {
        param_value.v.number = old_value;
        basic_set_variable(state, param_name, &param_value);
    }

    return err;
}

/*
 * =============================================================================
 * VARIABLE CLEANUP
 * =============================================================================
 */

/*
 * basic_clear_variables - Clear all variables (CLEAR command)
 *
 * This implements the CLEAR statement, freeing all variables and arrays.
 *
 * Parameters:
 *   state - BASIC interpreter state
 */
void basic_clear_variables(BasicState *state)
{
    if (state == NULL) {
        return;
    }

    /* Free simple variables */
    Variable *var = state->variables;
    while (var != NULL) {
        Variable *next = var->next;
        /* Note: string data in string_space will be reclaimed */
        free(var);
        var = next;
    }
    state->variables = NULL;

    /* Free arrays */
    Array *arr = state->arrays;
    while (arr != NULL) {
        Array *next = arr->next;
        free(arr->data);
        free(arr);
        arr = next;
    }
    state->arrays = NULL;

    /* Free user functions */
    UserFunction *fn = state->functions;
    while (fn != NULL) {
        UserFunction *next = fn->next;
        free(fn->definition);
        free(fn);
        fn = next;
    }
    state->functions = NULL;

    /* Reset string space */
    state->string_ptr = state->string_space;

    /* Reset data pointer */
    state->data_ptr.line = NULL;
    state->data_ptr.position = NULL;
}
