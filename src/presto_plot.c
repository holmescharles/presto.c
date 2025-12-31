/*
 * presto_plot.c - Gnuplot-based graphical output
 *
 * Generates PDF plots using gnuplot for:
 *   -g1: Analog data plots (eye, mouse, buttons)
 *   -g2: Timeline histogram (trials over time)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include "bhv2.h"
#include "presto_filter.h"
#include "presto_plot.h"

/* Data structures for plotting */

typedef struct {
    double *data;
    size_t length;
} signal_data_t;

typedef struct {
    int trial_num;
    int error_code;
    int condition;
    int block;
    signal_data_t eye_x, eye_y;
    signal_data_t mouse_x, mouse_y;
    signal_data_t *buttons;  /* Array of button signals */
    int n_buttons;
    double sample_interval;
    double abs_start_time;  /* For timeline */
    int has_eye;
    int has_mouse;
} trial_analog_data_t;

/* Helper functions */

static void signal_free(signal_data_t *sig) {
    if (sig && sig->data) {
        free(sig->data);
        sig->data = NULL;
        sig->length = 0;
    }
}

static void trial_analog_free(trial_analog_data_t *tad) {
    if (!tad) return;
    signal_free(&tad->eye_x);
    signal_free(&tad->eye_y);
    signal_free(&tad->mouse_x);
    signal_free(&tad->mouse_y);
    if (tad->buttons) {
        for (int i = 0; i < tad->n_buttons; i++) {
            signal_free(&tad->buttons[i]);
        }
        free(tad->buttons);
        tad->buttons = NULL;
    }
}

static int check_gnuplot_installed(void) {
    int ret = system("which gnuplot > /dev/null 2>&1");
    if (ret != 0) {
        fprintf(stderr, "Error: gnuplot not found. Install with:\n");
        fprintf(stderr, "  Ubuntu/Debian: sudo apt install gnuplot\n");
        fprintf(stderr, "  RHEL/CentOS:   sudo yum install gnuplot\n");
        fprintf(stderr, "  macOS:         brew install gnuplot\n");
        return -1;
    }
    return 0;
}

static char* create_temp_dir(void) {
    char *template = strdup("/tmp/presto_plot_XXXXXX");
    char *tmpdir = mkdtemp(template);
    if (!tmpdir) {
        perror("mkdtemp");
        free(template);
        return NULL;
    }
    return tmpdir;
}

static void cleanup_temp_dir(const char *tmpdir) {
    if (!tmpdir) return;
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
}

