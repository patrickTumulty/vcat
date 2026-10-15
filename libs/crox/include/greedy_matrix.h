
#ifndef GREEDY_MAT_H
#define GREEDY_MAT_H

#include "common_types.h"
#include "matrix.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Create a greedy matrix.
 *
 * Allocates a row-major matrix of @p height rows and @p width columns whose
 * cells are @p typeSize bytes each. The backing store only ever grows on
 * subsequent matrix_resize() calls. Use this entry point to store an arbitrary
 * cell type; the typed matrix_init_greedy_*() helpers below cover the built-in
 * variants. On success @p out receives the new handle.
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
Rc matrix_init_greedy(matrix *out, int32_t height, int32_t width, size_t typeSize);

/**
 * @brief Create a greedy matrix of a built-in variant.
 *
 * Each helper is a thin wrapper over matrix_init_greedy() that supplies the
 * element size of its variant, e.g. matrix_init_greedy_i32() uses
 * sizeof(int32_t). They share the same validation and return codes.
 *
 * @param[out] out    Receives the matrix handle; set to NULL on failure.
 * @param[in]  height Number of rows; must be greater than 0.
 * @param[in]  width  Number of columns; must be greater than 0.
 * @retval RC_OK              Matrix created.
 * @retval RC_BAD_INPUT_ARG   @p out is NULL or a dimension is not positive.
 * @retval RC_MEM_ALLOC_ERROR Allocation failed.
 */
Rc matrix_init_greedy_i8(matrix *out, int32_t height, int32_t width);
Rc matrix_init_greedy_u8(matrix *out, int32_t height, int32_t width);
Rc matrix_init_greedy_i16(matrix *out, int32_t height, int32_t width);
Rc matrix_init_greedy_u16(matrix *out, int32_t height, int32_t width);
Rc matrix_init_greedy_i32(matrix *out, int32_t height, int32_t width);
Rc matrix_init_greedy_u32(matrix *out, int32_t height, int32_t width);
Rc matrix_init_greedy_i64(matrix *out, int32_t height, int32_t width);
Rc matrix_init_greedy_u64(matrix *out, int32_t height, int32_t width);
Rc matrix_init_greedy_f32(matrix *out, int32_t height, int32_t width);
Rc matrix_init_greedy_f64(matrix *out, int32_t height, int32_t width);
Rc matrix_init_greedy_bool(matrix *out, int32_t height, int32_t width);
Rc matrix_init_greedy_ptr(matrix *out, int32_t height, int32_t width);

#endif // GREEDY_MAT_H
