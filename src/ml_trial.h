/*
 * ml_trial.h - MonkeyLogic trial wrapper for BHV2 files
 *
 * This module provides a MonkeyLogic-specific interpretation layer on top
 * of the generic BHV2 format parser. It understands:
 *   - "Trial1", "Trial2" variable naming convention
 *   - MonkeyLogic trial fields: TrialError, Condition, Block
 *   - Trial filtering/skipping based on metadata
 *
 * The underlying bhv2.{h,c} knows nothing about trials - it only reads
 * the BHV2 format (MATLAB variables). This layer adds the MonkeyLogic
 * domain semantics.
 */

#ifndef ML_TRIAL_H
#define ML_TRIAL_H

#include <stdbool.h>
#include "bhv2.h"
#include "skip.h"

/************************************************************/
/* MonkeyLogic trial file handle
 */
/************************************************************/

typedef struct {
    bhv2_file_t *bhv2_file;          /* Generic BHV2 format parser */
    skip_set_t *skips;               /* Trial filtering rules */
    
    /* Current trial state (populated by read_next_trial) */
    int current_trial_num;           /* Trial number (1-based, from "Trial123") */
    int current_error_code;          /* TrialError field value */
    int current_condition;           /* Condition field value */
    int current_block;               /* Block field value */
    bhv2_value_t *current_data;      /* Full trial struct (NULL if SKIP_DATA) */
    bool has_current;                /* True if current trial is valid */
} ml_trial_file_t;

/************************************************************/
/* Constants for read_next_trial()
 */
/************************************************************/

#define WITH_DATA 0
#define SKIP_DATA 1

/************************************************************/
/* Grab-style API
 */
/************************************************************/

/* Open MonkeyLogic BHV2 file (grab-style naming) */
ml_trial_file_t* open_input_file(const char *path);

/* Close file and free resources */
void close_input_file(ml_trial_file_t *file);

/* Reset file position to beginning */
void rewind_input_file(ml_trial_file_t *file);

/* Set skip rules for trial filtering */
void set_skips(ml_trial_file_t *file, skip_set_t *skips);

/* Read next trial (returns trial number, 0 on EOF, negative on error)
 * skip_data_flag: WITH_DATA or SKIP_DATA
 * 
 * Iterates through BHV2 variables looking for "Trial1", "Trial2", etc.
 * Extracts MonkeyLogic metadata (TrialError, Condition, Block).
 * Applies skip filters if configured.
 * Populates current trial state accessible via trial_*() functions.
 */
int read_next_trial(ml_trial_file_t *file, int skip_data_flag);

/* Trial accessor functions (grab-style)
 * These return values from the current trial loaded by read_next_trial()
 */
int trial_number(ml_trial_file_t *file);
int trial_error(ml_trial_file_t *file);
int trial_condition(ml_trial_file_t *file);
int trial_block(ml_trial_file_t *file);
bhv2_value_t* trial_data(ml_trial_file_t *file);

#endif /* ML_TRIAL_H */
