/*
 * bhv2.c - BHV2 file format parser implementation
 *
 * Streaming implementation for reading MonkeyLogic behavioral data.
 * Uses POSIX I/O (open, read, lseek) like grab tool for memory efficiency.
 */

#define _POSIX_C_SOURCE 200809L  /* For strdup, lseek */

#include "bhv2.h"
#include "skip.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>    /* isdigit */
#include <fcntl.h>    /* open */
#include <unistd.h>   /* lseek, close */
#include <sys/stat.h> /* fstat */

/*
 * Global error state
 */

bhv2_error_t bhv2_last_error = BHV2_OK;
char bhv2_error_detail[256] = {0};

const char* bhv2_strerror(bhv2_error_t err) {
    switch (err) {
        case BHV2_OK:        return "No error";
        case BHV2_ERR_IO:    return "I/O error";
        case BHV2_ERR_MEMORY: return "Memory allocation failed";
        case BHV2_ERR_FORMAT: return "Invalid file format";
        case BHV2_ERR_NOT_FOUND: return "Not found";
        default:             return "Unknown error";
    }
}

static void set_error(bhv2_error_t err, const char *detail) {
    bhv2_last_error = err;
    if (detail) {
        strncpy(bhv2_error_detail, detail, sizeof(bhv2_error_detail) - 1);
        bhv2_error_detail[sizeof(bhv2_error_detail) - 1] = '\0';
    } else {
        bhv2_error_detail[0] = '\0';
    }
}

/*
 * POSIX I/O helpers
 */

static int read_uint64_posix(int file_descriptor, uint64_t *value) {
    if (read(file_descriptor, value, 8) != 8) {
        set_error(BHV2_ERR_IO, "Failed to read uint64");
        return -1;
    }
    return 0;
}

static char* read_string_posix(int file_descriptor, uint64_t length) {
    if (length == 0) return strdup("");
    
    char *string = malloc(length + 1);
    if (!string) {
        set_error(BHV2_ERR_MEMORY, "Failed to allocate string");
        return NULL;
    }
    
    if (read(file_descriptor, string, length) != (ssize_t)length) {
        free(string);
        set_error(BHV2_ERR_IO, "Failed to read string");
        return NULL;
    }
    
    string[length] = '\0';
    return string;
}

static void skip_bytes_posix(int file_descriptor, size_t count) {
    lseek(file_descriptor, count, SEEK_CUR);
}

/*
 * Type utilities
 */

matlab_dtype_t matlab_dtype_from_string(const char *string) {
    if (strcmp(string, "double") == 0)  return MATLAB_DOUBLE;
    if (strcmp(string, "single") == 0)  return MATLAB_SINGLE;
    if (strcmp(string, "uint8") == 0)   return MATLAB_UINT8;
    if (strcmp(string, "uint16") == 0)  return MATLAB_UINT16;
    if (strcmp(string, "uint32") == 0)  return MATLAB_UINT32;
    if (strcmp(string, "uint64") == 0)  return MATLAB_UINT64;
    if (strcmp(string, "int8") == 0)    return MATLAB_INT8;
    if (strcmp(string, "int16") == 0)   return MATLAB_INT16;
    if (strcmp(string, "int32") == 0)   return MATLAB_INT32;
    if (strcmp(string, "int64") == 0)   return MATLAB_INT64;
    if (strcmp(string, "logical") == 0) return MATLAB_LOGICAL;
    if (strcmp(string, "char") == 0)    return MATLAB_CHAR;
    if (strcmp(string, "struct") == 0)  return MATLAB_STRUCT;
    if (strcmp(string, "cell") == 0)    return MATLAB_CELL;
    return MATLAB_UNKNOWN;
}

const char* matlab_dtype_to_string(matlab_dtype_t dtype) {
    switch (dtype) {
        case MATLAB_DOUBLE:  return "double";
        case MATLAB_SINGLE:  return "single";
        case MATLAB_UINT8:   return "uint8";
        case MATLAB_UINT16:  return "uint16";
        case MATLAB_UINT32:  return "uint32";
        case MATLAB_UINT64:  return "uint64";
        case MATLAB_INT8:    return "int8";
        case MATLAB_INT16:   return "int16";
        case MATLAB_INT32:   return "int32";
        case MATLAB_INT64:   return "int64";
        case MATLAB_LOGICAL: return "logical";
        case MATLAB_CHAR:    return "char";
        case MATLAB_STRUCT:  return "struct";
        case MATLAB_CELL:    return "cell";
        default:           return "unknown";
    }
}

