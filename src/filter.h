/*
 * presto_filter.h - Trial filtering for presto
 *
 * Implements grab-style filtering syntax:
 *   -XE0        Include only error code 0 (correct trials)
 *   -xE1:3      Exclude error codes 1-3
 *   -Xc2:5      Include conditions 2-5
 *   -X1:10      Include trials 1-10
 */

#ifndef PRESTO_FILTER_H
#define PRESTO_FILTER_H

#include <stdint.h>
#include <stdbool.h>
#include "bhv2.h"

/*
 * Filter specification types
 */

typedef enum {
    FILTER_TRIAL,      /* Trial number */
    FILTER_ERROR,      /* Error code (TrialError field) */
    FILTER_CONDITION   /* Condition number */
} filter_type_t;

/*
 * Range specification: single value, range, or union
 */

typedef struct {
    int *values;       /* Array of values to match */
    size_t count;      /* Number of values */
} filter_range_t;

/*
 * Single filter rule
 */

typedef struct {
    filter_type_t type;
    bool include;       /* true = include only, false = exclude */
    filter_range_t range;
} filter_rule_t;

/*
 * Filter set - all rules to apply
 */

typedef struct {
    filter_rule_t *rules;
    size_t count;
    size_t capacity;
} filter_set_t;

/*
 * Trial info extracted from BHV2 for filtering
 */

typedef struct {
    int trial_num;      /* 1-based trial number */
    int error_code;     /* TrialError value */
    int condition;      /* Condition number */
} trial_info_t;

/*
 * Filtered trial list (streaming version)
 */

typedef struct {
    int *trial_nums;        /* Array of 1-based trial numbers that passed filter */
    bhv2_value_t **trial_data; /* Actual trial data for macros */
    size_t count;
    size_t capacity;
} trial_list_t;

/*
 * Filter operations
 */

/* Create empty filter set */
filter_set_t* filter_set_new(void);

/* Free filter set */
void filter_set_free(filter_set_t *fs);

/* Parse a filter spec string and add to filter set
 * spec format: [E|c]<range> where range is N, N:M, or N,M,O
 * is_include: true for -X (include), false for -x (exclude)
 * Returns 0 on success, -1 on error
 */
int filter_parse_spec(filter_set_t *fs, const char *spec, bool is_include);

/* Parse a range string into values
 * Handles: "5", "1:10", "1,3,5"
 * Returns filter_range_t with allocated values array
 */
filter_range_t filter_parse_range(const char *str);

/* Free range values */
void filter_range_free(filter_range_t *range);

/* Check if a trial passes all filters
 * Returns true if trial should be included
 */
bool filter_check_trial(filter_set_t *fs, trial_info_t *info);

/* Create new trial list */
trial_list_t* trial_list_new(void);

/* Add trial to list */
int trial_list_add(trial_list_t *list, int trial_num, bhv2_value_t *trial_data);

/* Free trial list */
void trial_list_free(trial_list_t *list);

/*
 * Utility functions (streaming version)
 */

/* Get trial error code from trial variable data */
int get_trial_error_from_value(bhv2_value_t *trial_value);

/* Get trial condition from trial variable data */
int get_trial_condition_from_value(bhv2_value_t *trial_value);

#endif /* PRESTO_FILTER_H */
