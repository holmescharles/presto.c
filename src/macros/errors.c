/*
 * errors.c - Macro 2: Error breakdown
 */

#include "../macros.h"

int macro_errors(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    (void)file;  /* Unused */
    
    /* Count error codes */
    int error_counts[256] = {0};
    int max_error = -1;

    for (size_t i = 0; i < trials->count; i++) {
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
