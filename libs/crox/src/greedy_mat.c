
#include "greedy_mat.h"
#include "mat_shared.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    MatInternalHeader header;
    uint8_t *data;
    size_t allocatedBytes;
    int32_t width;
    int32_t height;
    size_t typeSize;
} GreedyMatInternal;

static bool mulOverflows(size_t a, size_t b)
{
    return b != 0 && a > SIZE_MAX / b;
}

static Rc greedyMatGet(Mat mat, int32_t x, int32_t y, void *out, size_t size)
{
    GreedyMatInternal *internal = (GreedyMatInternal *)mat;
    if (x < 0 || y < 0 || x >= internal->width || y >= internal->height)
        return RC_BAD_INPUT_ARG;
    if (size > internal->typeSize)
        return RC_BAD_INPUT_ARG;

    size_t index = ((size_t)y * (size_t)internal->width + (size_t)x) * internal->typeSize;
    memcpy(out, internal->data + index, size);
    return RC_OK;
}

static Rc greedyMatSet(Mat mat, int32_t x, int32_t y, const void *v, size_t size)
{
    GreedyMatInternal *internal = (GreedyMatInternal *)mat;
    if (x < 0 || y < 0 || x >= internal->width || y >= internal->height)
        return RC_BAD_INPUT_ARG;
    if (size > internal->typeSize)
        return RC_BAD_INPUT_ARG;

    size_t index = ((size_t)y * (size_t)internal->width + (size_t)x) * internal->typeSize;
    memcpy(internal->data + index, v, size);
    return RC_OK;
}

static int32_t greedyMatGetWidth(Mat mat)
{
    return ((GreedyMatInternal *)mat)->width;
}

static int32_t greedyMatGetHeight(Mat mat)
{
    return ((GreedyMatInternal *)mat)->height;
}

static Rc greedyMatResize(Mat mat, int32_t height, int32_t width)
{
    GreedyMatInternal *internal = (GreedyMatInternal *)mat;
    if (height <= 0 || width <= 0)
        return RC_BAD_INPUT_ARG;

    size_t rows = (size_t)height;
    size_t cols = (size_t)width;
    if (mulOverflows(rows, cols))
        return RC_BAD_INPUT_ARG;
    size_t cells = rows * cols;
    if (mulOverflows(cells, internal->typeSize))
        return RC_BAD_INPUT_ARG;
    size_t requiredBytes = cells * internal->typeSize;

    if (internal->allocatedBytes < requiredBytes)
    {
        /* Allocate before freeing so a failed allocation leaves the matrix
         * untouched rather than pointing at freed memory. */
        uint8_t *newData = (uint8_t *)calloc(1, requiredBytes);
        if (newData == NULL)
            return RC_MEM_ALLOC_ERROR;
        free(internal->data);
        internal->data = newData;
        internal->allocatedBytes = requiredBytes;
    }

    internal->height = height;
    internal->width = width;
    return RC_OK;
}

static void greedyMatFree(Mat mat)
{
    GreedyMatInternal *internal = (GreedyMatInternal *)mat;
    free(internal->data);
    free(internal);
}

Rc matInitGreedyMat(Mat *out, int32_t height, int32_t width, size_t typeSize)
{
    if (out == NULL)
        return RC_BAD_INPUT_ARG;
    *out = NULL;

    if (height <= 0 || width <= 0 || typeSize == 0)
        return RC_BAD_INPUT_ARG;

    size_t rows = (size_t)height;
    size_t cols = (size_t)width;
    if (mulOverflows(rows, cols))
        return RC_BAD_INPUT_ARG;
    size_t cells = rows * cols;
    if (mulOverflows(cells, typeSize))
        return RC_BAD_INPUT_ARG;
    size_t totalBytes = cells * typeSize;

    GreedyMatInternal *internal = (GreedyMatInternal *)calloc(1, sizeof(GreedyMatInternal));
    if (internal == NULL)
        return RC_MEM_ALLOC_ERROR;

    internal->data = (uint8_t *)calloc(1, totalBytes);
    if (internal->data == NULL)
    {
        free(internal);
        return RC_MEM_ALLOC_ERROR;
    }

    internal->height = height;
    internal->width = width;
    internal->allocatedBytes = totalBytes;
    internal->typeSize = typeSize;

    internal->header.get = greedyMatGet;
    internal->header.set = greedyMatSet;
    internal->header.getWidth = greedyMatGetWidth;
    internal->header.getHeight = greedyMatGetHeight;
    internal->header.resize = greedyMatResize;
    internal->header.free = greedyMatFree;

    *out = internal;
    return RC_OK;
}
