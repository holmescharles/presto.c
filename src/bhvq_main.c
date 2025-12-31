/*
 * bhvq_main.c - bhvq CLI entry point (DISABLED)
 *
 * Query BHV2 behavioral data files using MATLAB-like syntax.
 * Outputs JSON.
 *
 * Usage: bhvq [options] <query> <file>
 *
 * NOTE: Temporarily disabled while refactoring to use streaming API.
 */

#if 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "bhv2.h"
#include "bhvq_query.h"
#include "bhvq_json.h"

#define BHVQ_VERSION "1.0.0"

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options] <query> <file>\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Query BHV2 behavioral data files using MATLAB-like syntax.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Positional arguments:\n");
    fprintf(stderr, "  query   Query expression (default: '.')\n");
    fprintf(stderr, "          Examples: Field, Struct.Field, Array(1), Trial*.Data\n");
    fprintf(stderr, "  file    BHV2 file to query\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -c, --compact      Output compact JSON (single line)\n");
    fprintf(stderr, "  -l, --list         List top-level variables only\n");
    fprintf(stderr, "  -e, --exit-status  Exit with 1 if query returns null/empty\n");
    fprintf(stderr, "  -V, --version      Show version and exit\n");
    fprintf(stderr, "  -h, --help         Show this help and exit\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Query syntax:\n");
    fprintf(stderr, "  Field              Access field by name\n");
    fprintf(stderr, "  Struct.Field       Dot navigation into structs\n");
    fprintf(stderr, "  Array(1)           1-based indexing (like MATLAB)\n");
    fprintf(stderr, "  Array(1,2)         Multi-dimensional indexing\n");
    fprintf(stderr, "  Array(1,:)         Slicing (row 1, all columns)\n");
    fprintf(stderr, "  Trial*             Glob wildcard\n");
    fprintf(stderr, "  Trial{1..10}       Range expansion\n");
    fprintf(stderr, "  Trial{1,5,10}      List expansion\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s . data.bhv2                    List all variables\n", prog);
    fprintf(stderr, "  %s FileInfo data.bhv2             Get FileInfo struct\n", prog);
    fprintf(stderr, "  %s 'Trial1.AnalogData' data.bhv2  Get Trial1's AnalogData\n", prog);
    fprintf(stderr, "  %s 'Trial*.ErrorCode' data.bhv2   Get ErrorCode from all trials\n", prog);
}

static void print_version(void) {
    printf("bhvq %s\n", BHVQ_VERSION);
}

static void list_variables(bhv2_file_t *file) {
    for (uint64_t i = 0; i < file->n_variables; i++) {
        bhv2_variable_t *variable = &file->variables[i];
        bhv2_value_t *val = variable->value;
        
        /* Print name and type info */
        printf("%s", var->name);
        
        if (val) {
            printf(": ");
            
            /* Size */
            if (val->ndims == 1) {
                printf("%llux1 ", (unsigned long long)val->dims[0]);
            } else if (val->ndims == 2) {
                printf("%llux%llu ", 
                       (unsigned long long)val->dims[0],
                       (unsigned long long)val->dims[1]);
            } else if (val->ndims > 2) {
                for (uint64_t d = 0; d < val->ndims; d++) {
                    if (d > 0) printf("x");
                    printf("%llu", (unsigned long long)val->dims[d]);
                }
                printf(" ");
            }
            
            /* Type */
            printf("%s", matlab_dtype_to_string(val->dtype));
            
            /* Field count for structs */
            if (val->dtype == MATLAB_STRUCT) {
                printf(" (%llu fields)", (unsigned long long)val->data.s.n_fields);
            }
        }
        
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    int opt;
    bool compact = false;
    bool list_only = false;
    bool exit_status = false;
    
    static struct option long_options[] = {
        {"compact",     no_argument, 0, 'c'},
        {"list",        no_argument, 0, 'l'},
        {"exit-status", no_argument, 0, 'e'},
        {"version",     no_argument, 0, 'V'},
        {"help",        no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "cleVh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                compact = true;
                break;
            case 'l':
                list_only = true;
                break;
            case 'e':
                exit_status = true;
                break;
            case 'V':
                print_version();
                return 0;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Parse positional arguments */
    const char *query_expr = ".";
    const char *filepath = NULL;
    
    int remaining = argc - optind;
    if (remaining == 1) {
        /* Just file, default query */
        filepath = argv[optind];
    } else if (remaining == 2) {
        /* Query and file */
        query_expr = argv[optind];
        filepath = argv[optind + 1];
    } else {
        fprintf(stderr, "Error: expected <query> <file> or <file>\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Open file */
    bhv2_file_t *file = bhv2_open(filepath);
    if (!file) {
        fprintf(stderr, "Error: failed to open %s: %s\n", 
                filepath, bhv2_error_detail[0] ? bhv2_error_detail : bhv2_strerror(bhv2_last_error));
        return 1;
    }
    
    /* List mode */
    if (list_only) {
        list_variables(file);
        bhv2_file_free(file);
        return 0;
    }
    
    /* Parse and execute query */
    query_t *q = query_parse(query_expr);
    if (!q) {
        fprintf(stderr, "Error: failed to parse query: %s\n", query_expr);
        bhv2_file_free(file);
        return 1;
    }
    
    query_result_t *results = query_execute(file, q);
    
    /* Output results */
    json_opts_t opts = {
        .compact = compact,
        .indent = 0
    };
    
    json_print_results(results, &opts);
    
    /* Determine exit code */
    int exit_code = 0;
    if (exit_status && !results) {
        exit_code = 1;
    }
    
    /* Cleanup */
    query_result_free(results);
    query_free(q);
    bhv2_file_free(file);

    return exit_code;
}
#endif /* 0 - bhvq temporarily disabled */
