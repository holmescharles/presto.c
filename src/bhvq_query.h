/*
 * bhvq_query.h - Query parsing and pattern matching for bhvq
 */

#ifndef BHVQ_QUERY_H
#define BHVQ_QUERY_H

#include "bhv2.h"
#include <stdbool.h>

/*
 * Query segment - one part of a dot-separated path
 */

typedef struct {
    char *field;        /* Field name (may contain wildcards) */
    char *index_expr;   /* Index expression like "1", "1,2", "1,:" or NULL */
    bool has_wildcard;  /* Contains * or {..} */
} query_segment_t;

/*
 * Parsed query
 */

typedef struct {
    query_segment_t *segments;
    int n_segments;
} query_t;

/*
 * Query result - linked list of path/value pairs
 */

typedef struct query_result query_result_t;
struct query_result {
    char *path;           /* Full path like "Trial1.AnalogData.Button" */
    bhv2_value_t *value;  /* Reference to value (not owned) */
    query_result_t *next;
};

/*
 * Pattern expansion
 */

/* Expand brace patterns {1..5} or {1,2,3} into list of strings */
/* Returns number of expanded patterns, stores in *out (caller frees) */
int expand_pattern(const char *pattern, char ***out);

/* Free expanded pattern list */
void free_patterns(char **patterns, int count);

/*
 * Pattern matching
 */

/* Check if name matches pattern (supports * wildcards) */
bool match_glob(const char *name, const char *pattern);

/* Check if name matches any of the patterns */
bool match_any_pattern(const char *name, char **patterns, int n_patterns);

/*
 * Query parsing
 */

/* Parse query expression into segments */
query_t* query_parse(const char *expr);

/* Free parsed query */
void query_free(query_t *q);

/* Check if query has any pattern syntax */
bool query_has_patterns(query_t *q);

/*
 * Query execution
 */

/* Execute query on a file, return results */
query_result_t* query_execute(bhv2_file_t *file, query_t *q);

/* Execute query on a value (for recursive navigation) */
query_result_t* query_execute_on_value(bhv2_value_t *val, query_t *q, 
                                        int segment_idx, const char *path_prefix);

/* Free query results (does NOT free the values, just the result list) */
void query_result_free(query_result_t *results);

/*
 * Index parsing and application
 */

/* Apply MATLAB-style index expression to value, return indexed element */
/* Returns NULL if index out of bounds or invalid */
bhv2_value_t* apply_index(bhv2_value_t *val, const char *index_expr);

#endif /* BHVQ_QUERY_H */
