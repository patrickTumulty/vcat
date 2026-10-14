
#ifndef MAT_H
#define MAT_H

#include <stdbool.h>
#include <stdint.h>

typedef void *Mat;

int32_t matGetHeight(Mat mat);
int32_t matGetWidth(Mat mat);
void matResize(Mat mat, int height, int width);
void matFree(Mat mat);

int8_t matGetB(Mat mat, int32_t x, int32_t y);
uint8_t matGetUB(Mat mat, int32_t x, int32_t y);
int16_t matGetS(Mat mat, int32_t x, int32_t y);
uint16_t matGetUS(Mat mat, int32_t x, int32_t y);
int32_t matGetI(Mat mat, int32_t x, int32_t y);
uint32_t matGetUI(Mat mat, int32_t x, int32_t y);
int64_t matGetL(Mat mat, int32_t x, int32_t y);
uint64_t matGetUL(Mat mat, int32_t x, int32_t y);
float matGetF(Mat mat, int32_t x, int32_t y);
double matGetD(Mat mat, int32_t x, int32_t y);
bool matGetBool(Mat mat, int32_t x, int32_t y);
void *matGetPtr(Mat mat, int32_t x, int32_t y);

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

void matSetB(Mat mat, int32_t x, int32_t y, const int8_t v);
void matSetUB(Mat mat, int32_t x, int32_t y, const uint8_t v);
void matSetS(Mat mat, int32_t x, int32_t y, const int16_t v);
void matSetUS(Mat mat, int32_t x, int32_t y, const uint16_t v);
void matSetI(Mat mat, int32_t x, int32_t y, const int32_t v);
void matSetUI(Mat mat, int32_t x, int32_t y, const uint32_t v);
void matSetL(Mat mat, int32_t x, int32_t y, const int64_t v);
void matSetUL(Mat mat, int32_t x, int32_t y, const uint64_t v);
void matSetF(Mat mat, int32_t x, int32_t y, const float v);
void matSetD(Mat mat, int32_t x, int32_t y, const double v);
void matSetBool(Mat mat, int32_t x, int32_t y, const bool v);
void matSetPtr(Mat mat, int32_t x, int32_t y, const void *v);

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
