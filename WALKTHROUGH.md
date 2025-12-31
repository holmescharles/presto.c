# presto.c Code Walkthrough

This document explains how the pure C implementation of bhvq and presto works,
tracing through the code path for common operations.

## Project Structure

```
~/presto.c/
├── Makefile
├── bin/
│   ├── bhvq              # Query tool binary
│   └── presto            # Analysis tool binary
└── src/
    ├── bhv2.h            # BHV2 data structures and API
    ├── bhv2.c            # BHV2 file parser (~930 lines)
    ├── bhvq_main.c       # bhvq CLI entry point
    ├── bhvq_query.h      # Query parsing API
    ├── bhvq_query.c      # Pattern matching, glob expansion
    ├── bhvq_json.h       # JSON output API
    ├── bhvq_json.c       # JSON serialization
    ├── presto_main.c     # presto CLI entry point
    ├── presto_filter.h   # Trial filtering API
    ├── presto_filter.c   # Filter parsing and application
    ├── presto_macros.h   # Text macro API
    ├── presto_macros.c   # Macro implementations
    └── presto_plot.c     # Cairo plotting (stub)
```

---

## Part 1: The BHV2 Parser (bhv2.h / bhv2.c)

### Core Data Structures

The BHV2 format stores MATLAB variables in a binary format. We represent them with
a **tagged union** pattern:

```c
// bhv2.h:28-44
typedef enum {
    MATLAB_DOUBLE,    // 64-bit float
    MATLAB_SINGLE,    // 32-bit float  
    MATLAB_UINT8,     // ... numeric types
    MATLAB_STRUCT,    // MATLAB struct
    MATLAB_CELL,      // MATLAB cell array
    MATLAB_CHAR,      // String (char array)
    ...
} matlab_dtype_t;
```

Every value in the file is represented by `bhv2_value_t`:

```c
// bhv2.h:58-87
struct bhv2_value {
    matlab_dtype_t dtype;     // What type is this?
    uint64_t ndims;         // Number of dimensions
    uint64_t *dims;         // Array of dimension sizes [rows, cols, ...]
    uint64_t total;         // Total element count (product of dims)
    
    union {
        double *d;          // If dtype == MATLAB_DOUBLE
        float *f;           // If dtype == MATLAB_SINGLE
        uint8_t *u8;        // If dtype == MATLAB_UINT8
        char *str;          // If dtype == MATLAB_CHAR (null-terminated)
        
        struct {            // If dtype == MATLAB_STRUCT
            uint64_t n_fields;
            bhv2_struct_field_t *fields;
        } s;
        
        bhv2_value_t **cells;  // If dtype == MATLAB_CELL
    } data;
};
```

The key insight: **only one member of the union is valid**, determined by `dtype`.

### Opening a File: bhv2_open_stream()

When you call `bhv2_open_stream("/path/to/file.bhv2")`:

```c
// bhv2.c:~530
bhv2_file_t* bhv2_open_stream(const char *path) {
    // Open with POSIX file descriptor (not stdio)
    int fd = open(path, O_RDONLY);
    
    // Get file size
    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);  // Rewind
    
    // Allocate file structure (no variable array, just tracking position)
    bhv2_file_t *file = calloc(1, sizeof(bhv2_file_t));
    file->path = strdup(path);
    file->file_descriptor = fd;
    file->file_size = file_size;
    file->current_pos = 0;
    file->at_variable_data = false;
    
    return file;
}
```

The key difference: we don't read anything yet. The file stays on disk.

### Streaming Variables

Variables are read one at a time with `bhv2_read_next_variable_name()`:

```c
// bhv2.c:~575
int bhv2_read_next_variable_name(bhv2_file_t *file, char **name_out) {
    // Check if we're at EOF
    if (file->current_pos >= file->file_size) return -1;
    
    // Read variable name length
    uint64_t name_len;
    read(file->file_descriptor, &name_len, 8);
    
    // Read name
    char *name = malloc(name_len + 1);
    read(file->file_descriptor, name, name_len);
    name[name_len] = '\0';
    
    *name_out = name;
    file->at_variable_data = true;  // Next: read or skip data
    
    return 0;
}
```

After reading a name, you either read the data:

```c
// bhv2.c:~595
bhv2_value_t* bhv2_read_variable_data(bhv2_file_t *file) {
    // Call the POSIX reader (uses file descriptor, not FILE*)
    return bhv2_read_value_posix(file->file_descriptor);
}
```

Or skip it to save memory:

```c
// bhv2.c:~620
int bhv2_skip_variable_data(bhv2_file_t *file) {
    // Read dtype, ndims, dims to calculate size, then lseek() past it
    return bhv2_skip_value_posix(file->file_descriptor);
}
```

### Reading a Value: bhv2_read_value_posix()

