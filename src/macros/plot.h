/*
 * plot.h - Gnuplot-based graphical output for presto
 */

#ifndef PRESTO_PLOT_H
#define PRESTO_PLOT_H

#include "../bhv2.h"

/*
 * Run graphical macro
 * 
 * macro_id: 1 = analog data plots, 2 = timeline histogram
 * file: BHV2 file handle (with skips already set via bhv2_set_skips)
 * input_path: Original input file path (for naming output)
 * output_dir: Directory for output PDF (or "-" for stdout, NULL for current dir)
 * width: Plot width in inches
 * height: Plot height in inches
 * 
 * Returns: 0 on success, -1 on error
 */
int run_plot_macro(int macro_id, bhv2_file_t *file,
                   const char *input_path, const char *output_dir,
                   double width, double height);

#endif /* PRESTO_PLOT_H */
