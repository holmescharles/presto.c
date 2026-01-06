/*
 * bhv2.h - BHV2 file format parser for MonkeyLogic behavioral data
 *
 * Pure C implementation with no external dependencies.
 * Reads MATLAB-serialized binary format (little-endian, column-major).
 */

#ifndef BHV2_H
#define BHV2_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>  /* for off_t */

/*
 * Constants
 */

#define BHV2_MAX_NAME_LENGTH   10000
#define BHV2_MAX_TYPE_LENGTH   100
#define BHV2_MAX_NDIMS         100
#define BHV2_MAX_FIELDS        1000

/*
 * Data types
 */

typedef enum {
    MATLAB_DOUBLE,
    MATLAB_SINGLE,
    MATLAB_UINT8,
    MATLAB_UINT16,
    MATLAB_UINT32,
    MATLAB_UINT64,
    MATLAB_INT8,
    MATLAB_INT16,
    MATLAB_INT32,
    MATLAB_INT64,
    MATLAB_LOGICAL,
    MATLAB_CHAR,
    MATLAB_STRUCT,
    MATLAB_CELL,
    MATLAB_UNKNOWN
} matlab_dtype_t;

/*
 * Value - tagged union for all possible values
 */

typedef struct bhv2_value bhv2_value_t;
typedef struct bhv2_struct_field bhv2_struct_field_t;

struct bhv2_struct_field {
    char *name;
    bhv2_value_t *value;
};

struct bhv2_value {
    matlab_dtype_t dtype;
    uint64_t ndims;
    uint64_t *dims;         /* Array of dimension sizes */
    uint64_t total;         /* Total number of elements */
    
    union {
        double *d;          /* double array */
        float *f;           /* single (float) array */
        uint8_t *u8;
        uint16_t *u16;
        uint32_t *u32;
        uint64_t *u64;
        int8_t *i8;
        int16_t *i16;
        int32_t *i32;
        int64_t *i64;
        bool *logical;
        char *string;       /* char array (null-terminated string) */
        
        /* Struct array: array of field arrays */
        struct {
            uint64_t n_fields;
            bhv2_struct_field_t *fields;  /* Array of n_fields * total fields */
        } struct_array;
        
        /* Cell array: array of values */
        bhv2_value_t **cell_array;
    } data;
};

/*
 * Variable - named value with file position info
 */

typedef struct {
    char *name;
    bhv2_value_t *value;
    long file_pos;          /* Position in file (for lazy loading) */
} bhv2_variable_t;

/*
 * File - collection of top-level variables (streaming mode)
 */

typedef struct {
    char *path;
    int file_descriptor;     /* POSIX file descriptor for streaming */
    off_t file_size;         /* Total file size */
    off_t current_pos;       /* Current read position */
    bool at_variable_data;   /* Are we positioned at variable data? */
} bhv2_file_t;

/*
 * Error handling
 */

typedef enum {
    BHV2_OK = 0,
    BHV2_ERR_IO,
    BHV2_ERR_MEMORY,
    BHV2_ERR_FORMAT,
    BHV2_ERR_NOT_FOUND
} bhv2_error_t;

/* Get human-readable error message */
const char* bhv2_strerror(bhv2_error_t err);

/* Last error (thread-local in future) */
extern bhv2_error_t bhv2_last_error;
extern char bhv2_error_detail[256];

/*
 * Type utilities
 */

/* Convert dtype string to enum */
matlab_dtype_t matlab_dtype_from_string(const char *string);

/* Convert enum to dtype string */
const char* matlab_dtype_to_string(matlab_dtype_t dtype);

/* Get element size in bytes (0 for struct/cell) */
size_t matlab_dtype_size(matlab_dtype_t dtype);

/*
 * Memory management
 */

/* Allocate a new value */
bhv2_value_t* bhv2_value_new(matlab_dtype_t dtype, uint64_t ndims, uint64_t *dims);

/* Free a value and all its contents */
void bhv2_value_free(bhv2_value_t *value);

/* Free a variable struct */
void bhv2_variable_free(bhv2_variable_t *variable);

/* Free a file struct */
void bhv2_file_free(bhv2_file_t *file);

/*
 * Streaming API - for memory-efficient reading
 */

/* Open BHV2 file for streaming */
bhv2_file_t* bhv2_open_stream(const char *path);

/* Read next variable name (returns 0 on success, -1 on EOF/error)
 * Caller must free returned name with free()
 */
int bhv2_read_next_variable_name(bhv2_file_t *file, char **name_out);

/* Read variable data (after reading name) 
 * Caller must free returned value with bhv2_value_free()
 */
bhv2_value_t* bhv2_read_variable_data(bhv2_file_t *file);

/* Read variable data selectively (only specified struct fields)
 * wanted_fields: NULL-terminated array of field names to read
 * Caller must free returned value with bhv2_value_free()
 */
bhv2_value_t* bhv2_read_variable_data_selective(bhv2_file_t *file, const char **wanted_fields);

/* Skip variable data (after reading name) */
int bhv2_skip_variable_data(bhv2_file_t *file);

/* Read complete variable (name + data)
 * Caller must free with bhv2_variable_free()
 */
bhv2_variable_t* bhv2_read_next_variable(bhv2_file_t *file);

/*
 * Value accessor helpers
 */

/* Navigate into a struct value by field name */
bhv2_value_t* bhv2_struct_get(bhv2_value_t *value, const char *field, uint64_t index);

/* Get cell element */
bhv2_value_t* bhv2_cell_get(bhv2_value_t *value, uint64_t index);

/* Get scalar value (returns 0.0 if not numeric or out of bounds) */
double bhv2_get_double(bhv2_value_t *value, uint64_t index);

/* Get string value (returns NULL if not char array) */
const char* bhv2_get_string(bhv2_value_t *value);

/*
 * Index conversion (MATLAB column-major to linear)
 */

/* Convert 1-based MATLAB indices to 0-based linear index */
uint64_t bhv2_sub2ind(bhv2_value_t *value, uint64_t *indices, uint64_t n_indices);

/* Convert 0-based linear index to 1-based MATLAB indices */
void bhv2_ind2sub(bhv2_value_t *value, uint64_t index, uint64_t *indices);

#endif /* BHV2_H */
