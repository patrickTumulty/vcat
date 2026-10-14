
#ifndef GREEDY_MAT_H
#define GREEDY_MAT_H

#include "common_types.h"
#include "mat.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Create a greedy matrix.
 *
 * Allocates a row-major matrix of @p height rows and @p width columns whose
 * cells are @p typeSize bytes each. The backing store only ever grows on
 * subsequent matResize() calls. On success @p out receives the new handle.
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
Rc matInitGreedyMat(Mat *out, int32_t height, int32_t width, size_t typeSize);

#endif // GREEDY_MAT_H
