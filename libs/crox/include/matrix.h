
#ifndef MAT_H
#define MAT_H

#include "common_types.h"
#include <stdbool.h>
#include <stdint.h>

typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

/**
 * @brief Opaque handle to a matrix.
 *
 * A matrix stores fixed-size elements in row-major order. The element size is
 * fixed when the matrix is created; every read and write must use an accessor
 * whose value type is no larger than that element size. All coordinates are
 * 0-based, with (0, 0) at the top-left cell.
 */
typedef void *matrix;

/**
 * @brief Get the number of rows in a matrix.
 *
 * @param[in] mat Matrix handle, may be NULL.
 * @return Row count, or 0 if @p mat is NULL.
 */
i32 matrix_get_height(matrix mat);

/**
 * @brief Get the number of columns in a matrix.
 *
 * @param[in] mat Matrix handle, may be NULL.
 * @return Column count, or 0 if @p mat is NULL.
 */
i32 matrix_get_width(matrix mat);

/**
 * @brief Resize a matrix.
 *
 * Updates the reported dimensions and re-establishes the backing store. How
 * the store is managed depends on the implementation the matrix was created
 * with: a greedy matrix (matrix_init_greedy()) only grows its allocation and
 * reuses it when possible, while a standard matrix (matrix_init()) frees the
 * old store and allocates a fresh one on every resize. In either
 * case the cell contents after a resize are unspecified; callers must not
 * assume any value is preserved.
 *
 * @param[in] mat    Matrix handle, may be NULL.
 * @param[in] height New number of rows; must be greater than 0.
 * @param[in] width  New number of columns; must be greater than 0.
 * @retval RC_OK              Resize succeeded.
 * @retval RC_NOT_INITIALIZED @p mat is NULL.
 * @retval RC_BAD_INPUT_ARG   @p height or @p width is not positive, or the
 *                            resulting size would overflow.
 * @retval RC_MEM_ALLOC_ERROR Allocation failed; the matrix is left unchanged.
 */
Rc matrix_resize(matrix mat, i32 height, i32 width);

/**
 * @brief Free a matrix and its backing store.
 *
 * Safe to call with NULL. Calling this twice on the same handle is undefined.
 *
 * @param[in] mat Matrix handle, may be NULL.
 */
void matrix_free(matrix mat);

/**
 * @brief Get a pointer to a cell's raw bytes.
 *
 * Returns a pointer into the matrix's backing store at the start of cell
 * (x, y). The pointer is not typed: cast it to the element type the matrix was
 * created with, e.g. a matrix created with sizeof(MyStruct) yields a
 * MyStruct *. This makes it possible to store and mutate arbitrary cell types
 * in place without a copy.
 *
 * The pointer aliases the backing store, so it is invalidated by any
 * matrix_resize() that reallocates. Writes through it must respect the element
 * type the matrix was created with.
 *
 * @param[in] mat Matrix handle, may be NULL.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return Pointer to the cell's bytes, or NULL if @p mat is NULL or the
 *         coordinates are out of bounds.
 */
void *matrix_get_cell(matrix mat, i32 x, i32 y);

/**
 * @brief Typed cell accessors.
 *
 * The typedefs below alias the base matrix handle for readable signatures;
 * every accessor accepts a matrix created with any element size. A getter
 * returns a zero/false/NULL sentinel when @p mat is NULL, the coordinates are
 * out of bounds, or the cell is smaller than the value type. A setter returns
 * RC_NOT_INITIALIZED for a NULL handle, RC_BAD_INPUT_ARG when the coordinates
 * are out of bounds or the value type is larger than the cell, and RC_OK on
 * success. Coordinates are 0-based with (0, 0) at the top-left.
 */

typedef matrix matrix_i8;
typedef matrix matrix_u8;
typedef matrix matrix_i16;
typedef matrix matrix_u16;
typedef matrix matrix_i32;
typedef matrix matrix_u32;
typedef matrix matrix_i64;
typedef matrix matrix_u64;
typedef matrix matrix_f32;
typedef matrix matrix_f64;
typedef matrix matrix_bool;
typedef matrix matrix_ptr;

/** @brief Read an 8-bit signed cell. */
i8 matrix_get_i8(matrix_i8 mat, i32 x, i32 y);
/** @brief Write an 8-bit signed cell. */
Rc matrix_set_i8(matrix_i8 mat, i32 x, i32 y, i8 v);

/** @brief Read an 8-bit unsigned cell. */
u8 matrix_get_u8(matrix_u8 mat, i32 x, i32 y);
/** @brief Write an 8-bit unsigned cell. */
Rc matrix_set_u8(matrix_u8 mat, i32 x, i32 y, u8 v);

/** @brief Read a 16-bit signed cell. */
i16 matrix_get_i16(matrix_i16 mat, i32 x, i32 y);
/** @brief Write a 16-bit signed cell. */
Rc matrix_set_i16(matrix_i16 mat, i32 x, i32 y, i16 v);

/** @brief Read a 16-bit unsigned cell. */
u16 matrix_get_u16(matrix_u16 mat, i32 x, i32 y);
/** @brief Write a 16-bit unsigned cell. */
Rc matrix_set_u16(matrix_u16 mat, i32 x, i32 y, u16 v);

/** @brief Read a 32-bit signed cell. */
i32 matrix_get_i32(matrix_i32 mat, i32 x, i32 y);
/** @brief Write a 32-bit signed cell. */
Rc matrix_set_i32(matrix_i32 mat, i32 x, i32 y, i32 v);

/** @brief Read a 32-bit unsigned cell. */
u32 matrix_get_u32(matrix_u32 mat, i32 x, i32 y);
/** @brief Write a 32-bit unsigned cell. */
Rc matrix_set_u32(matrix_u32 mat, i32 x, i32 y, u32 v);

/** @brief Read a 64-bit signed cell. */
i64 matrix_get_i64(matrix_i64 mat, i32 x, i32 y);
/** @brief Write a 64-bit signed cell. */
Rc matrix_set_i64(matrix_i64 mat, i32 x, i32 y, i64 v);

/** @brief Read a 64-bit unsigned cell. */
u64 matrix_get_u64(matrix_u64 mat, i32 x, i32 y);
/** @brief Write a 64-bit unsigned cell. */
Rc matrix_set_u64(matrix_u64 mat, i32 x, i32 y, u64 v);

/** @brief Read a 32-bit float cell. */
f32 matrix_get_f32(matrix_f32 mat, i32 x, i32 y);
/** @brief Write a 32-bit float cell. */
Rc matrix_set_f32(matrix_f32 mat, i32 x, i32 y, f32 v);

/** @brief Read a 64-bit double cell. */
f64 matrix_get_f64(matrix_f64 mat, i32 x, i32 y);
/** @brief Write a 64-bit double cell. */
Rc matrix_set_f64(matrix_f64 mat, i32 x, i32 y, f64 v);

/** @brief Read a boolean cell. */
bool matrix_get_bool(matrix_bool mat, i32 x, i32 y);
/** @brief Write a boolean cell. */
Rc matrix_set_bool(matrix_bool mat, i32 x, i32 y, bool v);

/** @brief Read a pointer cell. */
void *matrix_get_ptr(matrix_ptr mat, i32 x, i32 y);
/** @brief Write a pointer cell. */
Rc matrix_set_ptr(matrix_ptr mat, i32 x, i32 y, const void *v);

#endif // MAT_H