/* Extract analog data from a trial */
static int extract_trial_analog_data(bhv2_value_t *trial_value, int trial_num, trial_analog_data_t *out) {
    memset(out, 0, sizeof(trial_analog_data_t));
    out->trial_num = trial_num;
    
    /* Get trial metadata */
    bhv2_value_t *trial_error = bhv2_struct_get(trial_value, "TrialError", 0);
    if (trial_error) {
        out->error_code = (int)bhv2_get_double(trial_error, 0);
    }
    
    bhv2_value_t *condition = bhv2_struct_get(trial_value, "Condition", 0);
    if (condition) {
        out->condition = (int)bhv2_get_double(condition, 0);
    }
    
    bhv2_value_t *block = bhv2_struct_get(trial_value, "Block", 0);
    if (block) {
        out->block = (int)bhv2_get_double(block, 0);
    }
    
    bhv2_value_t *abs_time = bhv2_struct_get(trial_value, "AbsoluteTrialStartTime", 0);
    if (abs_time) {
        out->abs_start_time = bhv2_get_double(abs_time, 0);
    }
    
    /* Get AnalogData struct */
    bhv2_value_t *analog_data = bhv2_struct_get(trial_value, "AnalogData", 0);
    if (!analog_data) {
        return 0;  /* No analog data */
    }
    
    /* Get sample interval */
    bhv2_value_t *sample_interval = bhv2_struct_get(analog_data, "SampleInterval", 0);
    if (sample_interval) {
        out->sample_interval = bhv2_get_double(sample_interval, 0);
    } else {
        out->sample_interval = 0.001;  /* Default 1ms */
    }
    
    /* Extract Eye data */
    bhv2_value_t *eye = bhv2_struct_get(analog_data, "Eye", 0);
    if (eye && eye->dims[0] > 0 && eye->dims[1] >= 2) {
        size_t n_samples = eye->dims[0];
        out->eye_x.data = malloc(n_samples * sizeof(double));
        out->eye_y.data = malloc(n_samples * sizeof(double));
        out->eye_x.length = n_samples;
        out->eye_y.length = n_samples;
        
        for (size_t i = 0; i < n_samples; i++) {
            out->eye_x.data[i] = bhv2_get_double(eye, i * eye->dims[1] + 0);
            out->eye_y.data[i] = bhv2_get_double(eye, i * eye->dims[1] + 1);
        }
        out->has_eye = 1;
    }
    
    /* Extract Mouse data */
    bhv2_value_t *mouse = bhv2_struct_get(analog_data, "Mouse", 0);
    if (mouse && mouse->dims[0] > 0 && mouse->dims[1] >= 2) {
        size_t n_samples = mouse->dims[0];
        out->mouse_x.data = malloc(n_samples * sizeof(double));
        out->mouse_y.data = malloc(n_samples * sizeof(double));
        out->mouse_x.length = n_samples;
        out->mouse_y.length = n_samples;
        
        for (size_t i = 0; i < n_samples; i++) {
            out->mouse_x.data[i] = bhv2_get_double(mouse, i * mouse->dims[1] + 0);
            out->mouse_y.data[i] = bhv2_get_double(mouse, i * mouse->dims[1] + 1);
        }
        out->has_mouse = 1;
    }
    
    /* Extract Button data */
    bhv2_value_t *button_struct = bhv2_struct_get(analog_data, "Button", 0);
    if (button_struct && button_struct->dtype == MATLAB_STRUCT) {
        /* Count buttons and extract data */
        out->n_buttons = 0;
        out->buttons = NULL;
        
        /* Allocate space for up to 10 buttons */
        signal_data_t temp_buttons[10];
        int temp_count = 0;
        
        for (int btn = 1; btn <= 10 && temp_count < 10; btn++) {
            char btn_name[16];
            snprintf(btn_name, sizeof(btn_name), "Btn%d", btn);
            
            bhv2_value_t *btn_data = bhv2_struct_get(button_struct, btn_name, 0);
            if (btn_data && btn_data->dims[0] > 0) {
                size_t n_samples = btn_data->dims[0];
                temp_buttons[temp_count].data = malloc(n_samples * sizeof(double));
                temp_buttons[temp_count].length = n_samples;
                
                for (size_t i = 0; i < n_samples; i++) {
                    temp_buttons[temp_count].data[i] = bhv2_get_double(btn_data, i);
                }
                temp_count++;
            }
        }
        
        if (temp_count > 0) {
            out->buttons = malloc(temp_count * sizeof(signal_data_t));
            memcpy(out->buttons, temp_buttons, temp_count * sizeof(signal_data_t));
            out->n_buttons = temp_count;
        }
    }
    
    return 1;
}

/* Write data file for a single trial */
static int write_trial_data_file(trial_analog_data_t *tad, const char *filepath) {
    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    
    /* Header */
    fprintf(fp, "# Trial %d: Error %d, Condition %d\n", tad->trial_num, tad->error_code, tad->condition);
    fprintf(fp, "# Time(ms)");
    if (tad->has_eye) fprintf(fp, "\tEye_X\tEye_Y");
    if (tad->has_mouse) fprintf(fp, "\tMouse_X\tMouse_Y");
    for (int i = 0; i < tad->n_buttons; i++) {
        fprintf(fp, "\tBtn%d", i + 1);
    }
    fprintf(fp, "\n");
    
    /* Determine maximum length */
    size_t max_len = 0;
    if (tad->has_eye && tad->eye_x.length > max_len) max_len = tad->eye_x.length;
    if (tad->has_mouse && tad->mouse_x.length > max_len) max_len = tad->mouse_x.length;
    for (int i = 0; i < tad->n_buttons; i++) {
        if (tad->buttons[i].length > max_len) max_len = tad->buttons[i].length;
    }
    
    /* Write data rows */
    for (size_t i = 0; i < max_len; i++) {
        double time_ms = i * tad->sample_interval * 1000.0;
        fprintf(fp, "%.3f", time_ms);
        
        if (tad->has_eye) {
            if (i < tad->eye_x.length) {
                fprintf(fp, "\t%.3f\t%.3f", tad->eye_x.data[i], tad->eye_y.data[i]);
            } else {
                fprintf(fp, "\tNaN\tNaN");
            }
        }
        
        if (tad->has_mouse) {
            if (i < tad->mouse_x.length) {
                fprintf(fp, "\t%.3f\t%.3f", tad->mouse_x.data[i], tad->mouse_y.data[i]);
            } else {
                fprintf(fp, "\tNaN\tNaN");
            }
        }
        
        for (int j = 0; j < tad->n_buttons; j++) {
            if (i < tad->buttons[j].length) {
                fprintf(fp, "\t%.0f", tad->buttons[j].data[i]);
            } else {
                fprintf(fp, "\tNaN");
            }
        }
        
        fprintf(fp, "\n");
    }
    
    fclose(fp);
    return 0;
}

