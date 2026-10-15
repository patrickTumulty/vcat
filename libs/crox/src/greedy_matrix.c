
#include "greedy_matrix.h"
#include "matrix_shared.h"
#include <stdint.h>
#include <stdlib.h>

/* Greedy resize: keep the existing allocation and only grow it when the new
 * size exceeds the current capacity. */
static Rc greedy_matrix_resize(matrix mat, int32_t height, int32_t width)
{
    matrix_storage *storage = (matrix_storage *)mat;
    if (height <= 0 || width <= 0)
        return RC_BAD_INPUT_ARG;

    size_t rows = (size_t)height;
    size_t cols = (size_t)width;
    if (matrix_mul_overflows(rows, cols))
        return RC_BAD_INPUT_ARG;
    size_t cells = rows * cols;
    if (matrix_mul_overflows(cells, storage->type_size))
        return RC_BAD_INPUT_ARG;
    size_t required_bytes = cells * storage->type_size;

    if (storage->allocated_bytes < required_bytes)
    {
        /* Allocate before freeing so a failed allocation leaves the matrix
         * untouched rather than pointing at freed memory. */
        uint8_t *new_data = (uint8_t *)calloc(1, required_bytes);
        if (new_data == NULL)
            return RC_MEM_ALLOC_ERROR;
        free(storage->data);
        storage->data = new_data;
        storage->allocated_bytes = required_bytes;
    }

    storage->height = height;
    storage->width = width;
    return RC_OK;
}

Rc matrix_init_greedy(matrix *out, int32_t height, int32_t width, size_t typeSize)
{
    return matrix_storage_init(out, height, width, typeSize, greedy_matrix_resize);
}

/* Emit a typed wrapper that creates a greedy matrix whose cells are one
 * `type`, forwarding to matrix_init_greedy() with sizeof(type). */
#define DEFINE_GREEDY_INIT(suffix, type)                                       \
    Rc matrix_init_greedy_##suffix(matrix *out, int32_t height, int32_t width) \
    {                                                                          \
        return matrix_init_greedy(out, height, width, sizeof(type));           \
    }

DEFINE_GREEDY_INIT(i8, int8_t)
DEFINE_GREEDY_INIT(u8, uint8_t)
DEFINE_GREEDY_INIT(i16, int16_t)
DEFINE_GREEDY_INIT(u16, uint16_t)
DEFINE_GREEDY_INIT(i32, int32_t)
DEFINE_GREEDY_INIT(u32, uint32_t)
DEFINE_GREEDY_INIT(i64, int64_t)
DEFINE_GREEDY_INIT(u64, uint64_t)
DEFINE_GREEDY_INIT(f32, float)
DEFINE_GREEDY_INIT(f64, double)
DEFINE_GREEDY_INIT(bool, bool)
DEFINE_GREEDY_INIT(ptr, void *)
