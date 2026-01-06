/************************************************************/
/* scenes.c - Macro 3: Scene structure
 * Shows the scene hierarchy from the first trial
 */
/************************************************************/

#include "../macros.h"

int macro_scenes(ml_trial_file_t *file, macro_result_t *result) {
    /* Read first trial to examine scene structure */
    int trial_num = read_next_trial(file, WITH_DATA);
    if (trial_num <= 0) {
        macro_result_set(result, "No trials");
        return 0;
    }
    
    bhv2_value_t *trial_value = trial_data(file);

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
