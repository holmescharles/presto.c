# 🪄 PRESTO - Pure C Behavioral Data Analyzer

> **Process MonkeyLogic BHV2 files into readable reports**

**Repository**: https://github.com/holmescharles/presto.c

---

## What is PRESTO?

**PRESTO** is a fast, pure C implementation for analyzing behavioral data from MonkeyLogic experiments. It takes BHV2 files and converts them into readable reports and analyses.

This is a complete rewrite of the original Python implementation, featuring:
- **Zero dependencies** - Pure C11, no external libraries required
- **Grab-style API** - Iterator pattern matching grab neurophysiology tool conventions
- **Fast BHV2 parsing** - Optimized file reading with bug fixes
- **Simple deployment** - Just compile and run

---

## 🚀 Quick Start

### Build

```bash
make
```

### Run

```bash
# Count trials
./bin/presto -o0 data.bhv2

# Behavior summary
./bin/presto -o1 data.bhv2

# Error code breakdown  
./bin/presto -o2 data.bhv2

# List all macros
./bin/presto -M
```

---

## 📋 Features

### Available Macros

- **Macro 0** (`-o0`): Count trials (filtered)
- **Macro 1** (`-o1`): Behavior summary (error codes, conditions)
- **Macro 2** (`-o2`): Error code breakdown
- **Macro 3** (`-o3`): Scene structure analysis
- **Macro 4** (`-o4`): Analog data info
- **Macro 5** (`-o5`): Error counts per condition

### Trial Filtering

PRESTO supports powerful trial filtering syntax:

```bash
# Include only correct trials (error code 0)
./bin/presto -XE0 -o1 data.bhv2

# Exclude error codes 1-3
./bin/presto -xE1:3 -o1 data.bhv2

# Include only conditions 2-5
./bin/presto -Xc2:5 -o1 data.bhv2

# Include only trials 1-10
./bin/presto -X1:10 -o1 data.bhv2

# Combine filters (correct trials in conditions 2-5)
./bin/presto -XE0 -Xc2:5 -o1 data.bhv2
```

**Filter syntax:**
- `-XE<spec>` - Include only error codes
- `-xE<spec>` - Exclude error codes
- `-Xc<spec>` - Include only conditions
- `-xc<spec>` - Exclude conditions
- `-X<spec>` - Include only trials
- `-x<spec>` - Exclude trials

**Spec format:**
- `N` - Single value (e.g., `-XE0`)
- `N:M` - Range (e.g., `-xE1:5`)
- `N,M,O` - Union (e.g., `-XE0,5,9`)

### Output Options

```bash
# Output to stdout (default)
./bin/presto -o1 data.bhv2

# Output to stdout for piping
./bin/presto -O - -o1 *.bhv2 | grep Subject

# Save to specific directory
./bin/presto -O results/ -o1 data.bhv2

# Force overwrite existing files
./bin/presto -f -o1 data.bhv2
```

---

## 📖 Usage Examples

### Basic Analysis

```bash
# Analyze a single session
./bin/presto -o1 session_001.bhv2

# Analyze multiple sessions
./bin/presto -o1 session_*.bhv2

# Get only correct trials
./bin/presto -XE0 -o1 session_001.bhv2
```

### Error Analysis

```bash
# Show error breakdown
./bin/presto -o2 data.bhv2

# Error counts per condition
./bin/presto -o5 data.bhv2
```

### Scene Analysis

```bash
# Show scene structure
./bin/presto -o3 data.bhv2
```

### Multiple Files with Output Directory

```bash
# Process all BHV2 files in directory
./bin/presto -o1 -O results/ *.bhv2

# With filtering
./bin/presto -XE0 -o1 -O results/ *.bhv2
```

---

## 🔧 Building from Source

### Requirements

- C11 compiler (gcc or clang)
- POSIX system (Linux, macOS, BSD)
- make

### Compile

```bash
# Build presto
make

# Clean build artifacts
make clean

# Rebuild from scratch
make clean && make
```

### Output

Compiled binary will be in `bin/presto`

---

## 🎯 Grab-Style API

PRESTO includes a grab-style iterator API for developers who want to build custom analysis tools.

See **[API.md](API.md)** for complete documentation.

**Quick example:**

```c
#include "bhv2.h"

int main() {
    bhv2_file_t *file = open_input_file("data.bhv2");
    
    while (read_next_trial(file, WITH_DATA) > 0) {
        int trial_num = trial_number_header(file);
        int error = trial_error_header(file);
        int condition = condition_header(file);
        
        printf("Trial %d: Error=%d, Condition=%d\n", 
               trial_num, error, condition);
    }
    
    close_input_file(file);
    return 0;
}
```

