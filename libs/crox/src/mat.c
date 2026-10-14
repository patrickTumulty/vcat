
#include "mat.h"
#include "mat_shared.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>


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

void matResize(Mat mat, int height, int width)
{
    if (mat == NULL)
        return;
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

static uint8_t *matGetInternal(Mat mat, int32_t x, int32_t y)
{
    if (mat == NULL)
        return NULL;
    MatInternalHeader *internal = (MatInternalHeader *)mat;
    return internal->get(mat, x, y);
}

static void matSetInternal(Mat mat, int32_t x, int32_t y, const void *v, const size_t vSize)
{
    if (mat == NULL)
        return;
    MatInternalHeader *internal = (MatInternalHeader *)mat;
    internal->set(mat, x, y, v, vSize);
}

int8_t matGetB(Mat mat, int32_t x, int32_t y)
{
    return *((int8_t *)matGetInternal(mat, x, y));
}

uint8_t matGetUB(Mat mat, int32_t x, int32_t y)
{
    return *((uint8_t *)matGetInternal(mat, x, y));
}

int16_t matGetS(Mat mat, int32_t x, int32_t y)
{
    return *((int16_t *)matGetInternal(mat, x, y));
}

uint16_t matGetUS(Mat mat, int32_t x, int32_t y)
{
    return *((uint16_t *)matGetInternal(mat, x, y));
}

int32_t matGetI(Mat mat, int32_t x, int32_t y)
{
    return *((int32_t *)matGetInternal(mat, x, y));
}

uint32_t matGetUI(Mat mat, int32_t x, int32_t y)
{
    return *((uint32_t *)matGetInternal(mat, x, y));
}

int64_t matGetL(Mat mat, int32_t x, int32_t y)
{
    return *((int64_t *)matGetInternal(mat, x, y));
}

uint64_t matGetUL(Mat mat, int32_t x, int32_t y)
{
    return *((uint64_t *)matGetInternal(mat, x, y));
}

float matGetF(Mat mat, int32_t x, int32_t y)
{
    return *((float *)matGetInternal(mat, x, y));
}

double matGetD(Mat mat, int32_t x, int32_t y)
{
    return *((double *)matGetInternal(mat, x, y));
}

bool matGetBool(Mat mat, int32_t x, int32_t y)
{
    return *((bool *)matGetInternal(mat, x, y));
}

void *matGetPtr(Mat mat, int32_t x, int32_t y)
{
    uint8_t *cell = matGetInternal(mat, x, y);
    if (cell == NULL)
        return NULL;
    return *((void **)cell);
}


void matSetB(Mat mat, int32_t x, int32_t y, const int8_t v)
{
    matSetInternal(mat, x, y, &v, sizeof(v));
}

void matSetUB(Mat mat, int32_t x, int32_t y, const uint8_t v)
{
    matSetInternal(mat, x, y, &v, sizeof(v));
}

void matSetS(Mat mat, int32_t x, int32_t y, const int16_t v)
{
    matSetInternal(mat, x, y, &v, sizeof(v));
}

void matSetUS(Mat mat, int32_t x, int32_t y, const uint16_t v)
{
    matSetInternal(mat, x, y, &v, sizeof(v));
}

void matSetI(Mat mat, int32_t x, int32_t y, const int32_t v)
{
    matSetInternal(mat, x, y, &v, sizeof(v));
}

void matSetUI(Mat mat, int32_t x, int32_t y, const uint32_t v)
{
    matSetInternal(mat, x, y, &v, sizeof(v));
}

void matSetL(Mat mat, int32_t x, int32_t y, const int64_t v)
{
    matSetInternal(mat, x, y, &v, sizeof(v));
}

void matSetUL(Mat mat, int32_t x, int32_t y, const uint64_t v)
{
    matSetInternal(mat, x, y, &v, sizeof(v));
}

void matSetF(Mat mat, int32_t x, int32_t y, const float v)
{
    matSetInternal(mat, x, y, &v, sizeof(v));
}

void matSetD(Mat mat, int32_t x, int32_t y, const double v)
{
    matSetInternal(mat, x, y, &v, sizeof(v));
}

void matSetBool(Mat mat, int32_t x, int32_t y, const bool v)
{
    matSetInternal(mat, x, y, &v, sizeof(v));
}

void matSetPtr(Mat mat, int32_t x, int32_t y, const void *v)
{
    matSetInternal(mat, x, y, &v, sizeof(void *));
}
