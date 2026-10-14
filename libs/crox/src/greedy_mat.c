
#include "greedy_mat.h"
#include "mat_shared.h"
#include <stdlib.h>
#include <string.h>

typedef struct
{
    MatInternalHeader header;
    uint8_t *data;
    int allocatedBytes;
    int width;
    int height;
    int typeSize;
} GreedyMatInternal;

static uint8_t *greedyMatGet(Mat mat, int32_t x, int32_t y)
{
    GreedyMatInternal *internal = (GreedyMatInternal *)mat;
    int index = (y * internal->width + x) * internal->typeSize;
    return internal->data + index;
}

static void greedyMatSet(Mat mat, int32_t x, int32_t y, uint8_t *v, size_t vSize)
{
    GreedyMatInternal *internal = (GreedyMatInternal *)mat;
    int index = (y * internal->width + x) * internal->typeSize;
    memcpy(internal->data + index, v, vSize);
}

static int32_t greedyMatGetWidth(Mat mat)
{
    return ((GreedyMatInternal *)mat)->width;
}

static int32_t greedyMatGetHeight(Mat mat)
{
    return ((GreedyMatInternal *)mat)->height;
}

static void greedyMatResize(Mat mat, int32_t height, int32_t width)
{
    GreedyMatInternal *internal = (GreedyMatInternal *)mat;
    int requiredBytes = (height * width) * internal->typeSize;
    if (internal->allocatedBytes < requiredBytes)
    {
        free(internal->data);
        internal->data = (uint8_t *)calloc(1, requiredBytes);
        if (internal->data == NULL)
        {
            return; // ERROR
        }
        internal->allocatedBytes = requiredBytes;
    }
    internal->height = height;
    internal->width = width;
}

static void greedyMatFree(Mat mat)
{
    GreedyMatInternal *internal = (GreedyMatInternal *)mat;
    free(internal->data);
    free(internal);
}

Mat matInitGreedyMat(int height, int width, size_t typeSize)
{
    GreedyMatInternal *internal = (GreedyMatInternal *)calloc(1, sizeof(GreedyMatInternal));
    if (internal == NULL)
    {
        return NULL;
    }

    int totalBytes = (height * width) * typeSize;
    internal->data = (uint8_t *)calloc(1, totalBytes);
    if (internal->data == NULL)
    {
        free(internal);
        return NULL;
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

    return internal;
}
