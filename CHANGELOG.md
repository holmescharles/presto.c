# Changelog

All notable changes to presto.c are documented here.

---

## [Unreleased]

### Changed (main branch)

- **Refactored filter system to grab-style "skip" naming** - Aligns with grab tool conventions
  - Renamed `filter.c/.h` → `skip.c/.h`
  - All types renamed: `filter_*` → `skip_*`
  - Enum values renamed for consistency: `FILTER_TRIAL` → `SKIP_BY_TRIAL`, etc.
  - **Semantic inversion**: `skip_trial()` returns `true` = skip (was: `filter_check_trial()` returns `true` = keep)
  - Matches grab API's `SKIP_DATA` / `WITH_DATA` convention
  - Updated all call sites and inverted boolean logic
  - Makes presto maximally grab-like while maintaining clarity

### Removed (main branch)

- **Removed bhvq stub files** - Cleaned up unimplemented query mode code
  - Removed `src/bhvq_main.c`, `src/bhvq_query.c/.h`, `src/bhvq_json.c/.h`
  - bhvq query mode is not planned for C implementation
  - Use Python version for jq-like query functionality
  
### Changed (main branch)

- **Simplified file naming** - Removed `presto_` prefix from core implementation files
  - `presto_filter.c/.h` → `filter.c/.h` (now `skip.c/.h` as of latest refactor)
  - `presto_macros.c/.h` → `macros.c/.h`
  - `presto_plot.c/.h` → `plot.c/.h`
  - Kept `presto_main.c` as entry point
  - Updated all includes and Makefile
  - Cleaner, more focused project structure

### Added (main branch)

- **Plot size control** - Add `-s WxH` flag for custom plot dimensions
  - Specify width and height in inches (e.g., `-s 8x6`, `-s 14x10`)
  - Default size: 11x8.5 inches (landscape letter)
  - Applies to both `-g1` (analog plots) and `-g2` (timeline)
  - Error checking for invalid formats and non-positive dimensions

- **Graphical macros using gnuplot** - Plotting functionality now implemented
  - **Macro 1** (`-g1`) - Analog data plots
    - Multi-page PDF with one trial per page
    - Three subplots per trial: Eye position (X/Y), Mouse position (X/Y), Button states
    - Auto-detects available analog data (Eye, Mouse, Buttons)
    - Title shows trial number, block, condition, and error code
  - **Macro 2** (`-g2`) - Timeline histogram
    - Single-page PDF showing trial distribution over time
    - X-axis: Time in minutes since experiment start
    - Y-axis: Number of trials per bin (20 bins, adaptive)
    - Info label shows total trials and experiment duration
  - **Dependencies**: Requires gnuplot (6.0+ tested, older versions likely work)
  - **Architecture**: C code generates tab-delimited data files and gnuplot scripts, executes gnuplot, cleans up temp files
  - **Output formats**: PDF files (format: `AnalogData_<filename>.pdf`, `Timeline_<filename>.pdf`)
  - **Limitations**: Stdout output (`-O -`) not yet implemented for plots

### Added (c-port branch)

- **Pure C implementation** of presto behavioral data analyzer
  - Zero dependencies (pure C11, POSIX)
  - Fast BHV2 file parsing with streaming API
  - 6 analysis macros matching Python version functionality

- **Grab-style iterator API** matching grab neurophysiology tool conventions
  - `open_input_file()` / `close_input_file()` - File operations
  - `read_next_trial(file, WITH_DATA/SKIP_DATA)` - Iterator with data skip option
  - `rewind_input_file()` - Reset file position for multiple passes
  - Header accessors: `trial_number_header()`, `trial_error_header()`, `condition_header()`, `block_number_header()`, `trial_data_header()`
  - See API.md for complete documentation

- **Test suite** for validation and examples
  - `tests/test_iterator.c` - Demonstrates grab-style API usage
  - `tests/debug_vars.c` - BHV2 file structure debugging tool

- **Comprehensive documentation**
  - README.md - User guide with examples
  - API.md - Grab-style API reference
  - WALKTHROUGH.md - Technical implementation deep dive
  - tests/README.md - Test program documentation

### Fixed

#### BHV2 Parser - Critical Bug Fixes

Two critical bugs in BHV2 file parsing prevented reading most real-world files:

**1. Struct field name skipping** (`src/bhv2.c:357-376`)
- **Problem**: When skipping MATLAB struct arrays during sequential file reading, the code skipped field VALUES but not field NAMES, causing file position desync
- **Impact**: Could not read past FileInfo struct (first complex struct in BHV2 files)
- **Fix**: Modified `skip_array_data_posix()` to read `name_len` and call `skip_bytes_posix()` for each struct field name before skipping the field value
- **Result**: FileInfo and other struct variables now parse correctly

