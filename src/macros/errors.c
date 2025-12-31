/*
 * errors.c - Macro 2: Error breakdown
 */

#include "../macros.h"

int macro_errors(bhv2_file_t *file, macro_result_t *result) {
    /* Count error codes (MonkeyLogic uses 0-9) */
    int error_counts[10] = {0};
    int total = 0;

    while (read_next_trial(file, SKIP_DATA) > 0) {
        int error = trial_error_header(file);
        if (error >= 0 && error < 10) {
            error_counts[error]++;
        }
        total++;
    }
    
    /* Header */
    macro_result_append(result, "Error\tCount\tPercent\n");
    
    /* Output all error codes 0-9 */
    for (int e = 0; e < 10; e++) {
        double pct = total > 0 ? 100.0 * error_counts[e] / total : 0.0;
        macro_result_appendf(result, "%d\t%d\t%.1f%%\n", e, error_counts[e], pct);
    }
    
    return 0;
}