size_t matlab_dtype_size(matlab_dtype_t dtype) {
    switch (dtype) {
        case MATLAB_DOUBLE:  return 8;
        case MATLAB_SINGLE:  return 4;
        case MATLAB_UINT8:   return 1;
        case MATLAB_UINT16:  return 2;
        case MATLAB_UINT32:  return 4;
        case MATLAB_UINT64:  return 8;
        case MATLAB_INT8:    return 1;
        case MATLAB_INT16:   return 2;
        case MATLAB_INT32:   return 4;
        case MATLAB_INT64:   return 8;
        case MATLAB_LOGICAL: return 1;
        case MATLAB_CHAR:    return 1;
        default:           return 0;  /* struct, cell have variable size */
    }
}

/*
 * Value allocation and deallocation
 */

bhv2_value_t* bhv2_value_new(matlab_dtype_t dtype, uint64_t ndims, uint64_t *dims) {
    bhv2_value_t *value = calloc(1, sizeof(bhv2_value_t));
    if (!value) {
        set_error(BHV2_ERR_MEMORY, "Failed to allocate value");
        return NULL;
    }
    
    value->dtype = dtype;
    value->ndims = ndims;
    
    value->dims = malloc(ndims * sizeof(uint64_t));
    if (!value->dims) {
        free(value);
        set_error(BHV2_ERR_MEMORY, "Failed to allocate dims");
        return NULL;
    }
    
    memcpy(value->dims, dims, ndims * sizeof(uint64_t));
    
    value->total = 1;
    for (uint64_t i = 0; i < ndims; i++) {
        value->total *= dims[i];
    }
    
    return value;
}

void bhv2_value_free(bhv2_value_t *value) {
    if (!value) return;
    
    free(value->dims);
    
    switch (value->dtype) {
        case MATLAB_DOUBLE:  free(value->data.d); break;
        case MATLAB_SINGLE:  free(value->data.f); break;
        case MATLAB_UINT8:   free(value->data.u8); break;
        case MATLAB_UINT16:  free(value->data.u16); break;
        case MATLAB_UINT32:  free(value->data.u32); break;
        case MATLAB_UINT64:  free(value->data.u64); break;
        case MATLAB_INT8:    free(value->data.i8); break;
        case MATLAB_INT16:   free(value->data.i16); break;
        case MATLAB_INT32:   free(value->data.i32); break;
        case MATLAB_INT64:   free(value->data.i64); break;
        case MATLAB_LOGICAL: free(value->data.logical); break;
        case MATLAB_CHAR:    free(value->data.string); break;
        case MATLAB_STRUCT:
            if (value->data.struct_array.fields) {
                uint64_t n_fields = value->data.struct_array.n_fields;
                for (uint64_t i = 0; i < n_fields * value->total; i++) {
                    free(value->data.struct_array.fields[i].name);
                    bhv2_value_free(value->data.struct_array.fields[i].value);
                }
                free(value->data.struct_array.fields);
            }
            break;
        case MATLAB_CELL:
            if (value->data.cell_array) {
                for (uint64_t i = 0; i < value->total; i++) {
                    bhv2_value_free(value->data.cell_array[i]);
                }
                free(value->data.cell_array);
            }
            break;
        default:
            break;
    }
    
    free(value);
}

void bhv2_variable_free(bhv2_variable_t *variable) {
    if (!variable) return;
    free(variable->name);
    bhv2_value_free(variable->value);
    free(variable);
}

void bhv2_file_free(bhv2_file_t *file) {
    if (!file) return;
    free(file->path);
    if (file->file_descriptor >= 0) {
        close(file->file_descriptor);
    }
    free(file);
}

/*
 * POSIX-based value reading (streaming)
 */

static bhv2_value_t* read_numeric_array_posix(int file_descriptor, matlab_dtype_t dtype, uint64_t ndims, uint64_t *dims);
static bhv2_value_t* read_char_array_posix(int file_descriptor, uint64_t ndims, uint64_t *dims);
static bhv2_value_t* read_struct_array_posix(int file_descriptor, uint64_t ndims, uint64_t *dims);
static bhv2_value_t* read_cell_array_posix(int file_descriptor, uint64_t ndims, uint64_t *dims);
static bhv2_value_t* read_array_data_posix(int file_descriptor, matlab_dtype_t dtype, uint64_t ndims, uint64_t *dims);
static int skip_array_data_posix(int file_descriptor, matlab_dtype_t dtype, uint64_t ndims, uint64_t *dims);

