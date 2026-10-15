
#ifndef STANDARD_MAT_H
#define STANDARD_MAT_H

#include "common_types.h"
#include "matrix.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Create a standard (non-greedy) matrix.
 *
 * Allocates a row-major matrix of @p height rows and @p width columns whose
 * cells are @p typeSize bytes each. Unlike a greedy matrix, the backing store
 * is not retained across resizes: every successful matrix_resize() frees the
 * old store and allocates a fresh one sized exactly to the new dimensions.
 * Use this entry point to store an arbitrary cell type; the typed
 * matrix_init_*() helpers below cover the built-in variants. On success @p out
 * receives the new handle.
 *
 * @param[out] out      Receives the matrix handle; set to NULL on failure.
 *                      Must not be NULL.
 * @param[in]  height   Number of rows; must be greater than 0.
 * @param[in]  width    Number of columns; must be greater than 0.
 * @param[in]  typeSize Size of one cell in bytes; must be greater than 0.
 * @retval RC_OK              Matrix created.
 * @retval RC_BAD_INPUT_ARG   @p out is NULL, @p height or @p width is not
 *                            positive, @p typeSize is 0, or the total size
 *                            would overflow.
 * @retval RC_MEM_ALLOC_ERROR Allocation failed.
 */
Rc matrix_init(matrix *out, int32_t height, int32_t width, size_t typeSize);

/**
 * @brief Create a standard matrix of a built-in variant.
 *
 * Each helper is a thin wrapper over matrix_init() that supplies the element
 * size of its variant, e.g. matrix_init_i32() uses sizeof(int32_t). They share
 * the same validation and return codes, and the resulting matrix has the same
 * non-greedy resize behavior.
 *
 * @param[out] out    Receives the matrix handle; set to NULL on failure.
 * @param[in]  height Number of rows; must be greater than 0.
 * @param[in]  width  Number of columns; must be greater than 0.
 * @retval RC_OK              Matrix created.
 * @retval RC_BAD_INPUT_ARG   @p out is NULL or a dimension is not positive.
 * @retval RC_MEM_ALLOC_ERROR Allocation failed.
 */
Rc matrix_init_i8(matrix *out, int32_t height, int32_t width);
Rc matrix_init_u8(matrix *out, int32_t height, int32_t width);
Rc matrix_init_i16(matrix *out, int32_t height, int32_t width);
Rc matrix_init_u16(matrix *out, int32_t height, int32_t width);
Rc matrix_init_i32(matrix *out, int32_t height, int32_t width);
Rc matrix_init_u32(matrix *out, int32_t height, int32_t width);
Rc matrix_init_i64(matrix *out, int32_t height, int32_t width);
Rc matrix_init_u64(matrix *out, int32_t height, int32_t width);
Rc matrix_init_f32(matrix *out, int32_t height, int32_t width);
Rc matrix_init_f64(matrix *out, int32_t height, int32_t width);
Rc matrix_init_bool(matrix *out, int32_t height, int32_t width);
Rc matrix_init_ptr(matrix *out, int32_t height, int32_t width);

#endif // STANDARD_MAT_H
