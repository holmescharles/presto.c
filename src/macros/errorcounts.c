/*
 * errorcounts.c - Macro 5: Error counts per condition
 */

#include "../macros.h"

int macro_errorcounts(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    (void)file;  /* Unused */
    
    /* Build a map of condition -> error code -> count */
    /* For simplicity, use fixed arrays (assume max 100 conditions, 256 errors) */
    #define MAX_COND 100
    int counts[MAX_COND][256] = {{0}};
    int cond_totals[MAX_COND] = {0};
    int max_cond = -1;
    int max_error = -1;
    
    for (size_t i = 0; i < trials->count; i++) {
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