bhv2_value_t* bhv2_read_value_posix(int file_descriptor) {
    /* Read dtype */
    uint64_t dtype_len;
    if (read_uint64_posix(file_descriptor, &dtype_len) < 0) {
        return NULL;
    }

    if (dtype_len > BHV2_MAX_TYPE_LENGTH) {
        set_error(BHV2_ERR_FORMAT, "Type name too long");
        return NULL;
    }

    char *dtype_string = read_string_posix(file_descriptor, dtype_len);
    if (!dtype_string) {
        return NULL;
    }

    matlab_dtype_t dtype = matlab_dtype_from_string(dtype_string);
    free(dtype_string);

    if (dtype == MATLAB_UNKNOWN) {
        set_error(BHV2_ERR_FORMAT, "Unknown dtype");
        return NULL;
    }

    /* Read dimensions */
    uint64_t ndims;
    if (read_uint64_posix(file_descriptor, &ndims) < 0) {
        return NULL;
    }

    if (ndims > BHV2_MAX_NDIMS) {
        set_error(BHV2_ERR_FORMAT, "Too many dimensions");
        return NULL;
    }

    uint64_t *dims = malloc(ndims * sizeof(uint64_t));
    if (!dims) {
        set_error(BHV2_ERR_MEMORY, "Failed to allocate dims");
        return NULL;
    }

    if (read(file_descriptor, dims, ndims * sizeof(uint64_t)) != (ssize_t)(ndims * sizeof(uint64_t))) {
        free(dims);
        set_error(BHV2_ERR_IO, "Failed to read dims");
        return NULL;
    }

    /* Read array data */
    bhv2_value_t *value = read_array_data_posix(file_descriptor, dtype, ndims, dims);
    free(dims);

    return value;
}

int bhv2_skip_value_posix(int file_descriptor) {
    /* Read dtype */
    uint64_t dtype_len;
    if (read_uint64_posix(file_descriptor, &dtype_len) < 0) {
        return -1;
    }

    if (dtype_len > BHV2_MAX_TYPE_LENGTH) {
        set_error(BHV2_ERR_FORMAT, "Type name too long");
        return -1;
    }

    char *dtype_string = read_string_posix(file_descriptor, dtype_len);
    if (!dtype_string) {
        return -1;
    }

    matlab_dtype_t dtype = matlab_dtype_from_string(dtype_string);
    free(dtype_string);

    if (dtype == MATLAB_UNKNOWN) {
        set_error(BHV2_ERR_FORMAT, "Unknown dtype");
        return -1;
    }

    /* Read dimensions */
    uint64_t ndims;
    if (read_uint64_posix(file_descriptor, &ndims) < 0) {
        return -1;
    }

    if (ndims > BHV2_MAX_NDIMS) {
        set_error(BHV2_ERR_FORMAT, "Too many dimensions");
        return -1;
    }

    uint64_t *dims = malloc(ndims * sizeof(uint64_t));
    if (!dims) {
        set_error(BHV2_ERR_MEMORY, "Failed to allocate dims");
        return -1;
    }

    if (read(file_descriptor, dims, ndims * sizeof(uint64_t)) != (ssize_t)(ndims * sizeof(uint64_t))) {
        free(dims);
        set_error(BHV2_ERR_IO, "Failed to read dims");
        return -1;
    }

    /* Skip data based on type */
    int result = skip_array_data_posix(file_descriptor, dtype, ndims, dims);
    free(dims);
    return result;
}

static int skip_array_data_posix(int file_descriptor, matlab_dtype_t dtype, uint64_t ndims, uint64_t *dims) {
    uint64_t total = 1;
    for (uint64_t i = 0; i < ndims; i++) {
        total *= dims[i];
    }

    if (dtype == MATLAB_STRUCT) {
        /* Read field count */
        uint64_t n_fields;
        if (read_uint64_posix(file_descriptor, &n_fields) < 0) return -1;

        /* Skip each element's fields (names + values) */
        for (uint64_t elem = 0; elem < total; elem++) {
            for (uint64_t f = 0; f < n_fields; f++) {
                /* Skip field name length and name */
                uint64_t name_len;
                if (read_uint64_posix(file_descriptor, &name_len) < 0) return -1;
                skip_bytes_posix(file_descriptor, name_len);
                
                /* Skip field value */
                if (bhv2_skip_value_posix(file_descriptor) < 0) return -1;
            }
        }
        return 0;
    }

    if (dtype == MATLAB_CELL) {
        /* Skip each cell element (which has name prefix) */
        for (uint64_t i = 0; i < total; i++) {
            /* Cell elements have format: [name_len][name][dtype][dims][data] */
            uint64_t name_len;
            if (read_uint64_posix(file_descriptor, &name_len) < 0) return -1;
            skip_bytes_posix(file_descriptor, name_len);
            
            /* Now skip the actual value */
            if (bhv2_skip_value_posix(file_descriptor) < 0) return -1;
        }
        return 0;
    }

    /* Numeric/char: calculate size and seek */
    size_t elem_size = matlab_dtype_size(dtype);
    if (elem_size == 0) return -1;  /* struct/cell should have been handled above */

    skip_bytes_posix(file_descriptor, total * elem_size);
    return 0;
}