Uses `read()` instead of `fread()`:

```c
// bhv2.c:~245
bhv2_value_t* bhv2_read_value_posix(int fd) {
    // 1. Read dtype string (e.g., "double", "struct", "cell")
    uint64_t dtype_len;
    read(fd, &dtype_len, 8);
    char *dtype_str = malloc(dtype_len + 1);
    read(fd, dtype_str, dtype_len);
    dtype_str[dtype_len] = '\0';
    
    matlab_dtype_t dtype = matlab_dtype_from_string(dtype_str);
    free(dtype_str);
    
    // 2. Read dimensions
    uint64_t ndims;
    read(fd, &ndims, 8);
    uint64_t *dims = malloc(ndims * sizeof(uint64_t));
    read(fd, dims, ndims * 8);
    
    // 3. Dispatch to type-specific reader
    bhv2_value_t *value = read_array_data_posix(fd, dtype, ndims, dims);
    free(dims);
    
    return value;
}
```

The type-specific readers handle numeric, char, struct, and cell arrays. Structs and cells are still recursive.

### Navigating Structs: bhv2_struct_get()

To get `Trial1.AnalogData.Button`:

```c
// bhv2.c:~600
bhv2_value_t* bhv2_struct_get(bhv2_value_t *val, const char *field, uint64_t index) {
    if (val->dtype != MATLAB_STRUCT) return NULL;
    
    // Search for field by name
    for (uint64_t i = 0; i < val->data.s.n_fields; i++) {
        if (strcmp(val->data.s.fields[i].name, field) == 0) {
            // Found it! Return the value at the given struct array index
            return val->data.s.fields[i * val->total + index].value;
        }
    }
    return NULL;  // Field not found
}
```

---

## Part 2: bhvq - Query Tool (CURRENTLY DISABLED)

bhvq has been temporarily disabled during the streaming refactor. When re-enabled, it will use the streaming API to avoid loading entire files into memory.

---

## Part 3: presto - Analysis Tool

### Example Command

```bash
./bin/presto -XE0 -o1 file.bhv2
# -XE0  = include only error code 0 (correct trials)
# -o1   = behavior summary macro
```

### Main Function Flow (presto_main.c)

```c
int main(int argc, char **argv) {
    presto_args_t args;
    
    // 1. Parse arguments
    parse_args(argc, argv, &args);
    // args.filters contains: {type=ERROR, include=true, values=[0]}
    // args.output_macro = 1
    
    // 2. Process each file
    for (int i = args.first_file_idx; i < argc; i++) {
        // 3. Open file (streaming - doesn't load into memory)
        bhv2_file_t *file = bhv2_open_stream(argv[i]);
        
        // 4. Stream through variables and build trial list
        trial_list_t *trials = trial_list_new();
        
        char *name;
        while (bhv2_read_next_variable_name(file, &name) == 0) {
            // Check if this is a trial variable: "Trial1", "Trial2", etc.
            if (strncmp(name, "Trial", 5) == 0 && isdigit(name[5])) {
                int trial_num = atoi(name + 5);
                
                // Read the trial data
                bhv2_value_t *trial_data = bhv2_read_variable_data(file);
                
                // Extract info for filtering
                trial_info_t info = {
                    .trial_num = trial_num,
                    .error_code = get_trial_error_from_value(trial_data),
                    .condition = get_trial_condition_from_value(trial_data)
                };
                
                // Check if trial passes filters
                if (filter_check_trial(args.filters, &info)) {
                    // Keep this trial
                    trial_list_add(trials, trial_num, trial_data);
                } else {
                    // Don't need it
                    bhv2_value_free(trial_data);
                }
            } else {
                // Not a trial variable - skip the data entirely
                bhv2_skip_variable_data(file);
            }
            free(name);
        }
        // At this point: only filtered trials are in memory
        
        // 5. Run the macro
        macro_result_t result;
        run_macro(args.output_macro, file, trials, &result);
        
        // 6. Output
        printf("%s\n", result.text);
        
        // 7. Cleanup
        trial_list_free(trials);
        bhv2_file_free(file);
    }
}
```

Key difference from the old version: we stream through the file, reading trial names but only keeping the data for trials that pass the filters. Variables we don't need (like ScreenInfo, ITI, etc.) are skipped entirely.

### Argument Parsing (presto_main.c)

```c
// presto_main.c:~120
static int parse_args(int argc, char **argv, presto_args_t *args) {
    args_init(args);  // Set defaults
    
    int i = 1;
    while (i < argc) {
        char *arg = argv[i];
        
        if (arg[0] != '-') {
            // First non-option is a file
            args->first_file_idx = i;
            return 0;
        }
        
        // Filter: -X (include) or -x (exclude)
        if (arg[1] == 'X' || arg[1] == 'x') {
            bool is_include = (arg[1] == 'X');
            const char *spec = arg + 2;  // "E0" from "-XE0"
            
            filter_parse_spec(args->filters, spec, is_include);
            i++;
            continue;
        }
        
        // Output macro: -o1
        if (arg[1] == 'o') {
            args->output_macro = atoi(arg + 2);  // 1
            i++;
            continue;
        }
        
        // ... other options
    }
}
```

