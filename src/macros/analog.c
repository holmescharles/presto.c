/*
 * analog.c - Macro 4: Analog data info
 */

#include "../macros.h"

int macro_analog(bhv2_file_t *file, macro_result_t *result) {
    /* Read first trial to examine analog structure */
    int trial_num = read_next_trial(file, WITH_DATA);
    if (trial_num <= 0) {
        macro_result_set(result, "No trials");
        return 0;
    }
    
    bhv2_value_t *trial_value = trial_data(file);

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
