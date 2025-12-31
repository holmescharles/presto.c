# Test Programs

This directory contains test programs for validating the grab-style API and debugging BHV2 file structures.

---

## test_iterator.c

Tests the grab-style iterator API with three scenarios:

1. **Iterate with SKIP_DATA** (headers only, fast)
   - Reads first 5 trial headers
   - Displays trial number, error code, and condition
   - Demonstrates fast header-only iteration

2. **Rewind and iterate with WITH_DATA** (full trial data)
   - Rewinds file to beginning
   - Reads first 5 trials with full data
   - Demonstrates data loading and rewind functionality

3. **Count all trials**
   - Rewinds and counts total trials in file
   - Demonstrates complete file iteration

### Compile

```bash
# Build bhv2.o first
make

# Compile test program
gcc -o test_iterator tests/test_iterator.c obj/bhv2.o -lm
```

### Run

```bash
./test_iterator path/to/file.bhv2
```

### Example Output

```
=== Test 1: Iterate with SKIP_DATA ===
Trial 1: Error=0, Condition=5
Trial 2: Error=0, Condition=3
Trial 3: Error=0, Condition=4
Trial 4: Error=0, Condition=6
Trial 5: Error=0, Condition=6

=== Test 2: Rewind and iterate with WITH_DATA ===
Trial 1: Error=0, has_data=yes
Trial 2: Error=0, has_data=yes
Trial 3: Error=0, has_data=yes
Trial 4: Error=0, has_data=yes
Trial 5: Error=0, has_data=yes

=== Test 3: Count all trials ===
Total trials: 10
```

### What It Tests

- ✅ `open_input_file()` / `close_input_file()`
- ✅ `read_next_trial(file, SKIP_DATA)` - Fast header-only reads
- ✅ `read_next_trial(file, WITH_DATA)` - Full data reads
- ✅ `rewind_input_file()` - File position reset
- ✅ `trial_number_header()` - Trial number accessor
- ✅ `trial_error_header()` - Error code accessor
- ✅ `condition_header()` - Condition accessor
- ✅ `trial_data_header()` - Data availability check

---

## debug_vars.c

Lists all variables in a BHV2 file. Useful for:
- Debugging file structure issues
- Verifying all variables are readable
- Understanding BHV2 file layout
- Confirming bug fixes (struct/cell skipping)

### Compile

```bash
# Build bhv2.o first
make

# Compile debug program
gcc -o debug_vars tests/debug_vars.c obj/bhv2.o -lm
```

### Run

```bash
./debug_vars path/to/file.bhv2
```

### Example Output

```
Variables in file:
  [1] IndexPosition
  [2] FileInfo
  [3] MLConfig
  [4] TrialRecord
  [5] Trial1
  [6] Trial2
  [7] Trial3
  [8] Trial4
  [9] Trial5
  [10] Trial6
  [11] Trial7
  [12] Trial8
  [13] Trial9
  [14] Trial10
  [15] FileIndex
```

### What It Tests

- ✅ Sequential variable reading
- ✅ Struct skipping (FileInfo, MLConfig)
- ✅ Cell array skipping (MLConfig.EyeTracerShape)
- ✅ Trial variable detection
- ✅ Complete file traversal

### Debugging

If this program stops before showing all Trial variables, it indicates a skip bug in the BHV2 parser. The bug fixes in this release allow it to read all 15 variables successfully.

---

## Running All Tests

Quick test script:

```bash
#!/bin/bash
# test_all.sh

# Build presto
make clean && make

# Compile test programs
gcc -o test_iterator tests/test_iterator.c obj/bhv2.o -lm
gcc -o debug_vars tests/debug_vars.c obj/bhv2.o -lm

# Run tests
echo "=== Testing iterator ==="
./test_iterator path/to/sample.bhv2

echo ""
echo "=== Testing variable reading ==="
./debug_vars path/to/sample.bhv2

echo ""
echo "=== Testing macros ==="
for i in 0 1 2 3 4 5; do
    echo "Macro $i:"
    ./bin/presto -o$i path/to/sample.bhv2
    echo ""
done
```

---

## Adding New Tests

To add a new test program:

1. Create `tests/my_test.c`
2. Include `#include "../src/bhv2.h"`
3. Link against `obj/bhv2.o` when compiling
4. Document in this README

Example template:

```c
#include <stdio.h>
#include "../src/bhv2.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.bhv2>\n", argv[0]);
        return 1;
    }
    
    bhv2_file_t *file = open_input_file(argv[1]);
    if (!file) {
        fprintf(stderr, "Failed to open file\n");
        return 1;
    }
    
    // Your test code here
    
    close_input_file(file);
    return 0;
}
```

Compile:
```bash
gcc -o my_test tests/my_test.c obj/bhv2.o -lm
```

---

## Test Data

Tests use sample BHV2 files with known characteristics:
- `sample_10_successes.bhv2` - 10 correct trials (error code 0)
- `sample_10_errors.bhv2` - 10 error trials with mixed error codes

These files exercise:
- Complex structs (FileInfo, MLConfig with 83 fields)
- Cell arrays (MLConfig.EyeTracerShape)
- Trial data structures
- Multiple conditions and blocks

---

## Validation

Expected results for sample files:

### sample_10_successes.bhv2
- **Variables**: 15 (IndexPosition through FileIndex)
- **Trials**: 10 (Trial1-Trial10)
- **Error codes**: All 0 (correct)
- **Conditions**: Mixed (3, 4, 5, 6)

### sample_10_errors.bhv2
- **Variables**: 15
- **Trials**: 10 (Trial1-Trial10)
- **Error codes**: Mixed (3, 7, 9)
- **Conditions**: Mixed

If tests produce different results, verify:
1. BHV2 parser bug fixes are applied
2. File is valid BHV2 format
3. Sample files haven't been modified

---

## See Also

- **[API.md](../API.md)** - Grab-style API reference
- **[WALKTHROUGH.md](../WALKTHROUGH.md)** - Implementation details
- **[CHANGELOG.md](../CHANGELOG.md)** - Bug fixes and development history
