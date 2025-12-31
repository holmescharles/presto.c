/*
 * presto_main.c - CLI entry point for presto behavioral data analyzer
 */

#define _POSIX_C_SOURCE 200809L  /* For strdup */

/*
 *
 * Usage: presto [options] <file.bhv2> [files...]
 *
 * Options:
 *   -XE<N>      Include only error code N (e.g., -XE0 for correct)
 *   -xE<N:M>    Exclude error codes N through M
 *   -Xc<N>      Include only condition N
 *   -xc<N:M>    Exclude conditions N through M
 *   -X<N:M>     Include only trials N through M
 *   -x<N:M>     Exclude trials N through M
 *   -o<N>       Text output macro N (default: 0 = count)
 *   -g<N>       Graphical output macro N
 *   -O <dir>    Output directory (default: current dir, "-" for stdout)
 *   -f          Force overwrite existing files
 *   -l          List available macros
 *   -h          Show help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <getopt.h>
#include "bhv2.h"
#include "skip.h"
#include "macros.h"
#include "macros/plot.h"

#define PRESTO_VERSION "0.1.0"

/*
 * Macro registry
 */

typedef struct {
    int id;
    const char *name;
    const char *description;
    bool is_graphical;
} macro_info_t;

static macro_info_t macros[] = {
    {0, "count", "Count trials (filtered)", false},
    {1, "behavior", "Behavior summary (error codes, conditions)", false},
    {2, "errors", "Error code breakdown", false},
    {3, "scenes", "Scene structure", false},
    {4, "analog", "Analog data info", false},
    {5, "errorcounts", "Error counts per condition", false},
    {-1, NULL, NULL, false}  /* Sentinel */
};

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options] <file.bhv2> [files...]\n", prog);
    fprintf(stderr, "\nTrial filtering:\n");
    fprintf(stderr, "  -XE<spec>   Include only error codes (e.g., -XE0, -XE1:3)\n");
    fprintf(stderr, "  -xE<spec>   Exclude error codes\n");
    fprintf(stderr, "  -Xc<spec>   Include only conditions\n");
    fprintf(stderr, "  -xc<spec>   Exclude conditions\n");
    fprintf(stderr, "  -X<spec>    Include only trials (e.g., -X1:10)\n");
    fprintf(stderr, "  -x<spec>    Exclude trials\n");
    fprintf(stderr, "\nOutput:\n");
    fprintf(stderr, "  -o<N>       Text output macro (default: 0)\n");
    fprintf(stderr, "  -g<N>       Graphical output macro\n");
    fprintf(stderr, "  -O <dir>    Output directory ('-' for stdout)\n");
    fprintf(stderr, "  -s <WxH>    Plot size in inches (default: 11x8.5, e.g., -s 8x6)\n");
    fprintf(stderr, "  -f          Force overwrite existing files\n");
    fprintf(stderr, "\nInfo:\n");
    fprintf(stderr, "  -M          List available macros\n");
    fprintf(stderr, "  -h          Show this help\n");
    fprintf(stderr, "  -V          Show version\n");
    fprintf(stderr, "\nSpec format: N (single), N:M (range), N,M,O (union)\n");
}

static void print_macros(void) {
    printf("Text macros:\n");
    for (int i = 0; macros[i].name != NULL; i++) {
        if (!macros[i].is_graphical) {
            printf("  -o%d  %s\n", macros[i].id, macros[i].description);
        }
    }
    printf("\nGraphical macros:\n");
    printf("  -g1  Plot analog data (PDF)\n");
    printf("  -g2  Plot timeline (PDF)\n");
}

/*
 * Parse filter arguments from argv
 * Returns index of first non-option argument (file)
 */

typedef struct {
    skip_set_t *skips;
    int output_macro;
    int graph_macro;
    char *output_dir;
    bool to_stdout;
    bool force;
    bool list_macros;
    bool show_help;
    bool show_version;
    int first_file_idx;
    double plot_width;   /* Plot width in inches */
    double plot_height;  /* Plot height in inches */
} presto_args_t;

