/*
 * bhvq_json.h - JSON output for bhvq
 */

#ifndef BHVQ_JSON_H
#define BHVQ_JSON_H

#include "bhv2.h"
#include "bhvq_query.h"
#include <stdio.h>
#include <stdbool.h>

/*
 * JSON output options
 */

typedef struct {
    bool compact;       /* Single line, no whitespace */
    int indent;         /* Current indentation level */
} json_opts_t;

/*
 * JSON output functions
 */

/* Print a value as JSON to stdout */
void json_print_value(bhv2_value_t *val, json_opts_t *opts);

/* Print query results as JSON */
void json_print_results(query_result_t *results, json_opts_t *opts);

/* Print a single result (path: value) as JSON object */
void json_print_result(query_result_t *result, json_opts_t *opts);

#endif /* BHVQ_JSON_H */
