# Grab-Style API Reference

This document describes the **grab-style iterator API** for reading BHV2 files in presto-c.

The API is inspired by the grab neurophysiology tool (circa 2018) and provides a clean, iterator-based pattern for trial processing.

---

## Overview

The grab-style API provides:
- **Iterator pattern** for reading trials sequentially
- **Header accessor functions** to get trial metadata
- **Data skipping option** for fast header-only reads
- **Rewind capability** for multiple passes over data

### Key Concepts

1. **File handle** - `bhv2_file_t*` represents an open BHV2 file
2. **Trial iterator** - `read_next_trial()` advances to next trial
3. **Header accessors** - `*_header()` functions extract metadata from current trial
4. **Data modes** - `WITH_DATA` vs `SKIP_DATA` for performance optimization

---

## API Functions

### File Operations

#### `open_input_file()`
```c
bhv2_file_t* open_input_file(const char *path);
```
Open a BHV2 file for reading.

**Parameters:**
- `path` - Path to BHV2 file

**Returns:**
- File handle on success, `NULL` on error

**Example:**
```c
bhv2_file_t *file = open_input_file("data.bhv2");
if (!file) {
    fprintf(stderr, "Failed to open file\n");
    return 1;
}
```

---

#### `close_input_file()`
```c
void close_input_file(bhv2_file_t *file);
```
Close file and free all resources.

**Parameters:**
- `file` - File handle from `open_input_file()`

**Example:**
```c
close_input_file(file);
```

---

#### `rewind_input_file()`
```c
void rewind_input_file(bhv2_file_t *file);
```
Reset file position to beginning, allowing multiple passes over trials.

**Parameters:**
- `file` - File handle to rewind

**Example:**
```c
// First pass: count trials
int count = 0;
while (read_next_trial(file, SKIP_DATA) > 0) {
    count++;
}

// Second pass: process trials
rewind_input_file(file);
while (read_next_trial(file, WITH_DATA) > 0) {
    // Process trial data
}
```

---

### Trial Iteration

#### `read_next_trial()`
```c
int read_next_trial(bhv2_file_t *file, int skip_data_flag);
```
Read the next trial from the file. This is the **core iterator function**.

**Parameters:**
- `file` - File handle
- `skip_data_flag` - `SKIP_DATA` (fast, header-only) or `WITH_DATA` (read full trial)

**Returns:**
- Trial number (>0) on success
- `0` on end of file
- Negative on error

**Constants:**
- `WITH_DATA` (0) - Read full trial data into memory
- `SKIP_DATA` (1) - Skip trial data, only parse header info

**Example:**
```c
// Read headers only (fast)
while (read_next_trial(file, SKIP_DATA) > 0) {
    printf("Trial %d: Error=%d\n", 
           trial_number_header(file),
           trial_error_header(file));
}

// Read full data
while (read_next_trial(file, WITH_DATA) > 0) {
    bhv2_value_t *data = trial_data_header(file);
    // Process trial data structure
}
```

**Performance Note:**
- `SKIP_DATA` is significantly faster when you only need trial numbers, error codes, and conditions
- Use `WITH_DATA` only when you need to access trial data structures (analog signals, behavioral events, etc.)

---

#### `skip_over_data()`
```c
void skip_over_data(bhv2_file_t *file);
```
Skip current trial's data if positioned at data after reading header.

**Parameters:**
- `file` - File handle

**Note:** This is an advanced function. Most users should use `read_next_trial(file, SKIP_DATA)` instead.

---

### Header Accessor Functions

After calling `read_next_trial()`, these functions return metadata from the **current trial**.

#### `trial_number_header()`
```c
int trial_number_header(bhv2_file_t *file);
```
Get trial number (1-based).

**Returns:** Trial number, or 0 if no trial loaded

---

#### `trial_error_header()`
```c
int trial_error_header(bhv2_file_t *file);
```
Get trial error code from `TrialError` field.

**Returns:** Error code (typically 0=correct, >0=error type)

**Common error codes:**
- `0` - Correct trial
- `3` - Fixation break
- `7` - Early response
- `9` - No response

---

#### `condition_header()`
```c
int condition_header(bhv2_file_t *file);
```
Get condition number from `Condition` field.

**Returns:** Condition number, or 0 if not set

---

