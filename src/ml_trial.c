/*
 * ml_trial.c - MonkeyLogic trial wrapper for BHV2 files
 *
 * Provides trial iteration and metadata extraction for MonkeyLogic BHV2 files.
 * This is the domain-specific layer that interprets BHV2 variables as trials.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "ml_trial.h"

/* Fields needed for trial metadata (filtering and accessors) */
static const char *trial_metadata_fields[] = {
    "TrialError", "Condition", "Block", NULL
};

/************************************************************/
/* Helper functions
 */
/************************************************************/

/* Extract MonkeyLogic metadata from a trial variable */
static void extract_trial_info(ml_trial_file_t *file, bhv2_value_t *trial_value) {
    if (!file || !trial_value) return;
    
    /* Extract TrialError */
    bhv2_value_t *error_val = bhv2_struct_get(trial_value, "TrialError", 0);
    file->current_error_code = error_val ? (int)bhv2_get_double(error_val, 0) : -1;
    
    /* Extract Condition */
    bhv2_value_t *cond_val = bhv2_struct_get(trial_value, "Condition", 0);
    file->current_condition = cond_val ? (int)bhv2_get_double(cond_val, 0) : -1;
    
    /* Extract Block */
    bhv2_value_t *block_val = bhv2_struct_get(trial_value, "Block", 0);
    file->current_block = block_val ? (int)bhv2_get_double(block_val, 0) : -1;
}

/* Clear current trial state */
static void clear_trial_state(ml_trial_file_t *file) {
    if (!file) return;
    
    if (file->current_data) {
        bhv2_value_free(file->current_data);
        file->current_data = NULL;
    }
    
    file->current_trial_num = 0;
    file->current_error_code = -1;
    file->current_condition = -1;
    file->current_block = -1;
    file->has_current = false;
}

/************************************************************/
/* Public API
 */
/************************************************************/

/* Open MonkeyLogic BHV2 file */
ml_trial_file_t* open_input_file(const char *path) {
    ml_trial_file_t *file = calloc(1, sizeof(ml_trial_file_t));
    if (!file) return NULL;
    
    file->bhv2_file = bhv2_open_stream(path);
    if (!file->bhv2_file) {
        free(file);
        return NULL;
    }
    
    return file;
}

/* Close file and free resources */
void close_input_file(ml_trial_file_t *file) {
    if (!file) return;
    
    clear_trial_state(file);
    bhv2_file_free(file->bhv2_file);
    free(file);
}

/* Reset file position to beginning */
void rewind_input_file(ml_trial_file_t *file) {
    if (!file || !file->bhv2_file || file->bhv2_file->file_descriptor < 0) return;
    
    /* Clear current trial state */
    clear_trial_state(file);
    
    /* Rewind the underlying BHV2 file */
    bhv2_file_t *bhv2 = file->bhv2_file;
    lseek(bhv2->file_descriptor, 0, SEEK_SET);
    bhv2->current_pos = 0;
    bhv2->at_variable_data = false;
}

/* Set skip rules for trial filtering */
void set_skips(ml_trial_file_t *file, skip_set_t *skips) {
    if (file) {
        file->skips = skips;
    }
}

/* Read next trial */
int read_next_trial(ml_trial_file_t *file, int skip_data_flag) {
    if (!file) return -1;
    
    /* Clear previous trial */
    clear_trial_state(file);
    
    /* Iterate through BHV2 variables looking for trials */
    char *name;
    while (bhv2_read_next_variable_name(file->bhv2_file, &name) == 0) {
        /* Check if this is a Trial variable: "Trial1", "Trial2", etc. */
        if (strncmp(name, "Trial", 5) == 0 && isdigit(name[5])) {
            int trial_num = atoi(name + 5);
            free(name);
            
            /* Read trial data - selectively for SKIP_DATA, fully for WITH_DATA */
            bhv2_value_t *trial_data;
            if (skip_data_flag == SKIP_DATA) {
                /* Only read metadata fields, skip bulk data */
                trial_data = bhv2_read_variable_data_selective(file->bhv2_file, trial_metadata_fields);
            } else {
                /* Read everything */
                trial_data = bhv2_read_variable_data(file->bhv2_file);
            }
            if (!trial_data) return -1;
            
            /* Extract trial info */
            file->current_trial_num = trial_num;
            extract_trial_info(file, trial_data);
            
            /* Check if trial should be skipped */
            if (file->skips) {
                trial_info_t info = {
                    .trial_num = trial_num,
                    .error_code = file->current_error_code,
                    .condition = file->current_condition,
                    .block = file->current_block
                };
                if (skip_trial(file->skips, &info)) {
                    /* Skip this trial - free data and continue */
                    bhv2_value_free(trial_data);
                    clear_trial_state(file);
                    continue;
                }
            }
            
            /* Trial passed filters */
            file->has_current = true;
            
            if (skip_data_flag == SKIP_DATA) {
                /* Caller doesn't need data - free it */
                bhv2_value_free(trial_data);
            } else {
                /* WITH_DATA: Keep the data */
                file->current_data = trial_data;
            }
            
            return trial_num;
        } else {
            /* Not a trial - skip it */
            free(name);
            bhv2_skip_variable_data(file->bhv2_file);
        }
    }
    
    /* EOF reached */
    return 0;
}

/* Trial accessor functions */
int trial_number(ml_trial_file_t *file) {
    return file ? file->current_trial_num : 0;
}

int trial_error(ml_trial_file_t *file) {
    return file ? file->current_error_code : -1;
}

int trial_condition(ml_trial_file_t *file) {
    return file ? file->current_condition : -1;
}

int trial_block(ml_trial_file_t *file) {
    return file ? file->current_block : -1;
}

bhv2_value_t* trial_data(ml_trial_file_t *file) {
    return file ? file->current_data : NULL;
}
