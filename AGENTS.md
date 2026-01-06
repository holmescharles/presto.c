# Presto.c Project Context & Session History

## Project Overview

**presto.c** is a pure C implementation of a behavioral data analyzer for MonkeyLogic BHV2 files. It uses a grab-style API (inspired by neurophysiology tools) with iterator patterns and skip/keep semantics.

**Repository**: https://github.com/holmescharles/presto.c (main branch)
**Location**: `/home/chholmes/presto/`
**Build Status**: ✅ Working - compiles cleanly with `make`

## Design Philosophy

**"Draw inspiration from grab, but embrace clearly better designs"**

This project is inspired by grab tool conventions but is not bound to strict adherence. When grab-style patterns work well (skip/keep semantics, iterator APIs, clear naming), we use them. When alternative designs are demonstrably clearer, simpler, or better suited to the non-coder audience, we deviate confidently.

Grab-inspired patterns we follow:
- Skip terminology (SKIP_DATA/WITH_DATA patterns)
- Clean, prefix-free file naming
- Iterator-based API design
- Clear, action-oriented function names

## Intended Audience & Handoff Context

**This project will be handed off to non-coders** with limited C and R knowledge. Design decisions prioritize:

1. **Simplicity over sophistication**: Prefer readable, straightforward code
2. **Maintainability**: Clear patterns that non-coders can extend
3. **Scriptability**: External tools (gnuplot) may be easier to modify than compiled C code
4. **Documentation**: Verbose comments and comprehensive guides (README.md, API.md, WALKTHROUGH.md)

**Key implications:**
- **Plotting**: Using gnuplot scripts (text-based, easy to modify) instead of C graphics libraries
- **Code structure**: Modular macros that can be added/modified independently
- **Error messages**: Clear, actionable feedback for users
- **Build system**: Simple Makefile without complex dependencies

This handoff constraint justifies choices that might seem suboptimal for experienced C programmers but are better for long-term maintainability by the intended audience.

## Recent Major Refactoring (COMPLETED)

### 1. Filter → Skip System Refactor (Commit: 88882b1)

**Goal**: Align with grab tool conventions using "skip" terminology instead of "filter"

**Key Changes:**
- **Files renamed**: `src/filter.c/.h` → `src/skip.c/.h`
- **Type system overhaul**: All `filter_*` → `skip_*` types
  - `filter_set_t` → `skip_set_t`
  - `filter_rule_t` → `skip_rule_t`
  - `filter_range_t` → `skip_range_t`
  - 15+ functions/types renamed
- **Enum consistency**: 
  - `FILTER_TRIAL` → `SKIP_BY_TRIAL`
  - `FILTER_ERROR` → `SKIP_BY_ERROR`
  - `FILTER_CONDITION` → `SKIP_BY_CONDITION`
- **⚠️ CRITICAL SEMANTIC INVERSION**: 
  ```c
  // OLD: filter_check_trial() returns true = KEEP trial
  // NEW: skip_trial() returns true = SKIP trial
  ```
  All call sites updated with inverted boolean logic throughout `main.c`
- **Files modified**: `skip.c`, `skip.h`, `main.c`, `macros.h`, `plot.c`, `plot.h`, `Makefile`

**Testing Results** (All passed ✅):
```bash
./bin/presto -o0 sample.bhv2                    # → 10 trials
./bin/presto -XE0 -o0 sample.bhv2               # → 10 trials (include error 0)
./bin/presto -XE0 -o0 errors.bhv2               # → 0 trials (no error 0 trials)
./bin/presto -xE0 -o0 sample.bhv2               # → 0 trials (exclude error 0)
```

### 2. Main Entry Point Rename (Commit: 0765b84)

**Goal**: Complete naming consistency across all files

**Key Changes:**
- **Files renamed**: `src/presto_main.c` → `src/main.c`
- **Updated references**: Makefile, README.md, CHANGELOG.md, WALKTHROUGH.md (7 total files)
- **Result**: All implementation files now have clean, consistent naming:
  ```
  ├── bhv2.c/h     # BHV2 parser + grab-style API
  ├── main.c       # Main entry point
  ├── skip.c/h     # Trial skipping (formerly filter)
  ├── macros.c/h   # Text output macros
  └── plot.c/h     # Graphical output (gnuplot)
  ```

### 3. Segfault Bug Fix & Function Rename (Commit: fd58212)

**Goal**: Fix critical segfault and improve API consistency

**Key Changes:**
- **Bug fix**: Added NULL check in `bhv2_struct_get()` (src/bhv2.c:~966-988)
  - **Problem**: `strcmp()` called on NULL field names during selective struct reads
  - **Solution**: Skip fields with NULL names before comparison
  - **Impact**: Prevents segfault when reading real BHV2 files
