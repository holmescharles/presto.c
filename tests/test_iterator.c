/*
 * test_iterator.c - Test the new grab-style trial iterator API
 */

#include <stdio.h>
#include "src/bhv2.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.bhv2>\n", argv[0]);
        return 1;
    }
    
    /* Open file */
    bhv2_file_t *file = open_input_file(argv[1]);
    if (!file) {
        fprintf(stderr, "Failed to open %s\n", argv[1]);
        return 1;
    }
    
    printf("=== Test 1: Iterate with SKIP_DATA ===\n");
    int count = 0;
    while (read_next_trial(file, SKIP_DATA)) {
        int num = trial_number_header(file);
        int error = trial_error_header(file);
        int cond = condition_header(file);
        printf("Trial %d: Error=%d, Condition=%d\n", num, error, cond);
        count++;
        if (count >= 5) break;  /* Just show first 5 */
    }
    
    printf("\n=== Test 2: Rewind and iterate with WITH_DATA ===\n");
    rewind_input_file(file);
    count = 0;
    while (read_next_trial(file, WITH_DATA)) {
        int num = trial_number_header(file);
        int error = trial_error_header(file);
        bhv2_value_t *data = trial_data_header(file);
        printf("Trial %d: Error=%d, has_data=%s\n", 
               num, error, data ? "yes" : "no");
        count++;
        if (count >= 5) break;  /* Just show first 5 */
    }
    
    printf("\n=== Test 3: Count all trials ===\n");
    rewind_input_file(file);
    int total = 0;
    while (read_next_trial(file, SKIP_DATA)) {
        total++;
    }
    printf("Total trials: %d\n", total);
    
    close_input_file(file);
    return 0;
}