static void args_init(presto_args_t *args) {
    args->skips = skip_set_new();
    args->output_macro = 0;  /* Default to count */
    args->graph_macro = -1;  /* No graph by default */
    args->output_dir = NULL;
    args->to_stdout = false;
    args->force = false;
    args->list_macros = false;
    args->show_help = false;
    args->show_version = false;
    args->first_file_idx = -1;
    args->plot_width = 11.0;   /* Default: 11 inches wide */
    args->plot_height = 8.5;   /* Default: 8.5 inches tall */
}

static void args_free(presto_args_t *args) {
    skip_set_free(args->skips);
    free(args->output_dir);
}

static int parse_args(int argc, char **argv, presto_args_t *args) {
    args_init(args);
    
    int i = 1;
    while (i < argc) {
        char *arg = argv[i];
        
        if (arg[0] != '-') {
            /* First non-option is file */
            args->first_file_idx = i;
            return 0;
        }
        
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            args->show_help = true;
            return 0;
        }
        
        if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
            args->show_version = true;
            return 0;
        }
        
        if (strcmp(arg, "-M") == 0) {
            args->list_macros = true;
            return 0;
        }
        
        if (strcmp(arg, "-f") == 0) {
            args->force = true;
            i++;
            continue;
        }
        
        if (strcmp(arg, "-O") == 0) {
            /* Output directory - next arg */
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -O requires a directory argument\n");
                return -1;
            }
            i++;
            if (strcmp(argv[i], "-") == 0) {
                args->to_stdout = true;
            } else {
                args->output_dir = strdup(argv[i]);
            }
            i++;
            continue;
        }
        
        if (strcmp(arg, "-s") == 0) {
            /* Plot size - next arg in format WxH */
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -s requires size argument (e.g., -s 11x8.5)\n");
                return -1;
            }
            i++;
            char *size_str = argv[i];
            char *x_pos = strchr(size_str, 'x');
            if (!x_pos) {
                fprintf(stderr, "Error: Invalid size format '%s' (use WxH, e.g., 11x8.5)\n", size_str);
                return -1;
            }
            *x_pos = '\0';  /* Split at 'x' */
            args->plot_width = atof(size_str);
            args->plot_height = atof(x_pos + 1);
            
            if (args->plot_width <= 0 || args->plot_height <= 0) {
                fprintf(stderr, "Error: Invalid plot dimensions: %gx%g (must be positive)\n",
                        args->plot_width, args->plot_height);
                return -1;
            }
            i++;
            continue;
        }
        
        /* Filter: -X (include) or -x (exclude) */
        if (arg[0] == '-' && (arg[1] == 'X' || arg[1] == 'x')) {
            bool is_include = (arg[1] == 'X');
            const char *spec = arg + 2;
            
            if (*spec == '\0') {
                fprintf(stderr, "Error: -%c requires a spec (e.g., -%cE0, -%c1:10)\n",
                        arg[1], arg[1], arg[1]);
                return -1;
            }
            
            if (skip_parse_spec(args->skips, spec, is_include) != 0) {
                fprintf(stderr, "Error: Invalid filter spec: %s\n", arg);
                return -1;
            }
            i++;
            continue;
        }
        
        /* Output macro: -o<N> */
        if (arg[0] == '-' && arg[1] == 'o' && arg[2] != '\0') {
            args->output_macro = atoi(arg + 2);
            args->graph_macro = -1;  /* Text output takes precedence */
            i++;
            continue;
        }
        
        /* Graph macro: -g<N> */
        if (arg[0] == '-' && arg[1] == 'g' && arg[2] != '\0') {
            args->graph_macro = atoi(arg + 2);
            i++;
            continue;
        }
        
        /* Unknown option */
        fprintf(stderr, "Error: Unknown option: %s\n", arg);
        return -1;
    }
    
    return 0;
}

/*
 * Main
 */

