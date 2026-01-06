/*
 * skip.c - Trial skipping implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "skip.h"

/************************************************************/
/* Skip set management
 */
/************************************************************/

skip_set_t* skip_set_new(void) {
    skip_set_t *ss = malloc(sizeof(skip_set_t));
    if (!ss) return NULL;
    
    ss->rules = NULL;
    ss->count = 0;
    ss->capacity = 0;
    return ss;
}

void skip_set_free(skip_set_t *ss) {
    if (!ss) return;
    
    for (size_t i = 0; i < ss->count; i++) {
        free(ss->rules[i].range.values);
    }
    free(ss->rules);
    free(ss);
}

static int skip_set_add(skip_set_t *ss, skip_rule_t rule) {
    if (ss->count >= ss->capacity) {
        size_t new_cap = ss->capacity == 0 ? 8 : ss->capacity * 2;
        skip_rule_t *new_rules = realloc(ss->rules, new_cap * sizeof(skip_rule_t));
        if (!new_rules) return -1;
        ss->rules = new_rules;
        ss->capacity = new_cap;
    }
    ss->rules[ss->count++] = rule;
    return 0;
}

/************************************************************/
/* Range parsing
 */
/************************************************************/

skip_range_t skip_parse_range(const char *str) {
    skip_range_t range = {NULL, 0};
    
    if (!str || !*str) return range;
    
    /* Count commas to estimate capacity */
    size_t capacity = 16;
    range.values = malloc(capacity * sizeof(int));
    if (!range.values) return range;
    
    const char *p = str;
    while (*p) {
        /* Skip whitespace */
        while (*p && isspace(*p)) p++;
        if (!*p) break;
        
        /* Parse number */
        char *end;
        long start = strtol(p, &end, 10);
        if (end == p) {
            /* Not a number - skip */
            p++;
            continue;
        }
        p = end;
        
        /* Check for range separator ':' */
        if (*p == ':') {
            p++;
            long stop = strtol(p, &end, 10);
            if (end == p) {
                /* Invalid range, just add start */
                if (range.count >= capacity) {
                    capacity *= 2;
                    int *new_vals = realloc(range.values, capacity * sizeof(int));
                    if (!new_vals) break;
                    range.values = new_vals;
                }
                range.values[range.count++] = (int)start;
            } else {
                p = end;
                /* Add all values in range [start, stop] */
                for (long v = start; v <= stop; v++) {
                    if (range.count >= capacity) {
                        capacity *= 2;
                        int *new_vals = realloc(range.values, capacity * sizeof(int));
                        if (!new_vals) break;
                        range.values = new_vals;
                    }
                    range.values[range.count++] = (int)v;
                }
            }
        } else {
            /* Single value */
            if (range.count >= capacity) {
                capacity *= 2;
                int *new_vals = realloc(range.values, capacity * sizeof(int));
                if (!new_vals) break;
                range.values = new_vals;
            }
            range.values[range.count++] = (int)start;
        }
        
        /* Skip comma separator */
        while (*p && (isspace(*p) || *p == ',')) p++;
    }
    
    return range;
}

void skip_range_free(skip_range_t *range) {
    if (range) {
        free(range->values);
        range->values = NULL;
        range->count = 0;
    }
}

/************************************************************/
/* Skip spec parsing
 */
/************************************************************/

int skip_parse_spec(skip_set_t *ss, const char *spec, bool is_include) {
    if (!ss || !spec || !*spec) return -1;
    
    skip_rule_t rule;
    rule.include = is_include;
    
    const char *p = spec;
    
    /* Determine type from first character */
    if (*p == 'E') {
        rule.type = SKIP_BY_ERROR;
        p++;
    } else if (*p == 'c') {
        rule.type = SKIP_BY_CONDITION;
        p++;
    } else if (*p == 'B') {
        rule.type = SKIP_BY_BLOCK;
        p++;
    } else if (isdigit(*p)) {
        rule.type = SKIP_BY_TRIAL;
        /* Don't advance - the number is the range */
    } else {
        return -1;  /* Unknown type */
    }
    
    /* Parse the range */
    rule.range = skip_parse_range(p);
    if (rule.range.count == 0) {
        return -1;
    }
    
    return skip_set_add(ss, rule);
}

