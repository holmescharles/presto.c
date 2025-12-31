/*
 * presto_filter.c - Trial filtering implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "presto_filter.h"

/*
 * Filter set management
 */

filter_set_t* filter_set_new(void) {
    filter_set_t *fs = malloc(sizeof(filter_set_t));
    if (!fs) return NULL;
    
    fs->rules = NULL;
    fs->count = 0;
    fs->capacity = 0;
    return fs;
}

void filter_set_free(filter_set_t *fs) {
    if (!fs) return;
    
    for (size_t i = 0; i < fs->count; i++) {
        free(fs->rules[i].range.values);
    }
    free(fs->rules);
    free(fs);
}

static int filter_set_add(filter_set_t *fs, filter_rule_t rule) {
    if (fs->count >= fs->capacity) {
        size_t new_cap = fs->capacity == 0 ? 8 : fs->capacity * 2;
        filter_rule_t *new_rules = realloc(fs->rules, new_cap * sizeof(filter_rule_t));
        if (!new_rules) return -1;
        fs->rules = new_rules;
        fs->capacity = new_cap;
    }
    fs->rules[fs->count++] = rule;
    return 0;
}

/*
 * Range parsing
 */

filter_range_t filter_parse_range(const char *str) {
    filter_range_t range = {NULL, 0};
    
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

void filter_range_free(filter_range_t *range) {
    if (range) {
        free(range->values);
        range->values = NULL;
        range->count = 0;
    }
}

/*
 * Filter spec parsing
 */

int filter_parse_spec(filter_set_t *fs, const char *spec, bool is_include) {
    if (!fs || !spec || !*spec) return -1;
    
    filter_rule_t rule;
    rule.include = is_include;
    
    const char *p = spec;
    
    /* Determine type from first character */
    if (*p == 'E') {
        rule.type = FILTER_ERROR;
        p++;
    } else if (*p == 'c') {
        rule.type = FILTER_CONDITION;
        p++;
    } else if (isdigit(*p)) {
        rule.type = FILTER_TRIAL;
        /* Don't advance - the number is the range */
    } else {
        return -1;  /* Unknown type */
    }
    
    /* Parse the range */
    rule.range = filter_parse_range(p);
    if (rule.range.count == 0) {
        return -1;
    }
    
    return filter_set_add(fs, rule);
}

/*
 * Trial filtering
 */

bool filter_check_trial(filter_set_t *fs, trial_info_t *info) {
    if (!fs || fs->count == 0) return true;  /* No filters = include all */
    
    /* Track include/exclude state for each type */
    bool has_include_trial = false;
    bool has_include_error = false;
    bool has_include_condition = false;
    
    bool passed_include_trial = false;
    bool passed_include_error = false;
    bool passed_include_condition = false;
    
    /* First pass: check excludes and track includes */
    for (size_t i = 0; i < fs->count; i++) {
        filter_rule_t *rule = &fs->rules[i];
        
        int test_value;
        switch (rule->type) {
            case FILTER_TRIAL:
                test_value = info->trial_num;
                if (rule->include) has_include_trial = true;
                break;
            case FILTER_ERROR:
                test_value = info->error_code;
                if (rule->include) has_include_error = true;
                break;
            case FILTER_CONDITION:
                test_value = info->condition;
                if (rule->include) has_include_condition = true;
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
                    case FILTER_TRIAL: passed_include_trial = true; break;
                    case FILTER_ERROR: passed_include_error = true; break;
                    case FILTER_CONDITION: passed_include_condition = true; break;
                }
            }
        } else {
            /* Exclude rule - if matched, trial is excluded */
            if (in_range) return false;
        }
    }
    
    /* Check include constraints:
     * If there's an include rule for a type, trial must pass at least one */
    if (has_include_trial && !passed_include_trial) return false;
    if (has_include_error && !passed_include_error) return false;
    if (has_include_condition && !passed_include_condition) return false;
    
    return true;
}

/*
 * Trial info extraction from BHV2 (streaming version)
 */

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

/*
 * Trial list management
 */

trial_list_t* trial_list_new(void) {
    trial_list_t *list = calloc(1, sizeof(trial_list_t));
    if (!list) return NULL;
    
    list->capacity = 1000;  /* Initial capacity */
    list->trial_nums = malloc(list->capacity * sizeof(int));
    list->trial_data = malloc(list->capacity * sizeof(bhv2_value_t*));
    
    if (!list->trial_nums || !list->trial_data) {
        free(list->trial_nums);
        free(list->trial_data);
        free(list);
        return NULL;
    }
    
    return list;
}

int trial_list_add(trial_list_t *list, int trial_num, bhv2_value_t *trial_data) {
    if (!list) return -1;
    
    /* Grow if needed */
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        int *new_nums = realloc(list->trial_nums, new_capacity * sizeof(int));
        bhv2_value_t **new_data = realloc(list->trial_data, new_capacity * sizeof(bhv2_value_t*));
        
        if (!new_nums || !new_data) {
            free(new_nums);
            return -1;
        }
        
        list->trial_nums = new_nums;
        list->trial_data = new_data;
        list->capacity = new_capacity;
    }
    
    list->trial_nums[list->count] = trial_num;
    list->trial_data[list->count] = trial_data;
    list->count++;
    
    return 0;
}

void trial_list_free(trial_list_t *list) {
    if (!list) return;
    
    /* Free all trial data */
    for (size_t i = 0; i < list->count; i++) {
        bhv2_value_free(list->trial_data[i]);
    }
    
    free(list->trial_nums);
    free(list->trial_data);
    free(list);
}
