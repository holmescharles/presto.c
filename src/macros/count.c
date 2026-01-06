/************************************************************/
/* count.c - Macro 0: Count trials
 */
/************************************************************/

#include "../macros.h"

int macro_count(ml_trial_file_t *file, macro_result_t *result) {
    int count = 0;
    
    while (read_next_trial(file, SKIP_DATA) > 0) {
        count++;
    }
    
    macro_result_appendf(result, "%d", count);
    return 0;
}