static bhv2_value_t* read_numeric_array_posix(int file_descriptor, matlab_dtype_t dtype, uint64_t ndims, uint64_t *dims) {
    bhv2_value_t *value = bhv2_value_new(dtype, ndims, dims);
    if (!value) return NULL;

    size_t elem_size = matlab_dtype_size(dtype);
    size_t total_bytes = value->total * elem_size;

    void *data = malloc(total_bytes);
    if (!data) {
        bhv2_value_free(value);
        set_error(BHV2_ERR_MEMORY, "Failed to allocate array data");
        return NULL;
    }

    if (read(file_descriptor, data, total_bytes) != (ssize_t)total_bytes) {
        free(data);
        bhv2_value_free(value);
        set_error(BHV2_ERR_IO, "Failed to read array data");
        return NULL;
    }

    /* Store in appropriate union member */
    switch (dtype) {
        case MATLAB_DOUBLE:  value->data.d = (double*)data; break;
        case MATLAB_SINGLE:  value->data.f = (float*)data; break;
        case MATLAB_UINT8:   value->data.u8 = (uint8_t*)data; break;
        case MATLAB_UINT16:  value->data.u16 = (uint16_t*)data; break;
        case MATLAB_UINT32:  value->data.u32 = (uint32_t*)data; break;
        case MATLAB_UINT64:  value->data.u64 = (uint64_t*)data; break;
        case MATLAB_INT8:    value->data.i8 = (int8_t*)data; break;
        case MATLAB_INT16:   value->data.i16 = (int16_t*)data; break;
        case MATLAB_INT32:   value->data.i32 = (int32_t*)data; break;
        case MATLAB_INT64:   value->data.i64 = (int64_t*)data; break;
        case MATLAB_LOGICAL: value->data.logical = (bool*)data; break;
        default:
            free(data);
            bhv2_value_free(value);
            set_error(BHV2_ERR_FORMAT, "Unexpected dtype in numeric read");
            return NULL;
    }

    return value;
}

static bhv2_value_t* read_char_array_posix(int file_descriptor, uint64_t ndims, uint64_t *dims) {
    bhv2_value_t *value = bhv2_value_new(MATLAB_CHAR, ndims, dims);
    if (!value) return NULL;

    /* MATLAB char arrays are 1xN or MxN; we flatten to string */
    char *string = (char*)malloc(value->total + 1);
    if (!string) {
        bhv2_value_free(value);
        set_error(BHV2_ERR_MEMORY, "Failed to allocate string");
        return NULL;
    }

    if (value->total > 0 && read(file_descriptor, string, value->total) != (ssize_t)value->total) {
        free(string);
        bhv2_value_free(value);
        set_error(BHV2_ERR_IO, "Failed to read char array");
        return NULL;
    }

    string[value->total] = '\0';
    value->data.string = string;

    return value;
}