### Filter Parsing (presto_filter.c)

```c
// presto_filter.c:~130
int filter_parse_spec(filter_set_t *fs, const char *spec, bool is_include) {
    // spec = "E0" (from -XE0)
    
    filter_rule_t rule;
    rule.include = is_include;  // true
    
    // First char determines type
    if (spec[0] == 'E') {
        rule.type = FILTER_ERROR;
        spec++;  // Now points to "0"
    } else if (spec[0] == 'c') {
        rule.type = FILTER_CONDITION;
        spec++;
    } else if (isdigit(spec[0])) {
        rule.type = FILTER_TRIAL;
        // Don't advance - "1:10" is the range
    }
    
    // Parse the range "0" or "1:10" or "1,3,5"
    rule.range = filter_parse_range(spec);
    // rule.range.values = [0]
    // rule.range.count = 1
    
    // Add to filter set
    filter_set_add(fs, rule);
    return 0;
}
```

### Range Parsing (presto_filter.c)

```c
// presto_filter.c:~50
filter_range_t filter_parse_range(const char *str) {
    filter_range_t range = {NULL, 0};
    
    // Handle different formats:
    // "5"     -> [5]
    // "1:10"  -> [1,2,3,4,5,6,7,8,9,10]
    // "1,3,5" -> [1,3,5]
    
    while (*p) {
        long start = strtol(p, &end, 10);  // Parse first number
        p = end;
        
        if (*p == ':') {
            // Range: parse end and expand
            p++;
            long stop = strtol(p, &end, 10);
            for (long v = start; v <= stop; v++) {
                range.values[range.count++] = v;
            }
        } else {
            // Single value
            range.values[range.count++] = start;
        }
        
        // Skip comma separator
        while (*p == ',') p++;
    }
    
    return range;
}
```

### Checking Trials (presto_filter.c)

```c
// presto_filter.c:~160
bool filter_check_trial(filter_set_t *fs, trial_info_t *info) {
    // For -XE0: include only if error_code is in [0]
    
    for (size_t i = 0; i < fs->count; i++) {
        filter_rule_t *rule = &fs->rules[i];
        
        // Get the value to test
        int test_value;
        switch (rule->type) {
            case FILTER_ERROR:     test_value = info->error_code; break;
            case FILTER_CONDITION: test_value = info->condition;  break;
            case FILTER_TRIAL:     test_value = info->trial_num;  break;
        }
        
        // Check if value is in the range
        bool in_range = false;
        for (size_t j = 0; j < rule->range.count; j++) {
            if (rule->range.values[j] == test_value) {
                in_range = true;
                break;
            }
        }
        
        // Apply include/exclude logic
        if (rule->include && !in_range) return false;  // Must match include
        if (!rule->include && in_range) return false;  // Must not match exclude
    }
    
    return true;  // Passed all filters
}
```

### Getting Trial Info (presto_filter.c)

```c
// presto_filter.c:~240
int get_trial_error_from_value(bhv2_value_t *trial_value) {
    if (!trial_value || trial_value->dtype != MATLAB_STRUCT) return -1;
    
    // Trial is a struct - get TrialError field
    bhv2_value_t *error_val = bhv2_struct_get(trial_value, "TrialError", 0);
    if (!error_val) return -1;
    
    return (int)bhv2_get_double(error_val, 0);
}

int get_trial_condition_from_value(bhv2_value_t *trial_value) {
    if (!trial_value || trial_value->dtype != MATLAB_STRUCT) return -1;
    
    bhv2_value_t *cond_val = bhv2_struct_get(trial_value, "Condition", 0);
    if (!cond_val) return -1;
    
    return (int)bhv2_get_double(cond_val, 0);
}
```

These functions operate directly on a trial's value instead of doing file lookups.

### Running a Macro (presto_macros.c)

