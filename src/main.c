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
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include "ml_trial.h"
#include "skip.h"
#include "macros.h"
#include "macros/plot.h"

#define PRESTO_VERSION "0.1.0"

/************************************************************/
/* Macro registry
 */
/************************************************************/

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
    fprintf(stderr, "       %s [options] -    (read from stdin)\n", prog);
    fprintf(stderr, "\nTrial filtering:\n");
    fprintf(stderr, "  -XE<spec>   Include only error codes (e.g., -XE0, -XE1:3)\n");
    fprintf(stderr, "  -xE<spec>   Exclude error codes\n");
    fprintf(stderr, "  -Xc<spec>   Include only conditions\n");
    fprintf(stderr, "  -xc<spec>   Exclude conditions\n");
    fprintf(stderr, "  -XB<spec>   Include only blocks (e.g., -XB3, -XB2:4)\n");
    fprintf(stderr, "  -xB<spec>   Exclude blocks\n");
    fprintf(stderr, "  -X<spec>    Include only trials (e.g., -X1:10)\n");
    fprintf(stderr, "  -x<spec>    Exclude trials\n");
    fprintf(stderr, "\nOutput:\n");
    fprintf(stderr, "  -o<N>       Text output macro (default: 0)\n");
    fprintf(stderr, "  -g<N>       Graphical output macro\n");
    fprintf(stderr, "  -O <dir>    Output directory ('-' for stdout)\n");
    fprintf(stderr, "  -s <WxH>    Plot size in inches (default: 11x8.5, e.g., -s 8x6)\n");
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

/************************************************************/
/* Buffer stdin to a temp file for seeking
 * Returns path to temp file (caller must free and unlink)
 * Returns NULL on error
 */
/************************************************************/
static char* buffer_stdin_to_tempfile(void) {
    char *tmppath = strdup("/tmp/presto_stdin_XXXXXX");
    int fd = mkstemp(tmppath);
    if (fd < 0) {
        perror("mkstemp");
        free(tmppath);
        return NULL;
    }
    
    /* Copy stdin to temp file */
    char buf[65536];
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(fd, buf + written, n - written);
            if (w < 0) {
                perror("write");
                close(fd);
                unlink(tmppath);
                free(tmppath);
                return NULL;
            }
            written += w;
        }
    }
    
    if (n < 0) {
        perror("read");
        close(fd);
        unlink(tmppath);
        free(tmppath);
        return NULL;
    }
    
    close(fd);
    return tmppath;
}

/************************************************************/
/* Build output filename for text macro results
 * Input: "/path/to/sample_10_errors.bhv2", macro_id=0
 * Output: "sample_10_errors.o0.txt" (caller must free)
 */
/************************************************************/
static char* make_output_filename(const char *input_path, int macro_id) {
    /* Get basename */
    char *path_copy = strdup(input_path);
    const char *base = basename(path_copy);
    
    /* Find extension and strip it */
    char *dot = strrchr(base, '.');
    size_t stem_len = dot ? (size_t)(dot - base) : strlen(base);
    
    /* Build output filename: stem.o<N>.txt */
    char *output = malloc(stem_len + 16);  /* stem + ".o" + number + ".txt" + null */
    if (output) {
        snprintf(output, stem_len + 16, "%.*s.o%d.txt", (int)stem_len, base, macro_id);
    }
    
    free(path_copy);
    return output;
}

/************************************************************/
/* Write text result to file
 * Returns 0 on success, -1 on error
 */
/************************************************************/
static int write_result_to_file(const char *dir, const char *filename, const char *text) {
    /* Build full path */
    size_t path_len = strlen(dir) + 1 + strlen(filename) + 1;
    char *path = malloc(path_len);
    if (!path) return -1;
    snprintf(path, path_len, "%s/%s", dir, filename);
    
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot write to %s: ", path);
        perror(NULL);
        free(path);
        return -1;
    }
    
    if (text) {
        fputs(text, fp);
        /* Ensure trailing newline */
        size_t len = strlen(text);
        if (len > 0 && text[len-1] != '\n') {
            fputc('\n', fp);
        }
    }
    
    fclose(fp);
    printf("Saved: %s\n", path);
    free(path);
    return 0;
}