/* Generate gnuplot script for analog data (-g1) */
static int generate_analog_plot_script(trial_analog_data_t *trials, int n_trials, 
                                       const char *tmpdir, const char *output_pdf) {
    char script_path[1024];
    snprintf(script_path, sizeof(script_path), "%s/plot.gp", tmpdir);
    
    FILE *fp = fopen(script_path, "w");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    
    /* Gnuplot header */
    fprintf(fp, "set terminal pdfcairo enhanced color font 'Sans,10' size 11,8.5\n");
    fprintf(fp, "set output '%s'\n\n", output_pdf);
    
    /* Process each trial */
    for (int t = 0; t < n_trials; t++) {
        trial_analog_data_t *tad = &trials[t];
        
        /* Count number of subplots */
        int n_plots = 0;
        if (tad->has_eye) n_plots++;
        if (tad->has_mouse) n_plots++;
        if (tad->n_buttons > 0) n_plots++;
        
        if (n_plots == 0) continue;
        
        /* Title */
        fprintf(fp, "set multiplot layout %d,1 title 'Trial %d | Block %d | Condition %d | Error %d'\n\n",
                n_plots, tad->trial_num, tad->block, tad->condition, tad->error_code);
        
        char data_file[1024];
        snprintf(data_file, sizeof(data_file), "%s/trial_%03d.dat", tmpdir, t);
        
        int plot_idx = 0;
        int col = 2;  /* Start at column 2 (column 1 is time) */
        
        /* Eye position plot */
        if (tad->has_eye) {
            fprintf(fp, "set title 'Eye Position'\n");
            fprintf(fp, "set xlabel 'Time (ms)'\n");
            fprintf(fp, "set ylabel 'Position (deg)'\n");
            fprintf(fp, "set grid\n");
            fprintf(fp, "plot '%s' using 1:%d with lines lw 2 lc rgb '#3498db' title 'Eye X', \\\n", data_file, col);
            fprintf(fp, "     '' using 1:%d with lines lw 2 lc rgb '#85c1e9' title 'Eye Y'\n\n", col + 1);
            col += 2;
            plot_idx++;
        }
        
        /* Mouse position plot */
        if (tad->has_mouse) {
            fprintf(fp, "set title 'Mouse Position'\n");
            fprintf(fp, "set xlabel 'Time (ms)'\n");
            fprintf(fp, "set ylabel 'Position (px)'\n");
            fprintf(fp, "set grid\n");
            fprintf(fp, "plot '%s' using 1:%d with lines lw 2 lc rgb '#e74c3c' title 'Mouse X', \\\n", data_file, col);
            fprintf(fp, "     '' using 1:%d with lines lw 2 lc rgb '#f1948a' title 'Mouse Y'\n\n", col + 1);
            col += 2;
            plot_idx++;
        }
        
        /* Button states plot */
        if (tad->n_buttons > 0) {
            fprintf(fp, "set title 'Button States'\n");
            fprintf(fp, "set xlabel 'Time (ms)'\n");
            fprintf(fp, "set ylabel 'State'\n");
            fprintf(fp, "set grid\n");
            fprintf(fp, "set yrange [-0.5:%d.5]\n", tad->n_buttons);
            fprintf(fp, "plot ");
            for (int b = 0; b < tad->n_buttons; b++) {
                if (b > 0) fprintf(fp, ",\\\n     ");
                fprintf(fp, "'%s' using 1:($%d*0.8+%d) with steps lw 2 title 'Button %d'", 
                        data_file, col + b, b, b + 1);
            }
            fprintf(fp, "\n\n");
        }
        
        fprintf(fp, "unset multiplot\n\n");
    }
    
    fclose(fp);
    return 0;
}