static bhv2_value_t* read_struct_array_posix(int file_descriptor, uint64_t ndims, uint64_t *dims) {
    bhv2_value_t *value = bhv2_value_new(MATLAB_STRUCT, ndims, dims);
    if (!value) return NULL;
    
    /* Read field count */
    uint64_t n_fields;
    if (read_uint64_posix(file_descriptor, &n_fields) < 0) {
        bhv2_value_free(value);
        return NULL;
    }
    
    value->data.struct_array.n_fields = n_fields;
    
    /* Allocate field storage: n_elements * n_fields */
    uint64_t total_fields = value->total * n_fields;
    value->data.struct_array.fields = (bhv2_struct_field_t*)calloc(total_fields, sizeof(bhv2_struct_field_t));
    if (!value->data.struct_array.fields) {
        bhv2_value_free(value);
        set_error(BHV2_ERR_MEMORY, "Failed to allocate struct fields");
        return NULL;
    }
    
    /* Read each element's fields */
    for (uint64_t elem = 0; elem < value->total; elem++) {
        for (uint64_t f = 0; f < n_fields; f++) {
            uint64_t idx = elem * n_fields + f;
            
            /* Read field name */
            uint64_t name_len;
            if (read_uint64_posix(file_descriptor, &name_len) < 0) {
                bhv2_value_free(value);
                return NULL;
            }
            
            if (name_len > BHV2_MAX_NAME_LENGTH) {
                bhv2_value_free(value);
                set_error(BHV2_ERR_FORMAT, "Field name too long");
                return NULL;
            }
            
            value->data.struct_array.fields[idx].name = read_string_posix(file_descriptor, name_len);
            if (!value->data.struct_array.fields[idx].name) {
                bhv2_value_free(value);
                return NULL;
            }
            
            /* Read field value (recursive) - call bhv2_read_value_posix */
            value->data.struct_array.fields[idx].value = bhv2_read_value_posix(file_descriptor);
            if (!value->data.struct_array.fields[idx].value) {
                bhv2_value_free(value);
                return NULL;
            }
        }
    }
    
    return value;
}

/*
 * Read struct array selectively - only read specified fields, skip the rest.
 * wanted_fields is a NULL-terminated array of field names to read.
 * Other fields are skipped (not allocated).
 */
static bhv2_value_t* read_struct_selective_posix(int file_descriptor, uint64_t ndims, uint64_t *dims,
                                                  const char **wanted_fields) {
    bhv2_value_t *value = bhv2_value_new(MATLAB_STRUCT, ndims, dims);
    if (!value) return NULL;
    
    /* Read field count */
    uint64_t n_fields;
    if (read_uint64_posix(file_descriptor, &n_fields) < 0) {
        bhv2_value_free(value);
        return NULL;
    }
    
    value->data.struct_array.n_fields = n_fields;
    
    /* Allocate field storage: n_elements * n_fields */
    uint64_t total_fields = value->total * n_fields;
    value->data.struct_array.fields = (bhv2_struct_field_t*)calloc(total_fields, sizeof(bhv2_struct_field_t));
    if (!value->data.struct_array.fields) {
        bhv2_value_free(value);
        set_error(BHV2_ERR_MEMORY, "Failed to allocate struct fields");
        return NULL;
    }
    
    /* Read each element's fields */
    for (uint64_t elem = 0; elem < value->total; elem++) {
        for (uint64_t f = 0; f < n_fields; f++) {
            uint64_t idx = elem * n_fields + f;
            
            /* Read field name */
            uint64_t name_len;
            if (read_uint64_posix(file_descriptor, &name_len) < 0) {
                bhv2_value_free(value);
                return NULL;
            }
            
            if (name_len > BHV2_MAX_NAME_LENGTH) {
                bhv2_value_free(value);
                set_error(BHV2_ERR_FORMAT, "Field name too long");
                return NULL;
            }
            
            char *field_name = read_string_posix(file_descriptor, name_len);
            if (!field_name) {
                bhv2_value_free(value);
                return NULL;
            }
            
            /* Check if this field is wanted */
            bool wanted = false;
            if (wanted_fields) {
                for (const char **w = wanted_fields; *w; w++) {
                    if (strcmp(field_name, *w) == 0) {
                        wanted = true;
                        break;
                    }
                }
            }
            
            if (wanted) {
                /* Store name and read value */
                value->data.struct_array.fields[idx].name = field_name;
                value->data.struct_array.fields[idx].value = bhv2_read_value_posix(file_descriptor);
                if (!value->data.struct_array.fields[idx].value) {
                    bhv2_value_free(value);
                    return NULL;
                }
            } else {
                /* Skip this field - don't store name or value */
                free(field_name);
                value->data.struct_array.fields[idx].name = NULL;
                value->data.struct_array.fields[idx].value = NULL;
                if (bhv2_skip_value_posix(file_descriptor) < 0) {
                    bhv2_value_free(value);
                    return NULL;
                }
            }
        }
    }
    
    return value;
}