**Key functions:**
- `open_input_file()` / `close_input_file()`
- `read_next_trial(file, WITH_DATA)` / `read_next_trial(file, SKIP_DATA)`
- `trial_number_header()`, `trial_error_header()`, `condition_header()`
- `rewind_input_file()` - Reset to beginning

See **[WALKTHROUGH.md](WALKTHROUGH.md)** for technical deep dive.

---

## 📚 Documentation

- **[API.md](API.md)** - Grab-style API reference with code examples
- **[WALKTHROUGH.md](WALKTHROUGH.md)** - Technical walkthrough of implementation
- **[CHANGELOG.md](CHANGELOG.md)** - Development history and bug fixes
- **[tests/README.md](tests/README.md)** - Test programs documentation

---

## 🧪 Testing

Test programs are available in the `tests/` directory:

```bash
# Compile test programs
gcc -o test_iterator tests/test_iterator.c obj/bhv2.o -lm
gcc -o debug_vars tests/debug_vars.c obj/bhv2.o -lm

# Run tests
./test_iterator path/to/file.bhv2
./debug_vars path/to/file.bhv2
```

See [tests/README.md](tests/README.md) for details.

---

## ⚠️ Known Limitations

### Not Yet Implemented

- **bhvq query mode** - The jq-like query interface is not yet implemented in C
  - Use the Python version for query mode: https://github.com/MooshLab/presto
- **Graphical macros** (`-g1`, `-g2`) - Plotting macros are stubs

### Workarounds

- For bhvq query functionality, use the Python implementation
- For plotting, export data and use external tools (matplotlib, R, etc.)

---

## 🐍 Python Version

A full-featured Python implementation with bhvq query mode and graphical macros is available:

**Repository**: https://github.com/MooshLab/presto

**When to use Python version:**
- Need bhvq jq-like query syntax
- Need graphical plotting macros
- Prefer pip installation

**When to use C version:**
- Want zero dependencies
- Need maximum speed
- Prefer grab-style API
- Building custom tools

Both versions read the same BHV2 format and produce compatible output.

---

## 🔍 Help & Info

```bash
# Show help
./bin/presto -h

# List available macros
./bin/presto -M

# Show version
./bin/presto -V
```

---

## 📝 Command-Line Reference

```
Usage: presto [options] <file.bhv2> [files...]

Trial filtering:
  -XE<spec>   Include only error codes (e.g., -XE0, -XE1:3)
  -xE<spec>   Exclude error codes
  -Xc<spec>   Include only conditions
  -xc<spec>   Exclude conditions
  -X<spec>    Include only trials (e.g., -X1:10)
  -x<spec>    Exclude trials

Output:
  -o<N>       Text output macro (default: 0)
  -g<N>       Graphical output macro (not yet implemented)
  -O <dir>    Output directory ('-' for stdout)
  -f          Force overwrite existing files

Info:
  -M          List available macros
  -h          Show help
  -V          Show version

Spec format: N (single), N:M (range), N,M,O (union)
```

---

## 💡 Examples

```bash
# Count correct trials
./bin/presto -XE0 -o0 data.bhv2

# Behavior summary for conditions 1-5
./bin/presto -Xc1:5 -o1 data.bhv2

# Error breakdown excluding aborts (error 3)
./bin/presto -xE3 -o2 data.bhv2

# Scene structure for first 20 trials
./bin/presto -X1:20 -o3 data.bhv2

# Process multiple files with filtering
./bin/presto -XE0 -o1 -O results/ session_*.bhv2

# Pipe to grep for specific subject
./bin/presto -O - -o1 *.bhv2 | grep Subject05
```

---

## 🛠️ Development

### Project Structure

```
presto.c/
├── src/              # C source code
│   ├── bhv2.c/h     # BHV2 parser + grab-style API
│   ├── presto_*.c/h # Presto implementation
│   └── bhvq_*.c/h   # bhvq stubs (incomplete)
├── tests/           # Test programs
├── bin/             # Compiled binaries
└── obj/             # Object files
```

### Building Custom Tools

Link against `obj/bhv2.o`:

```bash
gcc -o mytool mytool.c obj/bhv2.o -lm
```

See [API.md](API.md) for grab-style API documentation.

---

## 🐛 Bug Fixes

This implementation includes critical bug fixes for BHV2 parsing:

1. **Struct field name skipping** - Fixed file position desync when skipping MATLAB struct arrays
2. **Cell element name skipping** - Fixed parsing of cell arrays (e.g., MLConfig.EyeTracerShape)

See [CHANGELOG.md](CHANGELOG.md) for technical details.

---

## 📧 Contact

**Need help?** Email holmes@ucsd.edu if you run into issues or have questions.

---

**PRESTO** - Fast behavioral data analysis for MonkeyLogic experiments.
