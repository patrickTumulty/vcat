
#ifndef MAT_H
#define MAT_H

#include "common_types.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Opaque handle to a matrix.
 *
 * A matrix stores fixed-size elements in row-major order. The element size is
 * fixed when the matrix is created; every read and write must use an accessor
 * whose value type is no larger than that element size. All coordinates are
 * 0-based, with (0, 0) at the top-left cell.
 */
typedef void *Mat;

/**
 * @brief Get the number of rows in a matrix.
 *
 * @param[in] mat Matrix handle, may be NULL.
 * @return Row count, or 0 if @p mat is NULL.
 */
int32_t matGetHeight(Mat mat);

/**
 * @brief Get the number of columns in a matrix.
 *
 * @param[in] mat Matrix handle, may be NULL.
 * @return Column count, or 0 if @p mat is NULL.
 */
int32_t matGetWidth(Mat mat);

/**
 * @brief Resize a matrix.
 *
 * Matrices are greedy: the backing store only ever grows, so shrinking updates
 * the reported dimensions while keeping the existing allocation, and a later
 * grow back within that capacity does not allocate. The contents of the cells
 * after a resize are unspecified; callers must not assume any value is
 * preserved or zeroed.
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
Rc matResize(Mat mat, int32_t height, int32_t width);

/**
 * @brief Free a matrix and its backing store.
 *
 * Safe to call with NULL. Calling this twice on the same handle is undefined.
 *
 * @param[in] mat Matrix handle, may be NULL.
 */
void matFree(Mat mat);

/**
 * @brief Read a signed 8-bit value from a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return The stored value, or 0 if @p mat is NULL, the coordinates are out of
 *         bounds, or the element size is smaller than the value type.
 */
int8_t matGetB(Mat mat, int32_t x, int32_t y);

/**
 * @brief Read an unsigned 8-bit value from a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return The stored value, or 0 on failure (see matGetB()).
 */
uint8_t matGetUB(Mat mat, int32_t x, int32_t y);

/**
 * @brief Read a signed 16-bit value from a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return The stored value, or 0 on failure (see matGetB()).
 */
int16_t matGetS(Mat mat, int32_t x, int32_t y);

/**
 * @brief Read an unsigned 16-bit value from a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return The stored value, or 0 on failure (see matGetB()).
 */
uint16_t matGetUS(Mat mat, int32_t x, int32_t y);

/**
 * @brief Read a signed 32-bit value from a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return The stored value, or 0 on failure (see matGetB()).
 */
int32_t matGetI(Mat mat, int32_t x, int32_t y);

/**
 * @brief Read an unsigned 32-bit value from a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return The stored value, or 0 on failure (see matGetB()).
 */
uint32_t matGetUI(Mat mat, int32_t x, int32_t y);

/**
 * @brief Read a signed 64-bit value from a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return The stored value, or 0 on failure (see matGetB()).
 */
int64_t matGetL(Mat mat, int32_t x, int32_t y);

/**
 * @brief Read an unsigned 64-bit value from a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return The stored value, or 0 on failure (see matGetB()).
 */
uint64_t matGetUL(Mat mat, int32_t x, int32_t y);

/**
 * @brief Read a 32-bit float from a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return The stored value, or 0 on failure (see matGetB()).
 */
float matGetF(Mat mat, int32_t x, int32_t y);

/**
 * @brief Read a 64-bit double from a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return The stored value, or 0 on failure (see matGetB()).
 */
double matGetD(Mat mat, int32_t x, int32_t y);

/**
 * @brief Read a boolean value from a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return The stored value, or false on failure (see matGetB()).
 */
bool matGetBool(Mat mat, int32_t x, int32_t y);

/**
 * @brief Read a pointer value from a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @return The stored pointer, or NULL on failure (see matGetB()).
 */
void *matGetPtr(Mat mat, int32_t x, int32_t y);

/**
 * @brief Type-generic cell reader.
 *
 * Selects the typed getter that matches @p T. @p T is a type token, not a
 * value, e.g. `matGet(mat, x, y, int32_t)`. Pointer values are not covered by
 * this macro; use matGetPtr() directly.
 *
 * @param[in] MAT Matrix handle.
 * @param[in] X   Column index.
 * @param[in] Y   Row index.
 * @param[in] T   Element type token (int8_t, uint16_t, int32_t, float, ...).
 * @return The cell value, or the typed getter's sentinel value on failure.
 */