```c
// presto_macros.c:~70
int run_macro(int macro_id, bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    macro_result_init(result);
    
    switch (macro_id) {
        case 0: return macro_count(file, trials, result);
        case 1: return macro_behavior(file, trials, result);
        case 2: return macro_errors(file, trials, result);
        // ...
    }
}

// presto_macros.c:~95
int macro_behavior(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    // Count error codes across filtered trials
    int error_counts[256] = {0};
    
    for (size_t i = 0; i < trials->count; i++) {
        // Trial data is already in memory (no file lookup needed)
        int error = get_trial_error_from_value(trials->trial_data[i]);
        if (error >= 0 && error < 256) {
            error_counts[error]++;
        }
    }
    
    // Format output
    macro_result_appendf(result, "Trials: %zu\n", trials->count);
    
    int correct = error_counts[0];
    double pct = 100.0 * correct / trials->count;
    macro_result_appendf(result, "Correct: %d (%.1f%%)\n", correct, pct);
    
    macro_result_append(result, "Errors:\n");
    for (int e = 0; e <= max_error; e++) {
        if (error_counts[e] > 0) {
            macro_result_appendf(result, "  E%d: %d (%.1f%%)\n", 
                e, error_counts[e], 100.0 * error_counts[e] / trials->count);
        }
    }
    
    return 0;
}

---

## Part 4: Memory Management

### The Pattern: Stream on read, keep only what you need

Streaming version doesn't load entire files. Memory usage:
- File structure: just the path and file descriptor
- Trial list: only filtered trials (not all trials)
- Each trial: the struct value with all its fields

```c
// Typical usage pattern:
bhv2_file_t *file = bhv2_open_stream("file.bhv2");  // Opens fd, nothing loaded
trial_list_t *trials = trial_list_new();            // Empty list

char *name;
while (bhv2_read_next_variable_name(file, &name) == 0) {
    if (is_trial_we_want(name)) {
        bhv2_value_t *data = bhv2_read_variable_data(file);
        trial_list_add(trials, trial_num, data);  // Keep it
    } else {
        bhv2_skip_variable_data(file);  // Don't even read it
    }
    free(name);
}

// ... use trials ...

trial_list_free(trials);   // Frees trial data
bhv2_file_free(file);      // Closes file descriptor

// bhv2.c:~226
void bhv2_file_free(bhv2_file_t *file) {
    if (!file) return;
    free(file->path);
    if (file->file_descriptor >= 0) {
        close(file->file_descriptor);
    }
    free(file);
}

// presto_filter.c:~290
void trial_list_free(trial_list_t *list) {
    if (!list) return;
    
    // Free all trial data
    for (size_t i = 0; i < list->count; i++) {
        bhv2_value_free(list->trial_data[i]);
    }
    
    free(list->trial_nums);
    free(list->trial_data);
    free(list);
}
```

bhv2_value_free() is still recursive (same as before).

---

## Part 5: Adding a New Macro

To add a new text macro (e.g., `-o6` for reaction times):

### 1. Declare in presto_macros.h

```c
int macro_reaction_times(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result);
```

### 2. Implement in presto_macros.c

```c
int macro_reaction_times(bhv2_file_t *file, trial_list_t *trials, macro_result_t *result) {
    macro_result_append(result, "Trial\tRT_ms\n");
    
    for (size_t i = 0; i < trials->count; i++) {
        int trial_num = trials->trial_nums[i];
        
        // Trial data is already in memory
        bhv2_value_t *rt = bhv2_struct_get(trials->trial_data[i], "ReactionTime", 0);
        
        if (rt) {
            double rt_ms = bhv2_get_double(rt, 0);
            macro_result_appendf(result, "%d\t%.1f\n", trial_num, rt_ms);
        }
    }
    
    return 0;
}
```

### 3. Register in run_macro()

```c
int run_macro(int macro_id, ...) {
    switch (macro_id) {
        // ... existing cases ...
        case 6: return macro_reaction_times(file, trials, result);
    }
}
```

### 4. Add to help text in presto_main.c

```c
static macro_info_t macros[] = {
    // ... existing macros ...
    {6, "rt", "Reaction times per trial", false},
    {-1, NULL, NULL, false}
};
```

---

## Summary: Data Flow

```
bhvq: CURRENTLY DISABLED (will be updated to use streaming)


presto -XE0 -o1 file.bhv2
│
├─► main()
│   ├─► parse_args()
│   │   └─► filter_parse_spec() → Adds {ERROR, include, [0]} rule
│   ├─► bhv2_open_stream()      → Opens fd, doesn't load anything
│   ├─► Stream loop:
│   │   ├─► bhv2_read_next_variable_name()
│   │   ├─► if "Trial<N>":
│   │   │   ├─► bhv2_read_variable_data()
│   │   │   │   └─► bhv2_read_value_posix() (POSIX read(), not fread())
│   │   │   ├─► get_trial_error_from_value()
│   │   │   ├─► get_trial_condition_from_value()
│   │   │   ├─► filter_check_trial()
│   │   │   └─► trial_list_add() or bhv2_value_free()
│   │   └─► else: bhv2_skip_variable_data()
│   ├─► run_macro(1, ...)
│   │   └─► macro_behavior()    → Uses trials->trial_data[i] directly
│   └─► printf("%s", result.text)
```
