/*
 * skip.h - Trial skipping for presto
 *
 * Implements grab-style skip syntax:
 *   -XE0        Include only error code 0 (correct trials)
 *   -xE1:3      Exclude error codes 1-3
 *   -Xc2:5      Include conditions 2-5
 *   -X1:10      Include trials 1-10
 */

#ifndef SKIP_H
#define SKIP_H

#include <stdint.h>
#include <stdbool.h>
#include "bhv2.h"

/*
 * Skip specification types
 */

typedef enum {
    SKIP_BY_TRIAL,      /* Skip by trial number */
    SKIP_BY_ERROR,      /* Skip by error code (TrialError field) */
    SKIP_BY_CONDITION,  /* Skip by condition number */
    SKIP_BY_BLOCK       /* Skip by block number (Block field) */
} skip_type_t;

/*
 * Range specification: single value, range, or union
 */

typedef struct {
    int *values;       /* Array of values to match */
    size_t count;      /* Number of values */
} skip_range_t;

/*
 * Single skip rule
 */

typedef struct {
    skip_type_t type;
    bool include;       /* true = include only, false = exclude */
    skip_range_t range;
} skip_rule_t;

/*
 * Skip set - all rules to apply
 */

typedef struct skip_set {
    skip_rule_t *rules;
    size_t count;
    size_t capacity;
} skip_set_t;

/*
 * Trial info extracted from BHV2 for skip checking
 */

typedef struct {
    int trial_num;      /* 1-based trial number */
    int error_code;     /* TrialError value */
    int condition;      /* Condition number */
    int block;          /* Block number (Block field) */
} trial_info_t;

/*
 * Skip operations
 */

/* Create empty skip set */
skip_set_t* skip_set_new(void);

/* Free skip set */
void skip_set_free(skip_set_t *ss);

/* Parse a skip spec string and add to skip set
 * spec format: [E|c]<range> where range is N, N:M, or N,M,O
 * is_include: true for -X (include), false for -x (exclude)
 * Returns 0 on success, -1 on error
 */
int skip_parse_spec(skip_set_t *ss, const char *spec, bool is_include);

/* Parse a range string into values
 * Handles: "5", "1:10", "1,3,5"
 * Returns skip_range_t with allocated values array
 */
skip_range_t skip_parse_range(const char *str);

/* Free range values */
void skip_range_free(skip_range_t *range);

/* Check if a trial should be skipped
 * Returns true if trial should be skipped (excluded)
 */
bool skip_trial(skip_set_t *ss, trial_info_t *info);

/*
 * Utility functions for extracting trial info from BHV2 values
 */

/* Get trial error code from trial variable data */
int get_trial_error_from_value(bhv2_value_t *trial_value);

/* Get trial condition from trial variable data */
int get_trial_condition_from_value(bhv2_value_t *trial_value);

/* Get trial block number from trial variable data */
int get_trial_block_from_value(bhv2_value_t *trial_value);

#endif /* SKIP_H */