- **Function renames**: Removed redundant `_header` suffix from trial accessors
  - `trial_error_header()` → `trial_error()`
  - `trial_data_header()` → `trial_data()`
  - `trial_codes_header()` → `trial_codes()`
  - `trial_analog_header()` → `trial_analog()`
- **Files modified**: `src/bhv2.c`, `src/bhv2.h`, all files in `src/macros/`

**Testing Results** (✅ All passed):
```bash
./bin/presto -o0 /data/limbs/data/bhv2/251013_Subject04_free_reach.bhv2
# → 767 trials (previously: segfault)

./bin/presto -g1 -X1 -O ~ /data/limbs/data/bhv2/251013_Subject04_free_reach.bhv2
# → Generated PLOT.pdf successfully (previously: segfault)
```

**Root Cause Analysis:**
When reading BHV2 structs selectively, some fields are not populated (left NULL). The old code assumed all field names existed before calling `strcmp()`, causing NULL pointer dereference when fields were skipped.

### Git Status

- ✅ Both commits successfully pushed to `origin/main`
- ✅ Repository state: Clean working directory
- ✅ All changes synced with GitHub

## Current Project Structure

```
presto/
├── src/
│   ├── bhv2.c/h     # BHV2 parser + grab-style API
│   ├── main.c       # Main entry point (CLI argument parsing)
│   ├── skip.c/h     # Trial skipping/filtering system
│   ├── macros.c/h   # Text output macros (-o0 through -o5)
│   └── plot.c/h     # Graphical output macros (-g1, -g2)
├── bin/             # Compiled executables
├── obj/             # Object files
├── tests/           # Test programs
├── Makefile         # Build system
├── README.md        # User documentation
├── API.md           # API documentation
├── CHANGELOG.md     # Change history
├── WALKTHROUGH.md   # Code architecture guide
└── AGENTS.md        # This file (project context for AI agents)
```

## Available Features

### Command-Line Interface

**Trial Filtering** (skip system):
- `-XE0` - Include only trials with error code 0
- `-xE1:3` - Exclude trials with error codes 1-3
- `-Xc1:5` - Include only trials with conditions 1-5
- `-X1:10` - Include only trials 1-10
- Filters can be combined: `-XE0 -Xc1:5`

**Text Macros** (`-o` flag):
- `-o0` - Count trials
- `-o1` - Behavior summary
- `-o2` - Error code breakdown
- `-o3` - Scene information
- `-o4` - Analog data
- `-o5` - Error counts by type

**Graphical Macros** (`-g` flag, requires gnuplot):
- `-g1` - Analog signal plots
- `-g2` - Timeline visualization

**Output Options**:
- `-O dir` - Output directory
- `-f` - Force overwrite existing files
- `-s WxH` - Set plot size (e.g., `-s 800x600`)

### Build & Test Commands

```bash
# Build
make clean && make              # Clean build
make                            # Incremental build

# Basic usage
./bin/presto -o0 data.bhv2      # Count trials
./bin/presto -o1 data.bhv2      # Show behavior summary

# Filtering examples
./bin/presto -XE0 -o1 data.bhv2         # Only error code 0
./bin/presto -xE1:3 -o1 data.bhv2       # Exclude errors 1-3
./bin/presto -Xc1:5 -o2 data.bhv2       # Only conditions 1-5
./bin/presto -XE0 -Xc1:3 -o1 data.bhv2  # Combined filters

# Output to directory
./bin/presto -g1 -O output/ data.bhv2   # Save plots to output/
./bin/presto -o1 -O results/ -f data.bhv2  # Force overwrite
```

## Code Architecture Overview

### Core API (bhv2.c/h)

The BHV2 module provides a grab-style iterator API:

```c
// Basic usage pattern
bhv2_t *bhv2 = bhv2_open("file.bhv2");
bhv2_trial_t *trial;

while ((trial = bhv2_next_trial(bhv2))) {
    // Process trial
    int error_code = trial->TrialError;
    int condition = trial->Condition;
    // ... access codes, times, analog data, etc.
}

bhv2_close(bhv2);
```

**Key types:**
- `bhv2_t` - Main BHV2 file handle
- `bhv2_trial_t` - Trial data structure
- `bhv2_code_t` - Behavioral event codes
- `bhv2_analog_t` - Analog signal data

### Skip System (skip.c/h)

The skip module handles trial filtering with grab-style semantics:

```c
// Create skip set
skip_set_t *skip = skip_set_create();

// Add skip rules
skip_add_error(skip, 1, 3);      // Skip errors 1-3
skip_add_condition(skip, 5, 10);  // Skip conditions 5-10
skip_add_trial(skip, 1, 5);      // Skip trials 1-5

// Check if trial should be skipped
if (skip_trial(skip, trial_num, error_code, condition)) {
    continue;  // Skip this trial
}

skip_set_destroy(skip);
```

