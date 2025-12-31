/*
 * count.c - Macro 0: Count trials
 */

#include "../macros.h"

int macro_count(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    (void)file;  /* Unused */
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%zu", trials->count);
    macro_result_set(result, buf);
    return 0;
}