/* Generate gnuplot script for timeline (-g2) */
static int generate_timeline_plot_script(trial_analog_data_t *trials, int n_trials,
                                         const char *tmpdir, const char *output_pdf) {
    char script_path[1024];
    char data_path[1024];
    snprintf(script_path, sizeof(script_path), "%s/plot.gp", tmpdir);
    snprintf(data_path, sizeof(data_path), "%s/timeline.dat", tmpdir);
    
    /* Write timeline data file */
    FILE *data_fp = fopen(data_path, "w");
    if (!data_fp) {
        perror("fopen");
        return -1;
    }
    
    fprintf(data_fp, "# Time(min)\tError\n");
    for (int i = 0; i < n_trials; i++) {
        double time_min = trials[i].abs_start_time / 60000.0;  /* Convert ms to minutes */
        fprintf(data_fp, "%.3f\t%d\n", time_min, trials[i].error_code);
    }
    fclose(data_fp);
    
    /* Write gnuplot script */
    FILE *fp = fopen(script_path, "w");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    
    fprintf(fp, "set terminal pdfcairo enhanced color font 'Sans,12' size 11,8.5\n");
    fprintf(fp, "set output '%s'\n\n", output_pdf);
    
    fprintf(fp, "set title 'Experiment Timeline' font 'Sans,14'\n");
    fprintf(fp, "set xlabel 'Time (minutes)'\n");
    fprintf(fp, "set ylabel 'Number of Trials'\n");
    fprintf(fp, "set grid\n");
    fprintf(fp, "set style fill solid 0.8 border -1\n");
    fprintf(fp, "set boxwidth 0.9 relative\n\n");
    
    /* Count error types */
    int error_counts[256] = {0};
    int max_error = -1;
    for (int i = 0; i < n_trials; i++) {
        int e = trials[i].error_code;
        if (e >= 0 && e < 256) {
            error_counts[e]++;
            if (e > max_error) max_error = e;
        }
    }
    
    /* Create histogram with error-specific coloring */
    fprintf(fp, "# Color definitions\n");
    fprintf(fp, "color_correct = '#2ecc71'\n");
    fprintf(fp, "color_error3 = '#e74c3c'\n");
    fprintf(fp, "color_error7 = '#3498db'\n");
    fprintf(fp, "color_other = '#95a5a6'\n\n");
    
    fprintf(fp, "set style data histogram\n");
    fprintf(fp, "set style histogram clustered gap 1\n");
    fprintf(fp, "set style fill solid border -1\n");
    fprintf(fp, "set xtics rotate by -45\n\n");
    
    /* Calculate data range for binning */
    fprintf(fp, "stats '%s' using 1 nooutput\n", data_path);
    fprintf(fp, "bins = 20\n");
    fprintf(fp, "binwidth = (STATS_max - STATS_min) / bins\n");
    fprintf(fp, "bin(x) = binwidth * floor((x - STATS_min)/binwidth) + STATS_min\n\n");
    
    fprintf(fp, "plot '%s' using (bin($1)):(1.0) smooth freq with boxes \\\n", data_path);
    fprintf(fp, "     lc rgb '#3498db' title 'All Trials (n=%d)' fillstyle solid 0.5\n", n_trials);
    
    /* Add info text */
    double duration_min = 0;
    if (n_trials > 0) {
        duration_min = (trials[n_trials-1].abs_start_time - trials[0].abs_start_time) / 60000.0;
    }
    
    fprintf(fp, "\nset label 'Total: %d trials over %.1f minutes' at graph 0.02, graph 0.95 front\n",
            n_trials, duration_min);
    
    fclose(fp);
    return 0;
}

