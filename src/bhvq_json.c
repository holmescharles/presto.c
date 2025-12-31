/*
 * bhvq_json.c - JSON output implementation
 */

#include "bhvq_json.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * Helper functions
 */

static void print_indent(json_opts_t *opts) {
    if (opts->compact) return;
    for (int i = 0; i < opts->indent; i++) {
        printf("  ");
    }
}

static void print_newline(json_opts_t *opts) {
    if (!opts->compact) {
        printf("\n");
    }
}

static void print_separator(json_opts_t *opts) {
    if (opts->compact) {
        printf(",");
    } else {
        printf(",\n");
    }
}

static void print_colon(json_opts_t *opts) {
    if (opts->compact) {
        printf(":");
    } else {
        printf(": ");
    }
}

/* Print JSON string with escaping */
static void print_json_string(const char *str) {
    printf("\"");
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\b': printf("\\b"); break;
            case '\f': printf("\\f"); break;
            case '\n': printf("\\n"); break;
            case '\r': printf("\\r"); break;
            case '\t': printf("\\t"); break;
            default:
                if ((unsigned char)*p < 32) {
                    printf("\\u%04x", (unsigned char)*p);
                } else {
                    putchar(*p);
                }
        }
    }
    printf("\"");
}

/* Print a double value, handling special cases */
static void print_json_number(double val) {
    if (isnan(val)) {
        printf("null");  /* JSON doesn't have NaN */
    } else if (isinf(val)) {
        printf("null");  /* JSON doesn't have Infinity */
    } else if (val == (long long)val && fabs(val) < 1e15) {
        /* Print as integer if it's a whole number */
        printf("%lld", (long long)val);
    } else {
        printf("%g", val);
    }
}

/*
 * JSON output
 */

void json_print_value(bhv2_value_t *val, json_opts_t *opts) {
    if (!val) {
        printf("null");
        return;
    }
    
    switch (val->dtype) {
        case MATLAB_CHAR:
            /* String */
            print_json_string(val->data.str ? val->data.str : "");
            break;
            
        case MATLAB_DOUBLE:
        case MATLAB_SINGLE:
        case MATLAB_UINT8:
        case MATLAB_UINT16:
        case MATLAB_UINT32:
        case MATLAB_UINT64:
        case MATLAB_INT8:
        case MATLAB_INT16:
        case MATLAB_INT32:
        case MATLAB_INT64:
        case MATLAB_LOGICAL:
            /* Numeric array */
            if (val->total == 1) {
                /* Scalar - unwrap */
                double d = bhv2_get_double(val, 0);
                if (val->dtype == MATLAB_LOGICAL) {
                    printf("%s", d != 0.0 ? "true" : "false");
                } else {
                    print_json_number(d);
                }
            } else {
                /* Array */
                printf("[");
                for (uint64_t i = 0; i < val->total; i++) {
                    if (i > 0) printf(",");
                    double d = bhv2_get_double(val, i);
                    if (val->dtype == MATLAB_LOGICAL) {
                        printf("%s", d != 0.0 ? "true" : "false");
                    } else {
                        print_json_number(d);
                    }
                }
                printf("]");
            }
            break;
            
        case MATLAB_STRUCT:
            if (val->total == 1) {
                /* Single struct - print as object */
                printf("{");
                print_newline(opts);
                opts->indent++;
                
                uint64_t n_fields = val->data.s.n_fields;
                for (uint64_t f = 0; f < n_fields; f++) {
                    bhv2_struct_field_t *field = &val->data.s.fields[f];
                    
                    if (f > 0) print_separator(opts);
                    print_indent(opts);
                    print_json_string(field->name);
                    print_colon(opts);
                    json_print_value(field->value, opts);
                }
                
                opts->indent--;
                print_newline(opts);
                print_indent(opts);
                printf("}");
            } else {
                /* Struct array - print as array of objects */
                printf("[");
                print_newline(opts);
                opts->indent++;
                
                uint64_t n_fields = val->data.s.n_fields;
                for (uint64_t elem = 0; elem < val->total; elem++) {
                    if (elem > 0) print_separator(opts);
                    print_indent(opts);
                    printf("{");
                    print_newline(opts);
                    opts->indent++;
                    
                    uint64_t base = elem * n_fields;
                    for (uint64_t f = 0; f < n_fields; f++) {
                        bhv2_struct_field_t *field = &val->data.s.fields[base + f];
                        
                        if (f > 0) print_separator(opts);
                        print_indent(opts);
                        print_json_string(field->name);
                        print_colon(opts);
                        json_print_value(field->value, opts);
                    }
                    
                    opts->indent--;
                    print_newline(opts);
                    print_indent(opts);
                    printf("}");
                }
                
                opts->indent--;
                print_newline(opts);
                print_indent(opts);
                printf("]");
            }
            break;
            
        case MATLAB_CELL:
            /* Cell array - print as array */
            if (val->total == 1) {
                /* Single cell - unwrap */
                json_print_value(val->data.cells[0], opts);
            } else {
                printf("[");
                print_newline(opts);
                opts->indent++;
                
                for (uint64_t i = 0; i < val->total; i++) {
                    if (i > 0) print_separator(opts);
                    print_indent(opts);
                    json_print_value(val->data.cells[i], opts);
                }
                
                opts->indent--;
                print_newline(opts);
                print_indent(opts);
                printf("]");
            }
            break;
            
        default:
            printf("null");
            break;
    }
}

void json_print_result(query_result_t *result, json_opts_t *opts) {
    print_indent(opts);
    print_json_string(result->path);
    print_colon(opts);
    json_print_value(result->value, opts);
}

void json_print_results(query_result_t *results, json_opts_t *opts) {
    if (!results) {
        printf("null\n");
        return;
    }
    
    /* Count results */
    int count = 0;
    for (query_result_t *r = results; r; r = r->next) count++;
    
    if (count == 1) {
        /* Single result - just print the value */
        json_print_value(results->value, opts);
        printf("\n");
    } else {
        /* Multiple results - print as object with paths as keys */
        printf("{");
        print_newline(opts);
        opts->indent++;
        
        bool first = true;
        for (query_result_t *r = results; r; r = r->next) {
            if (!first) print_separator(opts);
            first = false;
            json_print_result(r, opts);
        }
        
        opts->indent--;
        print_newline(opts);
        printf("}\n");
    }
}
