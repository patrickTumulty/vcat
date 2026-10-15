
#include "matrix.h"
#include "matrix_shared.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Get the number of rows in a matrix (0 if @p mat is NULL).
 *
 * @param[in] mat Matrix handle, may be NULL.
 * @return Row count.
 */
int32_t matrix_get_height(matrix mat)
{
    if (mat == NULL)
        return 0;
    matrix_internal_hdr *internal = (matrix_internal_hdr *)mat;
    return internal->get_height(mat);
}

/**
 * @brief Get the number of columns in a matrix (0 if @p mat is NULL).
 *
 * @param[in] mat Matrix handle, may be NULL.
 * @return Column count.
 */
int32_t matrix_get_width(matrix mat)
{
    if (mat == NULL)
        return 0;
    matrix_internal_hdr *internal = (matrix_internal_hdr *)mat;
    return internal->get_width(mat);
}

/**
 * @brief Resize a matrix, dispatching to its concrete implementation.
 *
 * @param[in] mat    Matrix handle, may be NULL.
 * @param[in] height New number of rows.
 * @param[in] width  New number of columns.
 * @retval RC_NOT_INITIALIZED @p mat is NULL.
 * @retval RC_OK              Resized.
 * @retval RC_BAD_INPUT_ARG   A dimension is not positive or the size overflows.
 * @retval RC_MEM_ALLOC_ERROR Allocation failed; the matrix is left unchanged.
 */
Rc matrix_resize(matrix mat, int32_t height, int32_t width)
{
    if (mat == NULL)
        return RC_NOT_INITIALIZED;
    matrix_internal_hdr *internal = (matrix_internal_hdr *)mat;
    return internal->resize(mat, height, width);
}

/**
 * @brief Free a matrix and its backing store; safe with NULL.
 *
 * @param[in] mat Matrix handle, may be NULL.
 */
void matrix_free(matrix mat)
{
    if (mat == NULL)
        return;
    matrix_internal_hdr *internal = (matrix_internal_hdr *)mat;
    internal->free(mat);
}

/* Dispatch a typed read to the concrete implementation. */
static Rc matrix_get_internal(matrix mat, int32_t x, int32_t y, void *out, size_t size)
{
    if (mat == NULL)
        return RC_NOT_INITIALIZED;
    matrix_internal_hdr *internal = (matrix_internal_hdr *)mat;
    return internal->get(mat, x, y, out, size);
}

/* Dispatch a typed write to the concrete implementation. */
static Rc matrix_set_internal(matrix mat, int32_t x, int32_t y, const void *v, size_t size)
{
    if (mat == NULL)
        return RC_NOT_INITIALIZED;
    matrix_internal_hdr *internal = (matrix_internal_hdr *)mat;
    return internal->set(mat, x, y, v, size);
}

void *matrix_get_cell(matrix mat, int32_t x, int32_t y)
{
    if (mat == NULL)
        return NULL;
    matrix_internal_hdr *internal = (matrix_internal_hdr *)mat;
    return internal->get_cell(mat, x, y);
}

/* Typed accessors. Each getter returns the value type's zero/false/NULL
 * sentinel on failure; each setter forwards to matrix_set_internal(), whose
 * status code it returns unchanged. See matrix.h for the full contract. */

i8 matrix_get_i8(matrix_i8 mat, i32 x, i32 y)
{
    i8 v = 0;
    matrix_get_internal(mat, x, y, &v, sizeof(v));
    return v;
}

u8 matrix_get_u8(matrix_u8 mat, i32 x, i32 y)
{
    u8 v = 0;
    matrix_get_internal(mat, x, y, &v, sizeof(v));
    return v;
}

i16 matrix_get_i16(matrix_i16 mat, i32 x, i32 y)
{
    i16 v = 0;
    matrix_get_internal(mat, x, y, &v, sizeof(v));
    return v;
}