/* Main plotting function */
int run_plot_macro(int macro_id, bhv2_file_t *file, trial_list_t *trials,
                   const char *input_path, const char *output_dir) {
    (void)file;  /* Unused for now */
    
    /* Check gnuplot */
    if (check_gnuplot_installed() != 0) {
        return -1;
    }
    
    if (trials->count == 0) {
        fprintf(stderr, "Warning: No trials to plot\n");
        return -1;
    }
    
    /* Create temp directory */
    char *tmpdir = create_temp_dir();
    if (!tmpdir) {
        return -1;
    }
    
    /* Extract analog data from all trials */
    trial_analog_data_t *trial_data = calloc(trials->count, sizeof(trial_analog_data_t));
    if (!trial_data) {
        cleanup_temp_dir(tmpdir);
        free(tmpdir);
        return -1;
    }
    
    for (size_t i = 0; i < trials->count; i++) {
        extract_trial_analog_data(trials->trial_data[i], trials->trial_nums[i], &trial_data[i]);
    }
    
    /* Determine output filename */
    char output_pdf[1024];
    const char *base_name = basename((char*)input_path);
    char *dot = strrchr(base_name, '.');
    char stem[256];
    if (dot) {
        size_t len = dot - base_name;
        if (len >= sizeof(stem)) len = sizeof(stem) - 1;
        strncpy(stem, base_name, len);
        stem[len] = '\0';
    } else {
        strncpy(stem, base_name, sizeof(stem) - 1);
        stem[sizeof(stem) - 1] = '\0';
    }
    
    const char *out_dir = output_dir ? output_dir : ".";
    if (strcmp(out_dir, "-") == 0) {
        /* TODO: Handle stdout output */
        fprintf(stderr, "Error: Stdout output (-O -) not yet implemented for plots\n");
        cleanup_temp_dir(tmpdir);
        free(tmpdir);
        for (size_t i = 0; i < trials->count; i++) {
            trial_analog_free(&trial_data[i]);
        }
        free(trial_data);
        return -1;
    }
    
    int ret = 0;
    
    if (macro_id == 1) {
        /* -g1: Analog data plots */
        snprintf(output_pdf, sizeof(output_pdf), "%s/AnalogData_%s.pdf", out_dir, stem);
        
        /* Write data files */
        for (size_t i = 0; i < trials->count; i++) {
            char data_file[1024];
            snprintf(data_file, sizeof(data_file), "%s/trial_%03zu.dat", tmpdir, i);
            if (write_trial_data_file(&trial_data[i], data_file) != 0) {
                ret = -1;
                goto cleanup;
            }
        }
        
        /* Generate gnuplot script */
        if (generate_analog_plot_script(trial_data, trials->count, tmpdir, output_pdf) != 0) {
            ret = -1;
            goto cleanup;
        }
        
    } else if (macro_id == 2) {
        /* -g2: Timeline histogram */
        snprintf(output_pdf, sizeof(output_pdf), "%s/Timeline_%s.pdf", out_dir, stem);
        
        if (generate_timeline_plot_script(trial_data, trials->count, tmpdir, output_pdf) != 0) {
            ret = -1;
            goto cleanup;
        }
        
    } else {
        fprintf(stderr, "Error: Unknown plot macro %d\n", macro_id);
        ret = -1;
        goto cleanup;
    }
    
    /* Execute gnuplot */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "gnuplot %s/plot.gp 2>&1", tmpdir);
    int gnuplot_ret = system(cmd);
    if (gnuplot_ret != 0) {
        fprintf(stderr, "Error: gnuplot execution failed (exit code %d)\n", gnuplot_ret);
        fprintf(stderr, "Script location: %s/plot.gp\n", tmpdir);
        ret = -1;
        goto cleanup;
    }
    
    printf("Saved: %s\n", output_pdf);
    
cleanup:
    /* Clean up */
    for (size_t i = 0; i < trials->count; i++) {
        trial_analog_free(&trial_data[i]);
    }
    free(trial_data);
    
    if (ret == 0) {
        cleanup_temp_dir(tmpdir);
    } else {
        fprintf(stderr, "Temp files preserved for debugging: %s\n", tmpdir);
    }
    
    free(tmpdir);
    return ret;
}
