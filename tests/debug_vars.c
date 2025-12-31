/*
 * debug_vars.c - Debug: List all variables in a BHV2 file
 */

#include <stdio.h>
#include <stdlib.h>
#include "src/bhv2.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.bhv2>\n", argv[0]);
        return 1;
    }
    
    bhv2_file_t *file = bhv2_open_stream(argv[1]);
    if (!file) {
        fprintf(stderr, "Failed to open %s\n", argv[1]);
        return 1;
    }
    
    printf("Variables in file:\n");
    char *name;
    int count = 0;
    while (bhv2_read_next_variable_name(file, &name) == 0) {
        printf("  [%d] %s\n", ++count, name);
        free(name);
        bhv2_skip_variable_data(file);
        
        if (count >= 20) {
            printf("  ... (showing first 20)\n");
            break;
        }
    }
    
    bhv2_file_free(file);
    return 0;
}