u16 matrix_get_u16(matrix_u16 mat, i32 x, i32 y)
{
    u16 v = 0;
    matrix_get_internal(mat, x, y, &v, sizeof(v));
    return v;
}

i32 matrix_get_i32(matrix_i32 mat, i32 x, i32 y)
{
    i32 v = 0;
    matrix_get_internal(mat, x, y, &v, sizeof(v));
    return v;
}

u32 matrix_get_u32(matrix_u32 mat, i32 x, i32 y)
{
    u32 v = 0;
    matrix_get_internal(mat, x, y, &v, sizeof(v));
    return v;
}

i64 matrix_get_i64(matrix_i64 mat, i32 x, i32 y)
{
    i64 v = 0;
    matrix_get_internal(mat, x, y, &v, sizeof(v));
    return v;
}

u64 matrix_get_u64(matrix_u64 mat, i32 x, i32 y)
{
    u64 v = 0;
    matrix_get_internal(mat, x, y, &v, sizeof(v));
    return v;
}

f32 matrix_get_f32(matrix_f32 mat, i32 x, i32 y)
{
    f32 v = 0;
    matrix_get_internal(mat, x, y, &v, sizeof(v));
    return v;
}

f64 matrix_get_f64(matrix_f64 mat, i32 x, i32 y)
{
    f64 v = 0;
    matrix_get_internal(mat, x, y, &v, sizeof(v));
    return v;
}

bool matrix_get_bool(matrix_bool mat, i32 x, i32 y)
{
    bool v = false;
    matrix_get_internal(mat, x, y, &v, sizeof(v));
    return v;
}

void *matrix_get_ptr(matrix_ptr mat, i32 x, i32 y)
{
    void *v = NULL;
    matrix_get_internal(mat, x, y, &v, sizeof(v));
    return v;
}

Rc matrix_set_i8(matrix_i8 mat, i32 x, i32 y, i8 v)
{
    return matrix_set_internal(mat, x, y, &v, sizeof(v));
}

Rc matrix_set_u8(matrix_u8 mat, i32 x, i32 y, u8 v)
{
    return matrix_set_internal(mat, x, y, &v, sizeof(v));
}

Rc matrix_set_i16(matrix_i16 mat, i32 x, i32 y, i16 v)
{
    return matrix_set_internal(mat, x, y, &v, sizeof(v));
}

Rc matrix_set_u16(matrix_u16 mat, i32 x, i32 y, u16 v)
{
    return matrix_set_internal(mat, x, y, &v, sizeof(v));
}

Rc matrix_set_i32(matrix_i32 mat, i32 x, i32 y, i32 v)
{
    return matrix_set_internal(mat, x, y, &v, sizeof(v));
}

Rc matrix_set_u32(matrix_u32 mat, i32 x, i32 y, u32 v)
{
    return matrix_set_internal(mat, x, y, &v, sizeof(v));
}

Rc matrix_set_i64(matrix_i64 mat, i32 x, i32 y, i64 v)
{
    return matrix_set_internal(mat, x, y, &v, sizeof(v));
}

Rc matrix_set_u64(matrix_u64 mat, i32 x, i32 y, u64 v)
{
    return matrix_set_internal(mat, x, y, &v, sizeof(v));
}

Rc matrix_set_f32(matrix_f32 mat, i32 x, i32 y, f32 v)
{
    return matrix_set_internal(mat, x, y, &v, sizeof(v));
}

Rc matrix_set_f64(matrix_f64 mat, i32 x, i32 y, f64 v)
{
    return matrix_set_internal(mat, x, y, &v, sizeof(v));
}

Rc matrix_set_bool(matrix_bool mat, i32 x, i32 y, bool v)
{
    return matrix_set_internal(mat, x, y, &v, sizeof(v));
}

Rc matrix_set_ptr(matrix_ptr mat, i32 x, i32 y, const void *v)
{
    return matrix_set_internal(mat, x, y, &v, sizeof(v));
}