#### `block_number_header()`
```c
int block_number_header(bhv2_file_t *file);
```
Get block number from `BlockNumber` field.

**Returns:** Block number, or 0 if not set

---

#### `trial_data_header()`
```c
bhv2_value_t* trial_data_header(bhv2_file_t *file);
```
Get pointer to current trial's full data structure.

**Returns:** 
- Pointer to trial data (MATLAB struct)
- `NULL` if no trial loaded or data not read

**Important:**
- Only valid if `read_next_trial(file, WITH_DATA)` was called
- Returns `NULL` if `SKIP_DATA` was used
- Data is owned by the file handle - do not free it
- Data is invalidated by next call to `read_next_trial()` or `close_input_file()`

**Example:**
```c
while (read_next_trial(file, WITH_DATA) > 0) {
    bhv2_value_t *trial = trial_data_header(file);
    
    // Access trial fields using bhv2_struct_get()
    bhv2_value_t *trial_error = bhv2_struct_get(trial, "TrialError", 0);
    bhv2_value_t *condition = bhv2_struct_get(trial, "Condition", 0);
    
    // Access analog data, behavioral codes, etc.
}
```

---

## Usage Patterns

### Pattern 1: Fast Header-Only Processing

When you only need trial numbers, error codes, and conditions:

```c
#include "bhv2.h"

int main() {
    bhv2_file_t *file = open_input_file("data.bhv2");
    
    int total = 0, correct = 0;
    
    while (read_next_trial(file, SKIP_DATA) > 0) {
        total++;
        if (trial_error_header(file) == 0) {
            correct++;
        }
    }
    
    printf("Correct: %d/%d (%.1f%%)\n", 
           correct, total, 100.0 * correct / total);
    
    close_input_file(file);
    return 0;
}
```

---

### Pattern 2: Full Data Processing

When you need to access trial data structures:

```c
#include "bhv2.h"

int main() {
    bhv2_file_t *file = open_input_file("data.bhv2");
    
    while (read_next_trial(file, WITH_DATA) > 0) {
        bhv2_value_t *trial = trial_data_header(file);
        
        // Extract trial metadata
        int trial_num = trial_number_header(file);
        int error = trial_error_header(file);
        int condition = condition_header(file);
        
        printf("Trial %d (E%d, C%d):\n", trial_num, error, condition);
        
        // Access specific trial fields
        bhv2_value_t *reaction_time = bhv2_struct_get(trial, "ReactionTime", 0);
        if (reaction_time) {
            double rt = bhv2_get_double(reaction_time, 0);
            printf("  ReactionTime: %.1f ms\n", rt);
        }
    }
    
    close_input_file(file);
    return 0;
}
```

---

### Pattern 3: Multiple Passes with Rewind

When you need to iterate over trials multiple times:

```c
#include "bhv2.h"

int main() {
    bhv2_file_t *file = open_input_file("data.bhv2");
    
    // Pass 1: Count correct trials
    int correct_count = 0;
    while (read_next_trial(file, SKIP_DATA) > 0) {
        if (trial_error_header(file) == 0) {
            correct_count++;
        }
    }
    
    printf("Found %d correct trials\n", correct_count);
    
    // Pass 2: Process correct trials only
    rewind_input_file(file);
    while (read_next_trial(file, WITH_DATA) > 0) {
        if (trial_error_header(file) == 0) {
            // Process correct trial data
            bhv2_value_t *trial = trial_data_header(file);
            // ...
        }
    }
    
    close_input_file(file);
    return 0;
}
```

---

### Pattern 4: Filtered Processing

When you want to process specific trials based on criteria:

```c
#include "bhv2.h"

int main() {
    bhv2_file_t *file = open_input_file("data.bhv2");
    
    // Process only correct trials (E0) in conditions 1-5
    while (read_next_trial(file, WITH_DATA) > 0) {
        int error = trial_error_header(file);
        int condition = condition_header(file);
        
        if (error == 0 && condition >= 1 && condition <= 5) {
            int trial_num = trial_number_header(file);
            bhv2_value_t *trial = trial_data_header(file);
            
            printf("Processing trial %d (C%d)\n", trial_num, condition);
            // Process trial...
        }
    }
    
    close_input_file(file);
    return 0;
}
```

---

## Accessing Trial Data

After calling `read_next_trial(file, WITH_DATA)`, use these functions to navigate trial structures:

### `bhv2_struct_get()`
```c
bhv2_value_t* bhv2_struct_get(bhv2_value_t *value, const char *field, uint64_t index);
```
Get a field from a MATLAB struct.

**Example:**
```c
bhv2_value_t *trial = trial_data_header(file);
bhv2_value_t *error = bhv2_struct_get(trial, "TrialError", 0);
bhv2_value_t *condition = bhv2_struct_get(trial, "Condition", 0);
bhv2_value_t *codes = bhv2_struct_get(trial, "BehavioralCodes", 0);
```

---

### `bhv2_get_double()`
```c
double bhv2_get_double(bhv2_value_t *value, uint64_t index);
```
Extract numeric value as double.

**Example:**
```c
bhv2_value_t *reaction_time = bhv2_struct_get(trial, "ReactionTime", 0);
double rt = bhv2_get_double(reaction_time, 0);
```

---

### `bhv2_cell_get()`
```c
bhv2_value_t* bhv2_cell_get(bhv2_value_t *value, uint64_t index);
```
Get element from cell array.

**Example:**
```c
bhv2_value_t *analog_data = bhv2_struct_get(trial, "AnalogData", 0);
bhv2_value_t *eye_x = bhv2_cell_get(analog_data, 0);  // First channel
bhv2_value_t *eye_y = bhv2_cell_get(analog_data, 1);  // Second channel
```

---

## Comparison with Grab Tool

This API follows grab neurophysiology tool conventions:

| Grab Function | Presto-C Equivalent | Notes |
|--------------|---------------------|-------|
| `Read_Next_Trial(SKIP_DATA)` | `read_next_trial(file, SKIP_DATA)` | Header-only read |
| `Read_Next_Trial(WITH_DATA)` | `read_next_trial(file, WITH_DATA)` | Full data read |
| `Trial_Number_Header()` | `trial_number_header(file)` | Get trial number |
| `Trial_Error_Header()` | `trial_error_header(file)` | Get error code |
| `Condition_Header()` | `condition_header(file)` | Get condition |
| `Open_InputFile()` | `open_input_file(path)` | Open file |
| `Close_InputFile()` | `close_input_file(file)` | Close file |
| `Rewind_InputFile()` | `rewind_input_file(file)` | Reset to start |

**Key differences:**
- Grab uses global state, presto-c passes file handle explicitly
- Grab uses uppercase (legacy C style), presto-c uses lowercase
- Presto-c returns trial number from `read_next_trial()`, grab uses separate function

---

## Performance Tips

1. **Use `SKIP_DATA` when possible** - It's significantly faster when you don't need trial data structures
2. **Rewind instead of reopening** - `rewind_input_file()` is faster than closing and reopening
3. **Process filtered subsets early** - Check error codes and conditions before reading full data
4. **Avoid unnecessary data extraction** - Only call `bhv2_struct_get()` for fields you actually need

---

## Error Handling

All functions return appropriate values on error:

- `open_input_file()` returns `NULL`
- `read_next_trial()` returns negative value (check return code)
- Header accessor functions return 0 on error
- `trial_data_header()` returns `NULL` if data not available

**Example:**
```c
bhv2_file_t *file = open_input_file("data.bhv2");
if (!file) {
    fprintf(stderr, "Error: %s\n", bhv2_get_error());
    return 1;
}

int status;
while ((status = read_next_trial(file, WITH_DATA)) > 0) {
    // Process trial
}

if (status < 0) {
    fprintf(stderr, "Error reading trial: %s\n", bhv2_get_error());
}

close_input_file(file);
```

---

## Complete Example

See `tests/test_iterator.c` for a complete working example that demonstrates:
- Opening files
- Iterating with `SKIP_DATA`
- Rewinding and iterating with `WITH_DATA`
- Using header accessor functions
- Counting trials

**Compile:**
```bash
gcc -o test_iterator tests/test_iterator.c obj/bhv2.o -lm
```

**Run:**
```bash
./test_iterator path/to/file.bhv2
```

---

## See Also

- **[README.md](README.md)** - General usage and command-line interface
- **[WALKTHROUGH.md](WALKTHROUGH.md)** - Technical implementation details
- **[tests/README.md](tests/README.md)** - Test programs
- **[CHANGELOG.md](CHANGELOG.md)** - Bug fixes and development history
