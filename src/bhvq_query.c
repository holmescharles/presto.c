/*
 * bhvq_query.c - Query parsing and pattern matching implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "bhvq_query.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/*
 * Pattern expansion
 */

int expand_pattern(const char *pattern, char ***out) {
    /* Find brace expansion: prefix{...}suffix */
    const char *brace_start = strchr(pattern, '{');
    if (!brace_start) {
        /* No braces - return pattern as-is */
        *out = malloc(sizeof(char*));
        (*out)[0] = strdup(pattern);
        return 1;
    }
    
    const char *brace_end = strchr(brace_start, '}');
    if (!brace_end) {
        /* Unclosed brace - return as literal */
        *out = malloc(sizeof(char*));
        (*out)[0] = strdup(pattern);
        return 1;
    }
    
    /* Extract prefix, inside, suffix */
    size_t prefix_len = brace_start - pattern;
    char *prefix = malloc(prefix_len + 1);
    strncpy(prefix, pattern, prefix_len);
    prefix[prefix_len] = '\0';
    
    size_t inside_len = brace_end - brace_start - 1;
    char *inside = malloc(inside_len + 1);
    strncpy(inside, brace_start + 1, inside_len);
    inside[inside_len] = '\0';
    
    const char *suffix = brace_end + 1;
    
    /* Check for range syntax: 1..10 */
    char *dotdot = strstr(inside, "..");
    if (dotdot) {
        *dotdot = '\0';
        char *start_str = inside;
        char *end_str = dotdot + 2;
        
        char *endptr;
        long start = strtol(start_str, &endptr, 10);
        if (*endptr != '\0') {
            /* Invalid start number */
            free(prefix);
            free(inside);
            *out = malloc(sizeof(char*));
            (*out)[0] = strdup(pattern);
            return 1;
        }
        
        long end = strtol(end_str, &endptr, 10);
        if (*endptr != '\0') {
            /* Invalid end number */
            free(prefix);
            free(inside);
            *out = malloc(sizeof(char*));
            (*out)[0] = strdup(pattern);
            return 1;
        }
        
        if (start > end) {
            /* Backwards range - return empty */
            free(prefix);
            free(inside);
            *out = NULL;
            return 0;
        }
        
        int count = (int)(end - start + 1);
        *out = malloc(count * sizeof(char*));
        
        for (long i = start; i <= end; i++) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s%ld%s", prefix, i, suffix);
            (*out)[i - start] = strdup(buf);
        }
        
        free(prefix);
        free(inside);
        return count;
    }
    
    /* Check for list syntax: 1,2,3 */
    if (strchr(inside, ',')) {
        /* Count items */
        int count = 1;
        for (const char *p = inside; *p; p++) {
            if (*p == ',') count++;
        }
        
        *out = malloc(count * sizeof(char*));
        
        char *saveptr;
        char *item = strtok_r(inside, ",", &saveptr);
        int i = 0;
        while (item && i < count) {
            /* Trim whitespace */
            while (*item && isspace(*item)) item++;
            char *end = item + strlen(item) - 1;
            while (end > item && isspace(*end)) *end-- = '\0';
            
            char buf[256];
            snprintf(buf, sizeof(buf), "%s%s%s", prefix, item, suffix);
            (*out)[i++] = strdup(buf);
            
            item = strtok_r(NULL, ",", &saveptr);
        }
        
        free(prefix);
        free(inside);
        return i;
    }
    
    /* Unknown brace content - return as literal */
    free(prefix);
    free(inside);
    *out = malloc(sizeof(char*));
    (*out)[0] = strdup(pattern);
    return 1;
}

void free_patterns(char **patterns, int count) {
    if (!patterns) return;
    for (int i = 0; i < count; i++) {
        free(patterns[i]);
    }
    free(patterns);
}

/*
 * Pattern matching
 */

bool match_glob(const char *name, const char *pattern) {
    /* Simple glob matcher supporting * wildcard */
    while (*pattern) {
        if (*pattern == '*') {
            pattern++;
            if (!*pattern) return true;  /* Trailing * matches everything */
            
            /* Try to match rest of pattern at each position */
            while (*name) {
                if (match_glob(name, pattern)) return true;
                name++;
            }
            return false;
        }
        
        if (*name != *pattern) return false;
        name++;
        pattern++;
    }
    
    return *name == '\0';
}

bool match_any_pattern(const char *name, char **patterns, int n_patterns) {
    for (int i = 0; i < n_patterns; i++) {
        if (match_glob(name, patterns[i])) return true;
    }
    return false;
}

/*
 * Query parsing
 */

static bool has_pattern_syntax(const char *s) {
    return strchr(s, '*') != NULL || strchr(s, '{') != NULL;
}

