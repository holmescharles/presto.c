/*
 * macros.h - Text output macros for presto
 *
 * Macros iterate trials themselves using read_next_trial() (grab-style).
 * Skip filtering is handled internally by read_next_trial().
 */

#ifndef PRESTO_MACROS_H
#define PRESTO_MACROS_H

#include "ml_trial.h"

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
 * Returns 0 on success, -1 on error (unknown macro)
 */
int run_macro(int macro_id, ml_trial_file_t *file, macro_result_t *result);

/*
 * Individual macros (implementations in src/macros/)
 * Each macro iterates trials using read_next_trial(file, WITH_DATA/SKIP_DATA)
 */

/* Macro 0: Count trials */
int macro_count(ml_trial_file_t *file, macro_result_t *result);

/* Macro 1: Behavior summary */
int macro_behavior(ml_trial_file_t *file, macro_result_t *result);

/* Macro 2: Error code breakdown */
int macro_errors(ml_trial_file_t *file, macro_result_t *result);

/* Macro 3: Scene structure */
int macro_scenes(ml_trial_file_t *file, macro_result_t *result);

/* Macro 4: Analog data info */
int macro_analog(ml_trial_file_t *file, macro_result_t *result);

/* Macro 5: Error counts per condition */
int macro_errorcounts(ml_trial_file_t *file, macro_result_t *result);

#endif /* PRESTO_MACROS_H */
