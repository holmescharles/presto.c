/*
 * presto_plot.c - Cairo plotting for presto (stub)
 *
 * TODO: Implement graphical output macros
 */

#include <stdio.h>
#include "bhv2.h"
#include "presto_filter.h"

/* Placeholder - Cairo plotting not yet implemented */

int plot_analog(bhv2_file_t *file, trial_list_t *trials, const char *output_path) {
    (void)file;
    (void)trials;
    (void)output_path;
    fprintf(stderr, "Cairo plotting not yet implemented\n");
    return -1;
}

int plot_timeline(bhv2_file_t *file, trial_list_t *trials, const char *output_path) {
    (void)file;
    (void)trials;
    (void)output_path;
    fprintf(stderr, "Timeline plotting not yet implemented\n");
    return -1;
}