**2. Cell element name skipping** (`src/bhv2.c:377-389`)
- **Problem**: Cell array elements have format `[name_len][name][dtype][dims][data]`, but code called `bhv2_skip_value_posix()` which expects format `[dtype][dims][data]`, causing it to misinterpret name_len as dtype_len
- **Impact**: Could not read past MLConfig struct (contains EyeTracerShape cell array at field #12)
- **Fix**: Modified `skip_array_data_posix()` to read `name_len` and call `skip_bytes_posix()` before calling `bhv2_skip_value_posix()` for each cell element
- **Result**: MLConfig and other structs containing cell arrays now parse correctly

These fixes enable reading **all BHV2 variables** including Trial1, Trial2, ..., TrialN.

### Changed

- **Architecture**: Pure C implementation instead of Python
  - No external dependencies (was: numpy, scipy, Python runtime)
  - Direct POSIX file I/O instead of Python C extension
  - Streaming API optimized for sequential trial reading

- **API Style**: Grab-style iterator pattern
  - Clean iterator interface matching grab tool conventions
  - Explicit file handle passing (no global state)
  - Performance-optimized with data skip option

- **Build System**: Simple Makefile
  - Direct compilation with gcc/clang
  - No build dependencies beyond C compiler
  - Single `make` command builds complete tool

### Implementation Details

#### Modified Files

**src/bhv2.h**:
- Added trial state tracking fields to `bhv2_file_t` struct:
  - `current_trial_num`, `current_trial_error`, `current_trial_condition`
  - `current_trial_block`, `current_trial_data`, `has_current_trial`
- Added grab-style API function declarations (~30 lines)
- Added `WITH_DATA` (0) and `SKIP_DATA` (1) constants

**src/bhv2.c**:
- Modified `skip_array_data_posix()` function:
  - Lines ~357-376: Added field name reading/skipping for `MATLAB_STRUCT`
  - Lines ~377-389: Added element name reading/skipping for `MATLAB_CELL`
- Added grab-style iterator implementation (~150 lines):
  - `open_input_file()` / `close_input_file()` wrappers
  - `read_next_trial()` - Main iterator with trial detection logic
  - `rewind_input_file()` - File position reset
  - `skip_over_data()` - Conditional data skip
  - `extract_trial_info()` - Parse trial struct for metadata
  - `clear_trial_state()` - Reset trial state between reads
  - Header accessor functions: `*_header()` pattern

#### File Organization

- Created `tests/` directory for test programs
- Moved `test_iterator.c` and `debug_vars.c` to `tests/`
- Added `.gitignore` for build artifacts
- Added documentation files (README.md, API.md, CHANGELOG.md)

### Not Planned

Features from Python version not planned for C version:

- **bhvq query mode** - jq-like query syntax for BHV2 files
  - Removed stub code (`src/bhvq_*.c`) to simplify codebase
  - Use Python version for query functionality: https://github.com/MooshLab/presto

### Not Yet Implemented

Features that may be added in future:

- **Plot stdout output** (`-O -` for graphical macros)
  - Requires capturing gnuplot PDF output to buffer
  - Use file output instead: `-O results/` or default working directory

- **Advanced plot customization**
  - Python version has: `-og` (grid toggle), `-as2` (alignment), `-ob 1,2,4` (button selection)
  - C version auto-detects and plots all available data
  - C version auto-detects and plots all available data

### Note

- This C implementation was developed on the `c-port` branch and merged to `main`
- Graphical macros added to `main` branch
- bhvq stub files removed to simplify codebase
- Python implementation available as separate repository
- Both versions read the same BHV2 format and produce compatible output
- C version prioritizes speed and minimal dependencies (zero dependencies for text macros, gnuplot for graphical macros)
- Python version prioritizes feature completeness (bhvq query mode, advanced plotting customization)

---

## Technical Details

### BHV2 File Format

BHV2 files store MATLAB variables in binary format:
1. **IndexPosition** - uint64 specifying file index location
2. **FileInfo** - struct with machine format and encoding
3. **MLConfig** - struct with 83 MonkeyLogic configuration fields
4. **TrialRecord** - struct with session metadata
5. **Trial1**, **Trial2**, ..., **TrialN** - Individual trial data structs
6. **FileIndex** - struct mapping variable names to file positions

### Sequential Reading Requirements

Files must be read sequentially because:
- Variables have complex nested structures (structs, cells, numeric arrays)
- Each variable header specifies type and dimensions
- Some types require recursive parsing (nested structs, cell arrays)
- Skip functions must correctly handle all type-specific metadata

The bugs fixed in this release were preventing sequential reading past complex structs, making the file effectively unreadable beyond the first few variables.

### Testing

All tests pass with sample files:
- `sample_10_successes.bhv2` - 10 correct trials
- `sample_10_errors.bhv2` - 10 error trials with mixed error codes

Test coverage:
- `test_iterator`: Validates grab-style API (SKIP_DATA, WITH_DATA, rewind)
- `debug_vars`: Confirms all variables readable (15 variables including all trials)
- Macro 0-5: All produce correct output matching Python version

---

## Performance

Preliminary benchmarks show C version is **2-5x faster** than Python version for typical operations:
- Header-only reads with `SKIP_DATA`: ~3-5x faster
- Full data reads with `WITH_DATA`: ~2-3x faster
- File parsing overhead: ~4-5x faster (no Python interpreter startup)

---

## Development History

This C implementation was developed to address:
1. **Deployment complexity** - Python requires pip, virtualenvs, dependencies
2. **Performance** - C is faster for file I/O and parsing
3. **API clarity** - Grab-style pattern is cleaner than variable-by-variable reading
4. **Maintenance** - Pure C is easier to debug and profile than Python C extensions

The grab-style API was inspired by the grab neurophysiology tool (circa 2018), which provided a clean iterator pattern for trial reading that has proven successful in production use for many years.

---

## See Also

- **Python implementation**: https://github.com/MooshLab/presto
- **Grab tool**: Original grab neurophysiology tool that inspired the API design
- **MonkeyLogic**: https://monkeylogic.net - Behavioral control system for neuroscience experiments
