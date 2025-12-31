/*
 * presto_macros.h - Text output macros for presto
 */

#ifndef PRESTO_MACROS_H
#define PRESTO_MACROS_H

#include "bhv2.h"
#include "presto_filter.h"

/*
 * Macro result - text output from a macro
 */

typedef struct {
    char *text;         /* Allocated text output */
    size_t length;      /* Length of text */
} macro_result_t;

/*
 * Initialize/free result
 */

void macro_result_init(macro_result_t *result);
void macro_result_free(macro_result_t *result);

/*
 * Set result text (copies string)
 */
void macro_result_set(macro_result_t *result, const char *text);

/*
 * Append to result text
 */
void macro_result_append(macro_result_t *result, const char *text);
void macro_result_appendf(macro_result_t *result, const char *fmt, ...);

/*
 * Run a macro by ID
 * Returns 0 on success, -1 on error
 */
int run_macro(int macro_id, bhv2_file_t *file, trial_list_t *trials, macro_result_t *result);

/*
 * Individual macros
 */

/* Macro 0: Count trials */
int macro_count(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result);

/* Macro 1: Behavior summary */
int macro_behavior(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result);

/* Macro 2: Error code breakdown */
int macro_errors(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result);

/* Macro 3: Scene structure */
int macro_scenes(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result);

/* Macro 4: Analog data info */
int macro_analog(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result);

/* Macro 5: Error counts per condition */
int macro_errorcounts(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result);

#endif /* PRESTO_MACROS_H */
