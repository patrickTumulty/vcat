
#ifndef MAT_SHARED_H
#define MAT_SHARED_H

#include "common_types.h"
#include "mat.h"
#include <stddef.h>
#include <stdint.h>

/* Internal vtable shared between the mat facade and its implementations. Not
 * part of the public API: callers use mat.h / greedy_mat.h instead. */

/** @brief Read @p size bytes from cell (x, y) into @p out. */
typedef Rc (*MatAPIGet)(Mat mat, int32_t x, int32_t y, void *out, size_t size);

/** @brief Write @p size bytes from @p v into cell (x, y). */
typedef Rc (*MatAPISet)(Mat mat, int32_t x, int32_t y, const void *v, size_t size);

/** @brief Return the column count. */
typedef int32_t (*MatAPIGetWidth)(Mat mat);

/** @brief Return the row count. */
typedef int32_t (*MatAPIGetHeight)(Mat mat);

/** @brief Resize the matrix to @p height rows by @p width columns. */
typedef Rc (*MatAPIResize)(Mat mat, int32_t height, int32_t width);

/** @brief Release the matrix and its backing store. */
typedef void (*MatAPIFree)(Mat mat);

/**
 * @brief Function table placed at the start of every concrete matrix so that
 *        the Mat handle can be dispatched without knowing its type.
 */
typedef struct
{
    MatAPIGet get;
    MatAPISet set;
    MatAPIGetWidth getWidth;
    MatAPIGetHeight getHeight;
    MatAPIResize resize;
    MatAPIFree free;
} MatInternalHeader;

#endif // MAT_SHARED_H
