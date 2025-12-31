/*
 * macros.c - Text output macro implementations
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
 * Run macro by ID
 */

int run_macro(int macro_id, bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    macro_result_init(result);
    
    switch (macro_id) {
        case 0: return macro_count(file, trials, result);
        case 1: return macro_behavior(file, trials, result);
        case 2: return macro_errors(file, trials, result);
        case 3: return macro_scenes(file, trials, result);
        case 4: return macro_analog(file, trials, result);
        case 5: return macro_errorcounts(file, trials, result);
        default:
            macro_result_set(result, "Unknown macro");
            return -1;
    }
}

/*
 * Macro 0: Count trials
 */

int macro_count(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    (void)file;  /* Unused */
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%zu", trials->count);
    macro_result_set(result, buf);
    return 0;
}

/*
 * Macro 1: Behavior summary
 */

int macro_behavior(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    /* Count error codes */
    int error_counts[256] = {0};
    int max_error = -1;

    for (size_t i = 0; i < trials->count; i++) {
        int trial_num = trials->trial_nums[i];
        int error = get_trial_error_from_value(trials->trial_data[i]);
        if (error >= 0 && error < 256) {
            error_counts[error]++;
            if (error > max_error) max_error = error;
        }
    }
    
    /* Format output */
    macro_result_appendf(result, "Trials: %zu\n", trials->count);
    
    if (trials->count > 0 && max_error >= 0) {
        int correct = error_counts[0];
        double pct = 100.0 * correct / trials->count;
        macro_result_appendf(result, "Correct: %d (%.1f%%)\n", correct, pct);
        
        macro_result_append(result, "Errors:\n");
        for (int e = 0; e <= max_error; e++) {
            if (error_counts[e] > 0) {
                double epct = 100.0 * error_counts[e] / trials->count;
                macro_result_appendf(result, "  E%d: %d (%.1f%%)\n", e, error_counts[e], epct);
            }
        }
    }
    
    return 0;
}

/*
 * Macro 2: Error breakdown
 */

int macro_errors(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    /* Count error codes */
    int error_counts[256] = {0};
    int max_error = -1;

    for (size_t i = 0; i < trials->count; i++) {
        int trial_num = trials->trial_nums[i];
        int error = get_trial_error_from_value(trials->trial_data[i]);
        if (error >= 0 && error < 256) {
            error_counts[error]++;
            if (error > max_error) max_error = error;
        }
    }
    
    /* Header */
    macro_result_append(result, "Error\tCount\tPercent\n");
    
    /* Output each error code */
    for (int e = 0; e <= max_error; e++) {
        if (error_counts[e] > 0) {
            double pct = 100.0 * error_counts[e] / trials->count;
            macro_result_appendf(result, "%d\t%d\t%.1f%%\n", e, error_counts[e], pct);
        }
    }
    
    return 0;
}

/*
 * Macro 3: Scene structure
 * Shows the scene hierarchy from the first trial
 */

int macro_scenes(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    if (trials->count == 0) {
        macro_result_set(result, "No trials");
        return 0;
    }
    
    /* Get first trial to examine scene structure */
    int trial_num = trials->trial_nums[0];
    bhv2_value_t *trial_value = trials->trial_data[0];

    /* Get ObjectStatusRecord */
    bhv2_value_t *osr = bhv2_struct_get(trial_value, "ObjectStatusRecord", 0);
    if (!osr) {
        macro_result_set(result, "No ObjectStatusRecord");
        return 0;
    }
    
    macro_result_appendf(result, "ObjectStatusRecord from Trial %d:\n", trial_num);
    
    /* OSR is typically a struct with Scene fields */
    if (osr->dtype == MATLAB_STRUCT) {
        for (uint64_t i = 0; i < osr->data.struct_array.n_fields; i++) {
            const char *fname = osr->data.struct_array.fields[i].name;
            macro_result_appendf(result, "  %s\n", fname);
        }
    } else if (osr->dtype == MATLAB_CELL) {
        macro_result_appendf(result, "  Cell array with %lu elements\n", (unsigned long)osr->total);
    } else {
        macro_result_appendf(result, "  Type: %s\n", matlab_dtype_to_string(osr->dtype));
    }
    
    return 0;
}

/*
 * Macro 4: Analog data info
 */

int macro_analog(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    if (trials->count == 0) {
        macro_result_set(result, "No trials");
        return 0;
    }
    
    /* Get first trial */
    int trial_num = trials->trial_nums[0];
    bhv2_value_t *trial_value = trials->trial_data[0];

    /* Get AnalogData */
    bhv2_value_t *analog = bhv2_struct_get(trial_value, "AnalogData", 0);
    if (!analog) {
        macro_result_set(result, "No AnalogData");
        return 0;
    }
    
    macro_result_appendf(result, "AnalogData from Trial %d:\n", trial_num);
    
    if (analog->dtype == MATLAB_STRUCT) {
        for (uint64_t i = 0; i < analog->data.struct_array.n_fields; i++) {
            const char *fname = analog->data.struct_array.fields[i].name;
            bhv2_value_t *fval = analog->data.struct_array.fields[i].value;
            
            if (fval) {
                macro_result_appendf(result, "  %s: %s [", fname, matlab_dtype_to_string(fval->dtype));
                for (uint64_t d = 0; d < fval->ndims; d++) {
                    if (d > 0) macro_result_append(result, "x");
                    macro_result_appendf(result, "%lu", (unsigned long)fval->dims[d]);
                }
                macro_result_append(result, "]\n");
            } else {
                macro_result_appendf(result, "  %s: (null)\n", fname);
            }
        }
    } else {
        macro_result_appendf(result, "  Type: %s\n", matlab_dtype_to_string(analog->dtype));
    }
    
    return 0;
}

/*
 * Macro 5: Error counts per condition
 */

int macro_errorcounts(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    /* Build a map of condition -> error code -> count */
    /* For simplicity, use fixed arrays (assume max 100 conditions, 256 errors) */
    #define MAX_COND 100
    int counts[MAX_COND][256] = {{0}};
    int cond_totals[MAX_COND] = {0};
    int max_cond = -1;
    int max_error = -1;
    
    for (size_t i = 0; i < trials->count; i++) {
        int trial_num = trials->trial_nums[i];
        int cond = get_trial_condition_from_value(trials->trial_data[i]);
        int error = get_trial_error_from_value(trials->trial_data[i]);
        
        if (cond >= 0 && cond < MAX_COND && error >= 0 && error < 256) {
            counts[cond][error]++;
            cond_totals[cond]++;
            if (cond > max_cond) max_cond = cond;
            if (error > max_error) max_error = error;
        }
    }
    
    if (max_cond < 0 || max_error < 0) {
        macro_result_set(result, "No data");
        return 0;
    }
    
    /* Header: Cond, E0, E1, ..., Total */
    macro_result_append(result, "Cond");
    for (int e = 0; e <= max_error; e++) {
        macro_result_appendf(result, "\tE%d", e);
    }
    macro_result_append(result, "\tTotal\n");
    
    /* Rows */
    for (int c = 1; c <= max_cond; c++) {
        if (cond_totals[c] == 0) continue;
        
        macro_result_appendf(result, "%d", c);
        for (int e = 0; e <= max_error; e++) {
            macro_result_appendf(result, "\t%d", counts[c][e]);
        }
        macro_result_appendf(result, "\t%d\n", cond_totals[c]);
    }
    
    #undef MAX_COND
    return 0;
}