/************************************************************/
/* Trial skipping
 */
/************************************************************/

bool skip_trial(skip_set_t *ss, trial_info_t *info) {
    if (!ss || ss->count == 0) return false;  /* No rules = skip nothing (keep all) */
    
    /* Track include/exclude state for each type */
    bool has_include_trial = false;
    bool has_include_error = false;
    bool has_include_condition = false;
    bool has_include_block = false;
    
    bool passed_include_trial = false;
    bool passed_include_error = false;
    bool passed_include_condition = false;
    bool passed_include_block = false;
    
    /* First pass: check excludes and track includes */
    for (size_t i = 0; i < ss->count; i++) {
        skip_rule_t *rule = &ss->rules[i];
        
        int test_value;
        switch (rule->type) {
            case SKIP_BY_TRIAL:
                test_value = info->trial_num;
                if (rule->include) has_include_trial = true;
                break;
            case SKIP_BY_ERROR:
                test_value = info->error_code;
                if (rule->include) has_include_error = true;
                break;
            case SKIP_BY_CONDITION:
                test_value = info->condition;
                if (rule->include) has_include_condition = true;
                break;
            case SKIP_BY_BLOCK:
                test_value = info->block;
                if (rule->include) has_include_block = true;
                break;
            default:
                continue;
        }
        
        /* Check if value is in range */
        bool in_range = false;
        for (size_t j = 0; j < rule->range.count; j++) {
            if (rule->range.values[j] == test_value) {
                in_range = true;
                break;
            }
        }
        
        if (rule->include) {
            /* Include rule - track if this type passed */
            if (in_range) {
                switch (rule->type) {
                    case SKIP_BY_TRIAL: passed_include_trial = true; break;
                    case SKIP_BY_ERROR: passed_include_error = true; break;
                    case SKIP_BY_CONDITION: passed_include_condition = true; break;
                    case SKIP_BY_BLOCK: passed_include_block = true; break;
                }
            }
        } else {
            /* Exclude rule - if matched, skip this trial */
            if (in_range) return true;
        }
    }
    
    /* Check include constraints:
     * If there's an include rule for a type, trial must pass at least one */
    if (has_include_trial && !passed_include_trial) return true;
    if (has_include_error && !passed_include_error) return true;
    if (has_include_condition && !passed_include_condition) return true;
    if (has_include_block && !passed_include_block) return true;
    
    return false;
}

/************************************************************/
/* Trial info extraction from BHV2 (streaming version)
 */
/************************************************************/

int get_trial_error_from_value(bhv2_value_t *trial_value) {
    if (!trial_value || trial_value->dtype != MATLAB_STRUCT) return -1;

    /* Trial is a struct - get TrialError field */
    bhv2_value_t *error_val = bhv2_struct_get(trial_value, "TrialError", 0);
    if (!error_val) return -1;

    return (int)bhv2_get_double(error_val, 0);
}

int get_trial_condition_from_value(bhv2_value_t *trial_value) {
    if (!trial_value || trial_value->dtype != MATLAB_STRUCT) return -1;

    /* Trial is a struct - get Condition field */
    bhv2_value_t *cond_val = bhv2_struct_get(trial_value, "Condition", 0);
    if (!cond_val) return -1;

    return (int)bhv2_get_double(cond_val, 0);
}

int get_trial_block_from_value(bhv2_value_t *trial_value) {
    if (!trial_value || trial_value->dtype != MATLAB_STRUCT) return -1;

    /* Trial is a struct - get Block field */
    bhv2_value_t *block_val = bhv2_struct_get(trial_value, "Block", 0);
    if (!block_val) return -1;

    return (int)bhv2_get_double(block_val, 0);
}