/************************************************************/
/* Parse filter arguments from argv
 * Returns index of first non-option argument (file)
 */
/************************************************************/

typedef struct {
    skip_set_t *skips;
    int output_macro;
    int graph_macro;
    char *output_dir;
    bool to_stdout;
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
        
        /* '-' alone means stdin, treat as file not option */
        if (strcmp(arg, "-") == 0) {
            args->first_file_idx = i;
            return 0;
        }
        
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

/************************************************************/
/* Main
 */
/************************************************************/

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
    
    /* Check output directory exists if specified */
    if (args.output_dir && !args.to_stdout) {
        struct stat st;
        if (stat(args.output_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Error: Output directory does not exist: %s\n", args.output_dir);
            args_free(&args);
            return 1;
        }
    }
    
    /* Process each file */
    int status = 0;
    int n_files = argc - args.first_file_idx;
    char *stdin_tmpfile = NULL;  /* Track stdin temp file for cleanup */
    
    for (int i = args.first_file_idx; i < argc; i++) {
        const char *filepath = argv[i];
        const char *display_name = filepath;  /* Name to show in output */
        
        /* Handle stdin with explicit '-' */
        if (strcmp(filepath, "-") == 0) {
            if (n_files > 1) {
                fprintf(stderr, "Error: stdin (-) cannot be combined with other files\n");
                status = 1;
                break;
            }
            stdin_tmpfile = buffer_stdin_to_tempfile();
            if (!stdin_tmpfile) {
                fprintf(stderr, "Error: Failed to buffer stdin\n");
                status = 1;
                break;
            }
            filepath = stdin_tmpfile;
            display_name = "(stdin)";
        }
        
        /* Open BHV2 file with streaming API */
        ml_trial_file_t *file = open_input_file(filepath);
        if (!file) {
            fprintf(stderr, "Error: Failed to open %s: %s\n", display_name, bhv2_error_detail);
            status = 1;
            continue;
        }
        
        /* Set skip rules - macros will iterate trials themselves */
        set_skips(file, args.skips);
        
        /* Run the appropriate macro */
        if (args.graph_macro >= 0) {
            /* Graphical output */
            const char *output_path = args.output_dir ? args.output_dir : ".";
            int plot_status = run_plot_macro(args.graph_macro, file, filepath, output_path,
                                            args.plot_width, args.plot_height);
            if (plot_status != 0) {
                fprintf(stderr, "Error: Plot generation failed\n");
                status = 1;
            }
        } else {
            /* Text output */
            macro_result_t result;
            int macro_status = run_macro(args.output_macro, file, &result);
            
            if (macro_status != 0) {
                fprintf(stderr, "Error: Unknown macro -o%d\n\n", args.output_macro);
                fprintf(stderr, "Available text macros:\n");
                for (int j = 0; macros[j].name != NULL; j++) {
                    fprintf(stderr, "  -o%d  %s\n", macros[j].id, macros[j].description);
                }
                fprintf(stderr, "\nUse '%s -M' to list all macros.\n", argv[0]);
                status = 1;
            } else {
                /* Output result */
                if (args.to_stdout || args.output_dir == NULL) {
                    /* Print to stdout */
                    if (n_files > 1) {
                        /* Multiple files - use header separator */
                        printf("==> %s <==\n", display_name);
                        printf("%s\n", result.text ? result.text : "");
                    } else {
                        printf("%s\n", result.text ? result.text : "");
                    }
                } else {
                    /* Write to file */
                    char *outfile = make_output_filename(display_name, args.output_macro);
                    if (outfile) {
                        if (write_result_to_file(args.output_dir, outfile, result.text) != 0) {
                            status = 1;
                        }
                        free(outfile);
                    } else {
                        fprintf(stderr, "Error: Failed to create output filename\n");
                        status = 1;
                    }
                }
                
                macro_result_free(&result);
            }
        }
        
        close_input_file(file);
    }
    
    /* Clean up stdin temp file if used */
    if (stdin_tmpfile) {
        unlink(stdin_tmpfile);
        free(stdin_tmpfile);
    }
    
    args_free(&args);
    return status;
}