query_t* query_parse(const char *expr) {
    if (!expr || !*expr || strcmp(expr, ".") == 0) {
        /* Empty query - return all variables */
        query_t *q = calloc(1, sizeof(query_t));
        return q;
    }
    
    query_t *q = calloc(1, sizeof(query_t));
    
    /* Count segments by scanning for dots outside parens/braces */
    int depth = 0;
    int n_segments = 1;
    for (const char *p = expr; *p; p++) {
        if (*p == '(' || *p == '{') depth++;
        else if (*p == ')' || *p == '}') depth--;
        else if (*p == '.' && depth == 0) n_segments++;
    }
    
    q->segments = calloc(n_segments, sizeof(query_segment_t));
    q->n_segments = n_segments;
    
    /* Parse segments */
    const char *seg_start = expr;
    int seg_idx = 0;
    depth = 0;
    
    for (const char *p = expr; ; p++) {
        if (*p == '(' || *p == '{') depth++;
        else if (*p == ')' || *p == '}') depth--;
        
        if ((*p == '.' && depth == 0) || *p == '\0') {
            /* End of segment */
            size_t seg_len = p - seg_start;
            char *segment = malloc(seg_len + 1);
            strncpy(segment, seg_start, seg_len);
            segment[seg_len] = '\0';
            
            /* Check for index expression: Field(1,2) */
            char *paren = strchr(segment, '(');
            if (paren) {
                *paren = '\0';
                q->segments[seg_idx].field = strdup(segment);
                
                /* Extract index expression (without parens) */
                char *close = strrchr(paren + 1, ')');
                if (close) *close = '\0';
                q->segments[seg_idx].index_expr = strdup(paren + 1);
            } else {
                q->segments[seg_idx].field = strdup(segment);
                q->segments[seg_idx].index_expr = NULL;
            }
            
            q->segments[seg_idx].has_wildcard = has_pattern_syntax(q->segments[seg_idx].field);
            
            free(segment);
            seg_idx++;
            seg_start = p + 1;
            
            if (*p == '\0') break;
        }
    }
    
    return q;
}

void query_free(query_t *q) {
    if (!q) return;
    for (int i = 0; i < q->n_segments; i++) {
        free(q->segments[i].field);
        free(q->segments[i].index_expr);
    }
    free(q->segments);
    free(q);
}

bool query_has_patterns(query_t *q) {
    if (!q) return false;
    for (int i = 0; i < q->n_segments; i++) {
        if (q->segments[i].has_wildcard) return true;
    }
    return false;
}

/*
 * Query result management
 */

static query_result_t* result_new(const char *path, bhv2_value_t *value) {
    query_result_t *r = calloc(1, sizeof(query_result_t));
    r->path = strdup(path);
    r->value = value;
    return r;
}

static void result_append(query_result_t **head, query_result_t *new_result) {
    if (!*head) {
        *head = new_result;
        return;
    }
    
    query_result_t *tail = *head;
    while (tail->next) tail = tail->next;
    tail->next = new_result;
}

void query_result_free(query_result_t *results) {
    while (results) {
        query_result_t *next = results->next;
        free(results->path);
        /* Note: we don't free results->value - it's owned by the file */
        free(results);
        results = next;
    }
}

/*
 * Index application
 */

bhv2_value_t* apply_index(bhv2_value_t *val, const char *index_expr) {
    if (!val || !index_expr) return val;
    
    /* Parse comma-separated indices */
    char *expr_copy = strdup(index_expr);
    char *saveptr;
    char *idx_str = strtok_r(expr_copy, ",", &saveptr);
    
    uint64_t indices[BHV2_MAX_NDIMS];
    int n_indices = 0;
    bool has_colon = false;
    
    while (idx_str && n_indices < BHV2_MAX_NDIMS) {
        /* Trim whitespace */
        while (*idx_str && isspace(*idx_str)) idx_str++;
        char *end = idx_str + strlen(idx_str) - 1;
        while (end > idx_str && isspace(*end)) *end-- = '\0';
        
        if (strcmp(idx_str, ":") == 0) {
            has_colon = true;
            indices[n_indices++] = 0;  /* Placeholder for "all" */
        } else {
            char *endptr;
            long idx = strtol(idx_str, &endptr, 10);
            if (*endptr != '\0' || idx < 1) {
                free(expr_copy);
                return NULL;  /* Invalid index */
            }
            indices[n_indices++] = (uint64_t)idx;
        }
        
        idx_str = strtok_r(NULL, ",", &saveptr);
    }
    
    free(expr_copy);
    
    if (n_indices == 0) return val;
    
    /* For now, only support single linear index (no slicing in C version yet) */
    if (has_colon) {
        /* Slicing not fully implemented - return whole value for now */
        return val;
    }
    
    /* Single index: convert 1-based to 0-based linear index */
    if (n_indices == 1) {
        uint64_t idx = indices[0] - 1;
        
        if (val->dtype == MATLAB_STRUCT) {
            /* For struct array, index selects which struct element */
            /* Return a reference to that element's fields */
            /* This is tricky - for now, return the whole struct if index is 1 */
            if (idx == 0 && val->total == 1) {
                return val;
            }
            /* TODO: implement struct array indexing properly */
            return NULL;
        }
        
        if (val->dtype == MATLAB_CELL) {
            if (idx < val->total) {
                return val->data.cells[idx];
            }
            return NULL;
        }
        
        /* Numeric array - can't return single element as value easily */
        /* For now, return whole array */
        return val;
    }
    
    /* Multi-dimensional index */
    if ((uint64_t)n_indices == val->ndims) {
        uint64_t linear_idx = bhv2_sub2ind(val, indices, n_indices);
        /* Similar issue - can't easily return single element */
        (void)linear_idx;
    }
    
    return val;
}

