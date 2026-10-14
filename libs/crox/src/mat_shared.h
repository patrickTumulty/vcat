
#ifndef MAT_SHARED_H
#define MAT_SHARED_H

#include "mat.h"
#include <stddef.h>
#include <stdint.h>

typedef uint8_t *(*MatAPIGet)(Mat mat, int32_t x, int32_t y);
typedef void (*MatAPISet)(Mat mat, int32_t x, int32_t y, uint8_t *v, size_t vSize);
typedef int32_t (*MatAPIGetWidth)(Mat mat);
typedef int32_t (*MatAPIGetHeight)(Mat mat);
typedef void (*MatAPIResize)(Mat mat, int32_t height, int32_t width);
typedef void (*MatAPIFree)(Mat mat);

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