static bhv2_value_t* read_cell_array_posix(int file_descriptor, uint64_t ndims, uint64_t *dims) {
    bhv2_value_t *value = bhv2_value_new(MATLAB_CELL, ndims, dims);
    if (!value) return NULL;
    
    value->data.cell_array = (bhv2_value_t**)calloc(value->total, sizeof(bhv2_value_t*));
    if (!value->data.cell_array) {
        bhv2_value_free(value);
        set_error(BHV2_ERR_MEMORY, "Failed to allocate cells");
        return NULL;
    }
    
    /* Read each cell element (recursive) */
    for (uint64_t i = 0; i < value->total; i++) {
        /* Each cell stores: [name_len][name][dtype_len][dtype][ndims][dims][data] */
        /* but cell elements have empty names */
        
        uint64_t name_len;
        if (read_uint64_posix(file_descriptor, &name_len) < 0) {
            bhv2_value_free(value);
            return NULL;
        }
        
        /* Skip name (usually empty for cell elements) */
        if (name_len > 0) {
            skip_bytes_posix(file_descriptor, name_len);
        }
        
        /* Read cell value (recursive) - rest is handled by bhv2_read_value_posix */
        /* But we need to read dtype, ndims, dims manually since we already read name_len */
        
        /* Read dtype */
        uint64_t dtype_len;
        if (read_uint64_posix(file_descriptor, &dtype_len) < 0) {
            bhv2_value_free(value);
            return NULL;
        }
        
        if (dtype_len > BHV2_MAX_TYPE_LENGTH) {
            bhv2_value_free(value);
            set_error(BHV2_ERR_FORMAT, "Type name too long");
            return NULL;
        }
        
        char *dtype_string = read_string_posix(file_descriptor, dtype_len);
        if (!dtype_string) {
            bhv2_value_free(value);
            return NULL;
        }
        
        matlab_dtype_t cell_dtype = matlab_dtype_from_string(dtype_string);
        free(dtype_string);
        
        if (cell_dtype == MATLAB_UNKNOWN) {
            bhv2_value_free(value);
            set_error(BHV2_ERR_FORMAT, "Unknown dtype in cell");
            return NULL;
        }
        
        /* Read dimensions */
        uint64_t cell_ndims;
        if (read_uint64_posix(file_descriptor, &cell_ndims) < 0) {
            bhv2_value_free(value);
            return NULL;
        }
        
        if (cell_ndims > BHV2_MAX_NDIMS) {
            bhv2_value_free(value);
            set_error(BHV2_ERR_FORMAT, "Too many dimensions in cell");
            return NULL;
        }
        
        uint64_t *cell_dims = (uint64_t*)malloc(cell_ndims * sizeof(uint64_t));
        if (!cell_dims) {
            bhv2_value_free(value);
            set_error(BHV2_ERR_MEMORY, "Failed to allocate cell dims");
            return NULL;
        }
        
        if (read(file_descriptor, cell_dims, cell_ndims * sizeof(uint64_t)) != (ssize_t)(cell_ndims * sizeof(uint64_t))) {
            free(cell_dims);
            bhv2_value_free(value);
            set_error(BHV2_ERR_IO, "Failed to read cell dims");
            return NULL;
        }
        
        /* Read cell data */
        value->data.cell_array[i] = read_array_data_posix(file_descriptor, cell_dtype, cell_ndims, cell_dims);
        free(cell_dims);
        
        if (!value->data.cell_array[i]) {
            bhv2_value_free(value);
            return NULL;
        }
    }
    
    return value;
}

static bhv2_value_t* read_array_data_posix(int file_descriptor, matlab_dtype_t dtype, uint64_t ndims, uint64_t *dims) {
    switch (dtype) {
        case MATLAB_DOUBLE:
        case MATLAB_SINGLE:
        case MATLAB_UINT8:
        case MATLAB_UINT16:
        case MATLAB_UINT32:
        case MATLAB_UINT64:
        case MATLAB_INT8:
        case MATLAB_INT16:
        case MATLAB_INT32:
        case MATLAB_INT64:
        case MATLAB_LOGICAL:
            return read_numeric_array_posix(file_descriptor, dtype, ndims, dims);
            
        case MATLAB_CHAR:
            return read_char_array_posix(file_descriptor, ndims, dims);
            
        case MATLAB_STRUCT:
            return read_struct_array_posix(file_descriptor, ndims, dims);
            
        case MATLAB_CELL:
            return read_cell_array_posix(file_descriptor, ndims, dims);
            
        default:
            set_error(BHV2_ERR_FORMAT, "Unknown dtype");
            return NULL;
    }
}

/*
 * Streaming implementation
 */