/*
 * Query execution
 */

query_result_t* query_execute_on_value(bhv2_value_t *val, query_t *q,
                                        int segment_idx, const char *path_prefix) {
    if (!val || segment_idx >= q->n_segments) {
        /* Base case: no more segments, return current value */
        return result_new(path_prefix, val);
    }
    
    query_segment_t *seg = &q->segments[segment_idx];
    query_result_t *results = NULL;
    
    /* Value must be a struct to navigate into */
    if (val->dtype != MATLAB_STRUCT) {
        /* Can't navigate into non-struct */
        return NULL;
    }
    
    /* Get patterns to match */
    char **patterns;
    int n_patterns;
    
    if (seg->has_wildcard) {
        n_patterns = expand_pattern(seg->field, &patterns);
    } else {
        patterns = malloc(sizeof(char*));
        patterns[0] = strdup(seg->field);
        n_patterns = 1;
    }
    
    /* Iterate through struct fields */
    uint64_t n_fields = val->data.s.n_fields;
    
    for (uint64_t elem = 0; elem < val->total; elem++) {
        uint64_t base = elem * n_fields;
        
        for (uint64_t f = 0; f < n_fields; f++) {
            bhv2_struct_field_t *field = &val->data.s.fields[base + f];
            
            if (match_any_pattern(field->name, patterns, n_patterns)) {
                /* Build new path */
                char new_path[1024];
                if (path_prefix && *path_prefix) {
                    snprintf(new_path, sizeof(new_path), "%s.%s", path_prefix, field->name);
                } else {
                    snprintf(new_path, sizeof(new_path), "%s", field->name);
                }
                
                /* Apply index if present */
                bhv2_value_t *field_val = field->value;
                if (seg->index_expr) {
                    field_val = apply_index(field_val, seg->index_expr);
                    if (!field_val) continue;
                }
                
                /* Recurse to next segment */
                query_result_t *sub_results = query_execute_on_value(
                    field_val, q, segment_idx + 1, new_path);
                
                /* Append results */
                while (sub_results) {
                    query_result_t *next = sub_results->next;
                    sub_results->next = NULL;
                    result_append(&results, sub_results);
                    sub_results = next;
                }
            }
        }
    }
    
    free_patterns(patterns, n_patterns);
    return results;
}

query_result_t* query_execute(bhv2_file_t *file, query_t *q) {
    if (!file || !q) return NULL;
    
    /* Empty query - return all top-level variables */
    if (q->n_segments == 0) {
        query_result_t *results = NULL;
        for (uint64_t i = 0; i < file->n_vars; i++) {
            result_append(&results, result_new(file->vars[i].name, file->vars[i].value));
        }
        return results;
    }
    
    query_result_t *results = NULL;
    query_segment_t *first_seg = &q->segments[0];
    
    /* Get patterns for first segment */
    char **patterns;
    int n_patterns;
    
    if (first_seg->has_wildcard) {
        n_patterns = expand_pattern(first_seg->field, &patterns);
    } else {
        patterns = malloc(sizeof(char*));
        patterns[0] = strdup(first_seg->field);
        n_patterns = 1;
    }
    
    /* Find matching top-level variables */
    for (uint64_t i = 0; i < file->n_vars; i++) {
        if (match_any_pattern(file->vars[i].name, patterns, n_patterns)) {
            bhv2_value_t *val = file->vars[i].value;
            
            /* Apply index if present */
            if (first_seg->index_expr) {
                val = apply_index(val, first_seg->index_expr);
                if (!val) continue;
            }
            
            if (q->n_segments == 1) {
                /* Only one segment - return this variable */
                result_append(&results, result_new(file->vars[i].name, val));
            } else {
                /* More segments - recurse */
                query_result_t *sub_results = query_execute_on_value(
                    val, q, 1, file->vars[i].name);
                
                while (sub_results) {
                    query_result_t *next = sub_results->next;
                    sub_results->next = NULL;
                    result_append(&results, sub_results);
                    sub_results = next;
                }
            }
        }
    }
    
    free_patterns(patterns, n_patterns);
    return results;
}