int main(int argc, char **argv) {
    presto_args_t args;
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    if (parse_args(argc, argv, &args) != 0) {
        args_free(&args);
        return 1;
    }
    
    if (args.show_help) {
        print_usage(argv[0]);
        args_free(&args);
        return 0;
    }
    
    if (args.show_version) {
        printf("presto %s\n", PRESTO_VERSION);
        args_free(&args);
        return 0;
    }
    
    if (args.list_macros) {
        print_macros();
        args_free(&args);
        return 0;
    }
    
    if (args.first_file_idx < 0 || args.first_file_idx >= argc) {
        fprintf(stderr, "Error: No input files specified\n");
        print_usage(argv[0]);
        args_free(&args);
        return 1;
    }
    
    /* Process each file */
    int status = 0;
    for (int i = args.first_file_idx; i < argc; i++) {
        const char *filepath = argv[i];
        
        /* Open BHV2 file with streaming API */
        bhv2_file_t *file = bhv2_open_stream(filepath);
        if (!file) {
            fprintf(stderr, "Error: Failed to open %s: %s\n", filepath, bhv2_error_detail);
            status = 1;
            continue;
        }
        
        /* Stream through variables and filter trials */
        trial_list_t *trials = trial_list_new();
        if (!trials) {
            fprintf(stderr, "Error: Failed to allocate trial list\n");
            bhv2_file_free(file);
            status = 1;
            continue;
        }
        
        char *name;
        while (bhv2_read_next_variable_name(file, &name) == 0) {
            /* Check if this is a Trial variable (e.g., "Trial1", "Trial2", ...) */
            if (strncmp(name, "Trial", 5) == 0 && isdigit(name[5])) {
                int trial_num = atoi(name + 5);
                
                /* Read the trial data */
                bhv2_value_t *trial_data = bhv2_read_variable_data(file);
                if (!trial_data) {
                    fprintf(stderr, "Warning: Failed to read trial %d data\n", trial_num);
                    free(name);
                    continue;
                }
                
                /* Extract trial info for filtering */
                trial_info_t info = {
                    .trial_num = trial_num,
                    .error_code = get_trial_error_from_value(trial_data),
                    .condition = get_trial_condition_from_value(trial_data)
                };
                
                /* Check if trial should be skipped */
                if (skip_trial(args.skips, &info)) {
                    /* Trial should be skipped - free it */
                    bhv2_value_free(trial_data);
                } else {
                    /* Trial should be kept - add to list */
                    if (trial_list_add(trials, trial_num, trial_data) < 0) {
                        fprintf(stderr, "Error: Failed to add trial to list\n");
                        bhv2_value_free(trial_data);
                        free(name);
                        break;
                    }
                }
            } else {
                /* Not a trial variable - skip it */
                if (bhv2_skip_variable_data(file) < 0) {
                    fprintf(stderr, "Warning: Failed to skip variable %s\n", name);
                    free(name);
                    break;
                }
            }
            
            free(name);
        }
        
        /* Run the appropriate macro */
        if (args.graph_macro >= 0) {
            /* Graphical output */
            const char *output_path = args.output_dir ? args.output_dir : ".";
            int plot_status = run_plot_macro(args.graph_macro, file, trials, filepath, output_path,
                                            args.plot_width, args.plot_height);
            if (plot_status != 0) {
                fprintf(stderr, "Error: Plot generation failed\n");
                status = 1;
            }
        } else {
            /* Text output */
            macro_result_t result;
            int macro_status = run_macro(args.output_macro, file, trials, &result);
            
            if (macro_status != 0) {
                fprintf(stderr, "Error: Unknown macro -o%d\n\n", args.output_macro);
                fprintf(stderr, "Available text macros:\n");
                for (int i = 0; macros[i].name != NULL; i++) {
                    fprintf(stderr, "  -o%d  %s\n", macros[i].id, macros[i].description);
                }
                fprintf(stderr, "\nUse '%s -M' to list all macros.\n", argv[0]);
                status = 1;
            } else {
                /* Output result */
                if (args.to_stdout || args.output_dir == NULL) {
                    /* Print to stdout */
                    if (argc - args.first_file_idx > 1) {
                        /* Multiple files - prefix with filename */
                        printf("%s\t%s\n", filepath, result.text ? result.text : "");
                    } else {
                        printf("%s\n", result.text ? result.text : "");
                    }
                } else {
                    /* Write to file */
                    /* TODO: Implement file output */
                    printf("%s\n", result.text ? result.text : "");
                }
                
                macro_result_free(&result);
            }
        }
        
        trial_list_free(trials);
        bhv2_file_free(file);
    }
    
    args_free(&args);
    return status;
}