bhv2_file_t* bhv2_open_stream(const char *path) {
    int file_descriptor = open(path, O_RDONLY);
    if (file_descriptor < 0) {
        set_error(BHV2_ERR_IO, "Failed to open file");
        return NULL;
    }

    /* Get file size */
    off_t file_size = lseek(file_descriptor, 0, SEEK_END);
    if (file_size < 0) {
        close(file_descriptor);
        set_error(BHV2_ERR_IO, "Failed to get file size");
        return NULL;
    }

    /* Seek back to beginning */
    if (lseek(file_descriptor, 0, SEEK_SET) < 0) {
        close(file_descriptor);
        set_error(BHV2_ERR_IO, "Failed to seek to beginning");
        return NULL;
    }

    bhv2_file_t *file = calloc(1, sizeof(bhv2_file_t));
    if (!file) {
        close(file_descriptor);
        set_error(BHV2_ERR_MEMORY, "Failed to allocate file struct");
        return NULL;
    }

    file->path = strdup(path);
    if (!file->path) {
        close(file_descriptor);
        free(file);
        set_error(BHV2_ERR_MEMORY, "Failed to copy path");
        return NULL;
    }

    file->file_descriptor = file_descriptor;
    file->file_size = file_size;
    file->current_pos = 0;
    file->at_variable_data = false;

    return file;
}

int bhv2_read_next_variable_name(bhv2_file_t *file, char **name_out) {
    if (!file) return -1;
    
    /* Check if we're at EOF */
    if (file->current_pos >= file->file_size) {
        return -1;
    }
    
    /* Read variable name length */
    uint64_t name_len;
    if (read_uint64_posix(file->file_descriptor, &name_len) < 0) {
        return -1;
    }
    
    if (name_len > BHV2_MAX_NAME_LENGTH) {
        set_error(BHV2_ERR_FORMAT, "Variable name too long");
        return -1;
    }
    
    /* Read name */
    char *name = read_string_posix(file->file_descriptor, name_len);
    if (!name) {
        return -1;
    }
    
    *name_out = name;
    file->at_variable_data = true;
    file->current_pos = lseek(file->file_descriptor, 0, SEEK_CUR);
    
    return 0;
}

bhv2_value_t* bhv2_read_variable_data(bhv2_file_t *file) {
    if (!file || !file->at_variable_data) {
        set_error(BHV2_ERR_FORMAT, "Not positioned at variable data");
        return NULL;
    }
    
    bhv2_value_t *value = bhv2_read_value_posix(file->file_descriptor);
    
    file->at_variable_data = false;
    file->current_pos = lseek(file->file_descriptor, 0, SEEK_CUR);
    
    return value;
}

/*
 * Read variable data selectively - only read specified struct fields.
 * For trial variables, this reads metadata (TrialError, Condition, etc.)
 * while skipping bulk data (AnalogData, ObjectStatusRecord, etc.)
 */
bhv2_value_t* bhv2_read_variable_data_selective(bhv2_file_t *file, const char **wanted_fields) {
    if (!file || !file->at_variable_data) {
        set_error(BHV2_ERR_FORMAT, "Not positioned at variable data");
        return NULL;
    }
    
    int fd = file->file_descriptor;
    
    /* Read dtype */
    uint64_t dtype_len;
    if (read_uint64_posix(fd, &dtype_len) < 0) {
        return NULL;
    }
    
    if (dtype_len > BHV2_MAX_TYPE_LENGTH) {
        set_error(BHV2_ERR_FORMAT, "Type name too long");
        return NULL;
    }
    
    char *dtype_string = read_string_posix(fd, dtype_len);
    if (!dtype_string) {
        return NULL;
    }
    
    matlab_dtype_t dtype = matlab_dtype_from_string(dtype_string);
    free(dtype_string);
    
    if (dtype == MATLAB_UNKNOWN) {
        set_error(BHV2_ERR_FORMAT, "Unknown dtype");
        return NULL;
    }
    
    /* Read dimensions */
    uint64_t ndims;
    if (read_uint64_posix(fd, &ndims) < 0) {
        return NULL;
    }
    
    if (ndims > BHV2_MAX_NDIMS) {
        set_error(BHV2_ERR_FORMAT, "Too many dimensions");
        return NULL;
    }
    
    uint64_t *dims = malloc(ndims * sizeof(uint64_t));
    if (!dims) {
        set_error(BHV2_ERR_MEMORY, "Failed to allocate dims");
        return NULL;
    }
    
    if (read(fd, dims, ndims * sizeof(uint64_t)) != (ssize_t)(ndims * sizeof(uint64_t))) {
        free(dims);
        set_error(BHV2_ERR_IO, "Failed to read dims");
        return NULL;
    }
    
    /* For structs, use selective reader; otherwise fall back to full read */
    bhv2_value_t *value;
    if (dtype == MATLAB_STRUCT) {
        value = read_struct_selective_posix(fd, ndims, dims, wanted_fields);
    } else {
        /* Non-struct: read fully (shouldn't happen for trials) */
        value = read_array_data_posix(fd, dtype, ndims, dims);
    }
    
    free(dims);
    
    file->at_variable_data = false;
    file->current_pos = lseek(fd, 0, SEEK_CUR);
    
    return value;
}