#define matGet(MAT, X, Y, T)                 \
    _Generic((T)0,                           \
        int8_t: matGetB((MAT), (X), (Y)),    \
        uint8_t: matGetUB((MAT), (X), (Y)),  \
        int16_t: matGetS((MAT), (X), (Y)),   \
        uint16_t: matGetUS((MAT), (X), (Y)), \
        int32_t: matGetI((MAT), (X), (Y)),   \
        uint32_t: matGetUI((MAT), (X), (Y)), \
        int64_t: matGetL((MAT), (X), (Y)),   \
        uint64_t: matGetUL((MAT), (X), (Y)), \
        float: matGetF((MAT), (X), (Y)),     \
        double: matGetD((MAT), (X), (Y)),    \
        bool: matGetBool((MAT), (X), (Y)))

/**
 * @brief Write a signed 8-bit value to a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @param[in] v   Value to store.
 * @retval RC_OK              Value stored.
 * @retval RC_NOT_INITIALIZED @p mat is NULL.
 * @retval RC_BAD_INPUT_ARG   Coordinates out of bounds, or the value type is
 *                            larger than the matrix element size.
 */
Rc matSetB(Mat mat, int32_t x, int32_t y, int8_t v);

/**
 * @brief Write an unsigned 8-bit value to a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @param[in] v   Value to store.
 * @return See matSetB().
 */
Rc matSetUB(Mat mat, int32_t x, int32_t y, uint8_t v);

/**
 * @brief Write a signed 16-bit value to a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @param[in] v   Value to store.
 * @return See matSetB().
 */
Rc matSetS(Mat mat, int32_t x, int32_t y, int16_t v);

/**
 * @brief Write an unsigned 16-bit value to a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @param[in] v   Value to store.
 * @return See matSetB().
 */
Rc matSetUS(Mat mat, int32_t x, int32_t y, uint16_t v);

/**
 * @brief Write a signed 32-bit value to a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @param[in] v   Value to store.
 * @return See matSetB().
 */
Rc matSetI(Mat mat, int32_t x, int32_t y, int32_t v);

/**
 * @brief Write an unsigned 32-bit value to a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @param[in] v   Value to store.
 * @return See matSetB().
 */
Rc matSetUI(Mat mat, int32_t x, int32_t y, uint32_t v);

/**
 * @brief Write a signed 64-bit value to a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @param[in] v   Value to store.
 * @return See matSetB().
 */
Rc matSetL(Mat mat, int32_t x, int32_t y, int64_t v);

/**
 * @brief Write an unsigned 64-bit value to a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @param[in] v   Value to store.
 * @return See matSetB().
 */
Rc matSetUL(Mat mat, int32_t x, int32_t y, uint64_t v);

/**
 * @brief Write a 32-bit float to a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @param[in] v   Value to store.
 * @return See matSetB().
 */
Rc matSetF(Mat mat, int32_t x, int32_t y, float v);

/**
 * @brief Write a 64-bit double to a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @param[in] v   Value to store.
 * @return See matSetB().
 */
Rc matSetD(Mat mat, int32_t x, int32_t y, double v);

/**
 * @brief Write a boolean value to a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @param[in] v   Value to store.
 * @return See matSetB().
 */
Rc matSetBool(Mat mat, int32_t x, int32_t y, bool v);

/**
 * @brief Write a pointer value to a cell.
 * @param[in] mat Matrix handle.
 * @param[in] x   Column index.
 * @param[in] y   Row index.
 * @param[in] v   Pointer to store.
 * @return See matSetB().
 */
Rc matSetPtr(Mat mat, int32_t x, int32_t y, const void *v);

/**
 * @brief Type-generic cell writer.
 *
 * Selects the typed setter that matches @p V. Pointer values are not covered
 * by this macro; use matSetPtr() directly.
 *
 * @param[in] MAT Matrix handle.
 * @param[in] X   Column index.
 * @param[in] Y   Row index.
 * @param[in] V   Value to store; its type selects the setter.
 * @return See matSetB().
 */
#define matSet(MAT, X, Y, V)                                \
    _Generic((V),                                           \
        int8_t: matSetB((MAT), (X), (Y), (int8_t)(V)),      \
        uint8_t: matSetUB((MAT), (X), (Y), (uint8_t)(V)),   \
        int16_t: matSetS((MAT), (X), (Y), (int16_t)(V)),    \
        uint16_t: matSetUS((MAT), (X), (Y), (uint16_t)(V)), \
        int32_t: matSetI((MAT), (X), (Y), (int32_t)(V)),    \
        uint32_t: matSetUI((MAT), (X), (Y), (uint32_t)(V)), \
        int64_t: matSetL((MAT), (X), (Y), (int64_t)(V)),    \
        uint64_t: matSetUL((MAT), (X), (Y), (uint64_t)(V)), \
        float: matSetF((MAT), (X), (Y), (float)(V)),        \
        double: matSetD((MAT), (X), (Y), (double)(V)),      \
        bool: matSetBool((MAT), (X), (Y), (bool)(V)))

#endif // MAT_H
