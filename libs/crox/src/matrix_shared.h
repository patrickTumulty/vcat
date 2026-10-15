
#ifndef MAT_SHARED_H
#define MAT_SHARED_H

#include "common_types.h"
#include "matrix.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Internal vtable shared between the matrix facade and its implementations. Not
 * part of the public API: callers use matrix.h, greedy_matrix.h, or
 * standard_matrix.h instead. */

/** @brief Read @p size bytes from cell (x, y) into @p out. */
typedef Rc (*matrix_api_get)(matrix mat, i32 x, i32 y, void *out, size_t size);

/** @brief Write @p size bytes from @p v into cell (x, y). */
typedef Rc (*matrix_api_set)(matrix mat, i32 x, i32 y, const void *v, size_t size);

/** @brief Return a pointer to cell (x, y)'s raw bytes, or NULL if out of bounds. */
typedef void *(*matrix_api_get_cell)(matrix mat, i32 x, i32 y);

/** @brief Return the column count. */
typedef i32 (*matrix_api_get_width)(matrix mat);

/** @brief Return the row count. */
typedef i32 (*matrix_api_get_height)(matrix mat);

/** @brief Resize the matrix to @p height rows by @p width columns. */
typedef Rc (*matrix_api_resize)(matrix mat, i32 height, i32 width);

/** @brief Release the matrix and its backing store. */
typedef void (*matrix_api_free)(matrix mat);

/**
 * @brief Function table placed at the start of every concrete matrix so that
 *        the matrix handle can be dispatched without knowing its type.
 */
typedef struct
{
    matrix_api_get get;
    matrix_api_set set;
    matrix_api_get_cell get_cell;
    matrix_api_get_width get_width;
    matrix_api_get_height get_height;
    matrix_api_resize resize;
    matrix_api_free free;
} matrix_internal_hdr;

/**
 * @brief Row-major cell storage shared by the concrete matrix implementations.
 *
 * Every implementation keeps fixed-size cells in a single byte buffer in
 * row-major order, so cell access, bounds checks, dimension queries, and
 * teardown are identical across them. Only the resize policy differs, which
 * each implementation supplies through @c header.resize. @c allocated_bytes
 * records the current allocation size, which lets an implementation reuse the
 * buffer when it is large enough.
 */
typedef struct
{
    matrix_internal_hdr header;
    uint8_t *data;
    size_t allocated_bytes;
    i32 width;
    i32 height;
    size_t type_size;
} matrix_storage;

/* True when a * b cannot be represented as a size_t. */
static inline bool matrix_mul_overflows(size_t a, size_t b)
{
    return b != 0 && a > SIZE_MAX / b;
}

/* Copy `size` bytes from cell (x, y) into `out`. */
static inline Rc matrix_storage_get(matrix mat, i32 x, i32 y, void *out, size_t size)
{
    matrix_storage *storage = (matrix_storage *)mat;
    if (x < 0 || y < 0 || x >= storage->width || y >= storage->height)
        return RC_BAD_INPUT_ARG;
    if (size > storage->type_size)
        return RC_BAD_INPUT_ARG;

    size_t index = ((size_t)y * (size_t)storage->width + (size_t)x) * storage->type_size;
    memcpy(out, storage->data + index, size);
    return RC_OK;
}

/* Copy `size` bytes from `v` into cell (x, y). */
static inline Rc matrix_storage_set(matrix mat, i32 x, i32 y, const void *v, size_t size)
{
    matrix_storage *storage = (matrix_storage *)mat;
    if (x < 0 || y < 0 || x >= storage->width || y >= storage->height)
        return RC_BAD_INPUT_ARG;
    if (size > storage->type_size)
        return RC_BAD_INPUT_ARG;

    size_t index = ((size_t)y * (size_t)storage->width + (size_t)x) * storage->type_size;
    memcpy(storage->data + index, v, size);
    return RC_OK;
}

/* Return a pointer to cell (x, y)'s raw bytes, or NULL if out of bounds. */
static inline void *matrix_storage_get_cell(matrix mat, i32 x, i32 y)
{
    matrix_storage *storage = (matrix_storage *)mat;
    if (x < 0 || y < 0 || x >= storage->width || y >= storage->height)
        return NULL;

    size_t index = ((size_t)y * (size_t)storage->width + (size_t)x) * storage->type_size;
    return storage->data + index;
}

/* Return the column count. */
static inline i32 matrix_storage_get_width(matrix mat)
{
    return ((matrix_storage *)mat)->width;
}

/* Return the row count. */
static inline i32 matrix_storage_get_height(matrix mat)
{
    return ((matrix_storage *)mat)->height;
}

/* Release a matrix's backing store and handle. */
static inline void matrix_storage_free(matrix mat)
{
    matrix_storage *storage = (matrix_storage *)mat;
    free(storage->data);
    free(storage);
}

/*
 * Allocate and initialise a cell-based matrix: validate the dimensions, maybe
 * allocate a zeroed store sized exactly to the request, wire the shared cell
 * accessors into the vtable, and install `resize` as the implementation's
 * resize policy.
 */
static inline Rc matrix_storage_init(matrix *out,      //
                                     i32 height,       //
                                     i32 width,        //
                                     size_t type_size, //
                                     matrix_api_resize resize)
{
    if (out == NULL)
        return RC_BAD_INPUT_ARG;
    *out = NULL;

    if (height <= 0 || width <= 0 || type_size == 0)
        return RC_BAD_INPUT_ARG;

    size_t rows = (size_t)height;
    size_t cols = (size_t)width;
    if (matrix_mul_overflows(rows, cols))
        return RC_BAD_INPUT_ARG;
    size_t cells = rows * cols;
    if (matrix_mul_overflows(cells, type_size))
        return RC_BAD_INPUT_ARG;
    size_t total_bytes = cells * type_size;

    matrix_storage *storage = (matrix_storage *)calloc(1, sizeof(matrix_storage));
    if (storage == NULL)
        return RC_MEM_ALLOC_ERROR;

    storage->data = (uint8_t *)calloc(1, total_bytes);
    if (storage->data == NULL)
    {
        free(storage);
        return RC_MEM_ALLOC_ERROR;
    }

    storage->width = width;
    storage->height = height;
    storage->type_size = type_size;
    storage->allocated_bytes = total_bytes;

    storage->header.get = matrix_storage_get;
    storage->header.set = matrix_storage_set;
    storage->header.get_cell = matrix_storage_get_cell;
    storage->header.get_width = matrix_storage_get_width;
    storage->header.get_height = matrix_storage_get_height;
    storage->header.resize = resize;
    storage->header.free = matrix_storage_free;

    *out = storage;
    return RC_OK;
}

#endif // MAT_SHARED_H