int bhv2_skip_variable_data(bhv2_file_t *file) {
    if (!file || !file->at_variable_data) {
        set_error(BHV2_ERR_FORMAT, "Not positioned at variable data");
        return -1;
    }
    
    if (bhv2_skip_value_posix(file->file_descriptor) < 0) {
        return -1;
    }
    
    file->at_variable_data = false;
    file->current_pos = lseek(file->file_descriptor, 0, SEEK_CUR);
    
    return 0;
}

bhv2_variable_t* bhv2_read_next_variable(bhv2_file_t *file) {
    if (!file) return NULL;
    
    char *name;
    if (bhv2_read_next_variable_name(file, &name) < 0) {
        return NULL;
    }
    
    bhv2_value_t *value = bhv2_read_variable_data(file);
    if (!value) {
        free(name);
        return NULL;
    }

    bhv2_variable_t *variable = malloc(sizeof(bhv2_variable_t));
    if (!variable) {
        free(name);
        bhv2_value_free(value);
        set_error(BHV2_ERR_MEMORY, "Failed to allocate variable");
        return NULL;
    }

    variable->name = name;
    variable->value = value;
    variable->file_pos = file->current_pos;

    return variable;
}

/*
 * Value accessor helpers
 */

bhv2_value_t* bhv2_struct_get(bhv2_value_t *value, const char *field, uint64_t index) {
    if (!value || value->dtype != MATLAB_STRUCT) {
        set_error(BHV2_ERR_FORMAT, "Not a struct");
        return NULL;
    }
    
    if (index >= value->total) {
        set_error(BHV2_ERR_NOT_FOUND, "Index out of bounds");
        return NULL;
    }
    
    uint64_t n_fields = value->data.struct_array.n_fields;
    uint64_t base = index * n_fields;
    
    for (uint64_t f = 0; f < n_fields; f++) {
        /* Skip fields with NULL names (sparse/selective read) */
        if (value->data.struct_array.fields[base + f].name == NULL) {
            continue;
        }
        if (strcmp(value->data.struct_array.fields[base + f].name, field) == 0) {
            return value->data.struct_array.fields[base + f].value;
        }
    }
    
    set_error(BHV2_ERR_NOT_FOUND, "Field not found");
    return NULL;
}

bhv2_value_t* bhv2_cell_get(bhv2_value_t *value, uint64_t index) {
    if (!value || value->dtype != MATLAB_CELL) {
        set_error(BHV2_ERR_FORMAT, "Not a cell array");
        return NULL;
    }
    
    if (index >= value->total) {
        set_error(BHV2_ERR_NOT_FOUND, "Index out of bounds");
        return NULL;
    }
    
    return value->data.cell_array[index];
}

double bhv2_get_double(bhv2_value_t *value, uint64_t index) {
    if (!value || index >= value->total) return 0.0;
    
    switch (value->dtype) {
        case MATLAB_DOUBLE:  return value->data.d[index];
        case MATLAB_SINGLE:  return (double)value->data.f[index];
        case MATLAB_UINT8:   return (double)value->data.u8[index];
        case MATLAB_UINT16:  return (double)value->data.u16[index];
        case MATLAB_UINT32:  return (double)value->data.u32[index];
        case MATLAB_UINT64:  return (double)value->data.u64[index];
        case MATLAB_INT8:    return (double)value->data.i8[index];
        case MATLAB_INT16:   return (double)value->data.i16[index];
        case MATLAB_INT32:   return (double)value->data.i32[index];
        case MATLAB_INT64:   return (double)value->data.i64[index];
        case MATLAB_LOGICAL: return value->data.logical[index] ? 1.0 : 0.0;
        default: return 0.0;
    }
}

const char* bhv2_get_string(bhv2_value_t *value) {
    if (!value || value->dtype != MATLAB_CHAR) return NULL;
    return value->data.string;
}
