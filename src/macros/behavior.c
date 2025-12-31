/*
 * behavior.c - Macro 1: Behavior summary
 */

#include "../macros.h"

int macro_behavior(bhv2_file_t *file, macro_result_t *result) {
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
    
    /* Format output */
    macro_result_appendf(result, "Trials: %d\n", total);
    
    if (total > 0) {
        int correct = error_counts[0];
        double pct = 100.0 * correct / total;
        macro_result_appendf(result, "Correct: %d (%.1f%%)\n", correct, pct);
        
        macro_result_append(result, "Errors:\n");
        for (int e = 0; e < 10; e++) {
            double epct = 100.0 * error_counts[e] / total;
            macro_result_appendf(result, "  E%d: %d (%.1f%%)\n", e, error_counts[e], epct);
        }
    }
    
    return 0;
}
