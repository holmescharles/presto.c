/*
 * macros.c - Macro dispatcher and result utilities
 */

#define _POSIX_C_SOURCE 200809L  /* For strdup */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "macros.h"

/*
 * Result management
 */

void macro_result_init(macro_result_t *result) {
    result->text = NULL;
    result->length = 0;
}

void macro_result_free(macro_result_t *result) {
    free(result->text);
    result->text = NULL;
    result->length = 0;
}

void macro_result_set(macro_result_t *result, const char *text) {
    free(result->text);
    if (text) {
        result->length = strlen(text);
        result->text = strdup(text);
    } else {
        result->text = NULL;
        result->length = 0;
    }
}

void macro_result_append(macro_result_t *result, const char *text) {
    if (!text) return;
    
    size_t add_len = strlen(text);
    size_t new_len = result->length + add_len;
    
    char *new_text = realloc(result->text, new_len + 1);
    if (!new_text) return;
    
    if (result->length == 0) {
        new_text[0] = '\0';
    }
    
    strcat(new_text, text);
    result->text = new_text;
    result->length = new_len;
}

void macro_result_appendf(macro_result_t *result, const char *fmt, ...) {
    char buf[4096];
    va_list args;
    
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    macro_result_append(result, buf);
}

/*
 * Run macro by ID - dispatches to individual macro implementations
 */

int run_macro(int macro_id, bhv2_file_t *file, macro_result_t *result) {
    macro_result_init(result);
    
    switch (macro_id) {
        case 0: return macro_count(file, result);
        case 1: return macro_behavior(file, result);
        case 2: return macro_errors(file, result);
        case 3: return macro_scenes(file, result);
        case 4: return macro_analog(file, result);
        case 5: return macro_errorcounts(file, result);
        default:
            macro_result_set(result, "Unknown macro");
            return -1;
    }
}
