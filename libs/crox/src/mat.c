
#include "mat.h"
#include "mat_shared.h"
#include <stddef.h>
#include <stdint.h>

int32_t matGetHeight(Mat mat)
{
    if (mat == NULL)
        return 0;
    MatInternalHeader *internal = (MatInternalHeader *)mat;
    return internal->getHeight(mat);
}

int32_t matGetWidth(Mat mat)
{
    if (mat == NULL)
        return 0;
    MatInternalHeader *internal = (MatInternalHeader *)mat;
    return internal->getWidth(mat);
}

Rc matResize(Mat mat, int32_t height, int32_t width)
{
    if (mat == NULL)
        return RC_NOT_INITIALIZED;
    MatInternalHeader *internal = (MatInternalHeader *)mat;
    return internal->resize(mat, height, width);
}

void matFree(Mat mat)
{
    if (mat == NULL)
        return;
    MatInternalHeader *internal = (MatInternalHeader *)mat;
    internal->free(mat);
}

static Rc matGetInternal(Mat mat, int32_t x, int32_t y, void *out, size_t size)
{
    if (mat == NULL)
        return RC_NOT_INITIALIZED;
    MatInternalHeader *internal = (MatInternalHeader *)mat;
    return internal->get(mat, x, y, out, size);
}

static Rc matSetInternal(Mat mat, int32_t x, int32_t y, const void *v, size_t size)
{
    if (mat == NULL)
        return RC_NOT_INITIALIZED;
    MatInternalHeader *internal = (MatInternalHeader *)mat;
    return internal->set(mat, x, y, v, size);
}

int8_t matGetB(Mat mat, int32_t x, int32_t y)
{
    int8_t v = 0;
    matGetInternal(mat, x, y, &v, sizeof(v));
    return v;
}

uint8_t matGetUB(Mat mat, int32_t x, int32_t y)
{
    uint8_t v = 0;
    matGetInternal(mat, x, y, &v, sizeof(v));
    return v;
}

int16_t matGetS(Mat mat, int32_t x, int32_t y)
{
    int16_t v = 0;
    matGetInternal(mat, x, y, &v, sizeof(v));
    return v;
}

uint16_t matGetUS(Mat mat, int32_t x, int32_t y)
{
    uint16_t v = 0;
    matGetInternal(mat, x, y, &v, sizeof(v));
    return v;
}

int32_t matGetI(Mat mat, int32_t x, int32_t y)
{
    int32_t v = 0;
    matGetInternal(mat, x, y, &v, sizeof(v));
    return v;
}

uint32_t matGetUI(Mat mat, int32_t x, int32_t y)
{
    uint32_t v = 0;
    matGetInternal(mat, x, y, &v, sizeof(v));
    return v;
}

int64_t matGetL(Mat mat, int32_t x, int32_t y)
{
    int64_t v = 0;
    matGetInternal(mat, x, y, &v, sizeof(v));
    return v;
}

uint64_t matGetUL(Mat mat, int32_t x, int32_t y)
{
    uint64_t v = 0;
    matGetInternal(mat, x, y, &v, sizeof(v));
    return v;
}

float matGetF(Mat mat, int32_t x, int32_t y)
{
    float v = 0;
    matGetInternal(mat, x, y, &v, sizeof(v));
    return v;
}

double matGetD(Mat mat, int32_t x, int32_t y)
{
    double v = 0;
    matGetInternal(mat, x, y, &v, sizeof(v));
    return v;
}

bool matGetBool(Mat mat, int32_t x, int32_t y)
{
    bool v = false;
    matGetInternal(mat, x, y, &v, sizeof(v));
    return v;
}

void *matGetPtr(Mat mat, int32_t x, int32_t y)
{
    void *v = NULL;
    matGetInternal(mat, x, y, &v, sizeof(v));
    return v;
}

Rc matSetB(Mat mat, int32_t x, int32_t y, int8_t v)
{
    return matSetInternal(mat, x, y, &v, sizeof(v));
}

Rc matSetUB(Mat mat, int32_t x, int32_t y, uint8_t v)
{
    return matSetInternal(mat, x, y, &v, sizeof(v));
}

Rc matSetS(Mat mat, int32_t x, int32_t y, int16_t v)
{
    return matSetInternal(mat, x, y, &v, sizeof(v));
}

Rc matSetUS(Mat mat, int32_t x, int32_t y, uint16_t v)
{
    return matSetInternal(mat, x, y, &v, sizeof(v));
}

Rc matSetI(Mat mat, int32_t x, int32_t y, int32_t v)
{
    return matSetInternal(mat, x, y, &v, sizeof(v));
}

Rc matSetUI(Mat mat, int32_t x, int32_t y, uint32_t v)
{
    return matSetInternal(mat, x, y, &v, sizeof(v));
}

Rc matSetL(Mat mat, int32_t x, int32_t y, int64_t v)
{
    return matSetInternal(mat, x, y, &v, sizeof(v));
}

Rc matSetUL(Mat mat, int32_t x, int32_t y, uint64_t v)
{
    return matSetInternal(mat, x, y, &v, sizeof(v));
}

Rc matSetF(Mat mat, int32_t x, int32_t y, float v)
{
    return matSetInternal(mat, x, y, &v, sizeof(v));
}

Rc matSetD(Mat mat, int32_t x, int32_t y, double v)
{
    return matSetInternal(mat, x, y, &v, sizeof(v));
}

Rc matSetBool(Mat mat, int32_t x, int32_t y, bool v)
{
    return matSetInternal(mat, x, y, &v, sizeof(v));
}

Rc matSetPtr(Mat mat, int32_t x, int32_t y, const void *v)
{
    return matSetInternal(mat, x, y, &v, sizeof(v));
}