**Key types:**
- `skip_set_t` - Collection of skip rules
- `skip_rule_t` - Individual skip rule
- `skip_type_t` - Rule type enum (SKIP_BY_TRIAL, SKIP_BY_ERROR, SKIP_BY_CONDITION)

**⚠️ Important**: `skip_trial()` returns `true` when a trial should be SKIPPED (semantic inversion from old filter system).

### Macro System (macros.c/h, plot.c/h)

Macros are pre-built analysis routines:

```c
// Text macros
macro_count(bhv2, skip);           // -o0: Count trials
macro_behavior(bhv2, skip);        // -o1: Behavior summary
macro_errors(bhv2, skip);          // -o2: Error breakdown

// Plot macros
plot_analog(bhv2, skip, opts);     // -g1: Analog plots
plot_timeline(bhv2, skip, opts);   // -g2: Timeline
```

### Main Entry Point (main.c)

Handles CLI argument parsing and orchestrates:
1. Parse command-line arguments
2. Build skip set from filter flags
3. Open BHV2 file
4. Execute requested macros
5. Clean up and exit

## Testing Strategy

### Current Test Coverage

**Filter/Skip Tests** (all passing ✅):
- Include error codes: `-XE0`
- Exclude error codes: `-xE1:3`
- Include conditions: `-Xc1:5`
- Exclude conditions: `-xc2:4`
- Include trial ranges: `-X1:10`
- Exclude trial ranges: `-x5:15`
- Combined filters: `-XE0 -Xc1:5`

**Macro Tests**:
- Text output macros: `-o0` through `-o5`
- Graphical macros: `-g1`, `-g2` (requires gnuplot)

### Test Data

Test BHV2 files should include:
- Variety of error codes (0, 1, 2, 3, etc.)
- Multiple conditions
- Behavioral event codes
- Analog signal data (for plot tests)

## Known Issues & Limitations

None currently identified. All features working as expected.

## Future Directions (Potential)

Possible areas for enhancement:
1. **Additional macros** - More pre-built analysis routines
2. **Performance optimization** - Large file handling improvements
3. **Extended filtering** - More complex filter combinations
4. **Output formats** - CSV, JSON export options
5. **Real-time visualization** - Live plotting during analysis
6. **Error handling** - More detailed error messages

## Development Workflow

### Making Changes

1. **Edit source files** in `src/`
2. **Rebuild**: `make clean && make`
3. **Test**: Run with sample data
4. **Verify**: Check output matches expectations
5. **Commit**: Use descriptive commit messages following existing style
6. **Push**: `git push origin main`

### Commit Message Style

Based on git history:
- Clear, concise summaries (50 chars or less)
- Detailed explanations in body when needed
- Focus on "why" rather than "what"
- Examples from this session:
  - "Rename filter system to skip for grab-style consistency"
  - "Rename presto_main.c to main.c for consistency"

### Code Style Guidelines

- **Naming**: Clear, action-oriented function names
- **Consistency**: Follow grab-style conventions
- **Comments**: Explain "why", not "what"
- **Error handling**: Always check return values
- **Memory management**: Clean up all allocations

## Important Constraints

1. **Non-coder audience**: Design for maintainability by users with limited C knowledge (see "Intended Audience & Handoff Context")
2. **Grab-inspired patterns**: Draw from grab conventions when they improve clarity, but don't enforce strict adherence
3. **Semantic clarity**: Function names should clearly indicate their purpose
4. **API stability**: Maintain backward compatibility when possible
5. **Performance**: Keep iterator overhead minimal
6. **Portability**: Pure C99, minimal dependencies

## Session Continuity

This file (`AGENTS.md`) serves as the project context for AI coding agents (like OpenCode). When starting a new session:

1. Agent should read this file first to understand project state
2. All major changes should be documented here
3. Keep this file updated as the project evolves
4. This file is NOT for user documentation (use README.md for that)

## Quick Reference Commands

```bash
# Navigation
cd ~/presto                     # Go to project directory

# Building
make clean && make              # Full rebuild
make                            # Incremental build

# Testing basic functionality
./bin/presto -o0 tests/*.bhv2   # Count trials in test files

# Git operations
git status                      # Check status
git add .                       # Stage changes
git commit -m "message"         # Commit with message
git push origin main            # Push to GitHub

# Finding code
grep -r "function_name" src/    # Search for function
ls -la src/                     # List source files
```

## Contact & Resources

- **Repository**: https://github.com/holmescharles/presto.c
- **Documentation**: See README.md, API.md, WALKTHROUGH.md in repository
- **Issues**: Use GitHub issues for bug reports and feature requests

---

*Last updated: 2026-01-05*
*Session: Filter→Skip refactor + main.c rename + segfault fix + function renames + design philosophy clarification*
