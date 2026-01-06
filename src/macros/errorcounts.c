/************************************************************/
/* errorcounts.c - Macro 5: Error counts per condition
 */
/************************************************************/

#include "../macros.h"

int macro_errorcounts(ml_trial_file_t *file, macro_result_t *result) {
    /* Build a map of condition -> error code -> count */
    /* For simplicity, use fixed arrays (assume max 100 conditions, 10 error codes) */
    #define MAX_COND 100
    int counts[MAX_COND][10] = {{0}};
    int cond_totals[MAX_COND] = {0};
    int max_cond = -1;
    
    while (read_next_trial(file, SKIP_DATA) > 0) {
        int cond = trial_condition(file);
        int error = trial_error(file);
        
        if (cond >= 0 && cond < MAX_COND && error >= 0 && error < 10) {
            counts[cond][error]++;
            cond_totals[cond]++;
            if (cond > max_cond) max_cond = cond;
        }
    }
    
    if (max_cond < 0) {
        macro_result_set(result, "No data");
        return 0;
    }
    
    /* Header: Cond, E0, E1, ..., E9, Total */
    macro_result_append(result, "Cond");
    for (int e = 0; e < 10; e++) {
        macro_result_appendf(result, "\tE%d", e);
    }
    macro_result_append(result, "\tTotal\n");
    
    /* Rows */
    for (int c = 1; c <= max_cond; c++) {
        if (cond_totals[c] == 0) continue;
        
        macro_result_appendf(result, "%d", c);
        for (int e = 0; e < 10; e++) {
            macro_result_appendf(result, "\t%d", counts[c][e]);
        }
        macro_result_appendf(result, "\t%d\n", cond_totals[c]);
    }
    
    #undef MAX_COND
    return 0;
}
