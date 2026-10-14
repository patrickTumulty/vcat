
#include "common_types.h"
#include "greedy_mat.h"
#include "mat.h"
#include "munit.h"
#include <stddef.h>
#include <stdint.h>

/* Basic creation: a matrix reports the dimensions it was created with. */
static MunitResult test_create(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    Mat mat = NULL;
    munit_assert_int(matInitGreedyMat(&mat, 10, 20, sizeof(int32_t)), ==, RC_OK);
    munit_assert_not_null(mat);
    munit_assert_int32(matGetHeight(mat), ==, 10);
    munit_assert_int32(matGetWidth(mat), ==, 20);
    matFree(mat);
    return MUNIT_OK;
}

/* Dimension queries on a NULL matrix are safe and report zero. */
static MunitResult test_dimensions_null(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    munit_assert_int32(matGetWidth(NULL), ==, 0);
    munit_assert_int32(matGetHeight(NULL), ==, 0);
    return MUNIT_OK;
}

/* int32_t values written anywhere in the matrix can be read back. */
static MunitResult test_get_set_int32(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    const int32_t height = 5;
    const int32_t width = 7;
    Mat mat = NULL;
    munit_assert_int(matInitGreedyMat(&mat, height, width, sizeof(int32_t)), ==, RC_OK);
    munit_assert_not_null(mat);

    int32_t counter = 0;
    for (int32_t y = 0; y < height; y++)
    {
        for (int32_t x = 0; x < width; x++)
        {
            munit_assert_int(matSet(mat, x, y, counter++), ==, RC_OK);
        }
    }

    counter = 0;
    for (int32_t y = 0; y < height; y++)
    {
        for (int32_t x = 0; x < width; x++)
        {
            munit_assert_int32(matGet(mat, x, y, int32_t), ==, counter++);
        }
    }

    matFree(mat);
    return MUNIT_OK;
}

/* 8-bit signed and unsigned integers. */
static MunitResult test_get_set_8bit(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    Mat mat = NULL;
    munit_assert_int(matInitGreedyMat(&mat, 2, 2, sizeof(int8_t)), ==, RC_OK);
    munit_assert_not_null(mat);

    munit_assert_int(matSet(mat, 0, 0, (int8_t)-128), ==, RC_OK);
    munit_assert_int(matSet(mat, 1, 1, (int8_t)127), ==, RC_OK);
    munit_assert_int8(matGet(mat, 0, 0, int8_t), ==, INT8_C(-128));
    munit_assert_int8(matGet(mat, 1, 1, int8_t), ==, INT8_C(127));

    matFree(mat);

    Mat umat = NULL;
    munit_assert_int(matInitGreedyMat(&umat, 2, 2, sizeof(uint8_t)), ==, RC_OK);
    munit_assert_not_null(umat);

    munit_assert_int(matSet(umat, 0, 0, (uint8_t)0), ==, RC_OK);
    munit_assert_int(matSet(umat, 1, 1, (uint8_t)255), ==, RC_OK);
    munit_assert_uint8(matGet(umat, 0, 0, uint8_t), ==, UINT8_C(0));
    munit_assert_uint8(matGet(umat, 1, 1, uint8_t), ==, UINT8_C(255));

    matFree(umat);
    return MUNIT_OK;
}

/* 16-bit signed and unsigned integers. */
static MunitResult test_get_set_16bit(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    Mat mat = NULL;
    munit_assert_int(matInitGreedyMat(&mat, 2, 2, sizeof(int16_t)), ==, RC_OK);
    munit_assert_not_null(mat);

    munit_assert_int(matSet(mat, 0, 0, (int16_t)-32768), ==, RC_OK);
    munit_assert_int(matSet(mat, 1, 1, (int16_t)32767), ==, RC_OK);
    munit_assert_int16(matGet(mat, 0, 0, int16_t), ==, INT16_C(-32768));
    munit_assert_int16(matGet(mat, 1, 1, int16_t), ==, INT16_C(32767));

    matFree(mat);

    Mat umat = NULL;
    munit_assert_int(matInitGreedyMat(&umat, 2, 2, sizeof(uint16_t)), ==, RC_OK);
    munit_assert_not_null(umat);

    munit_assert_int(matSet(umat, 0, 0, (uint16_t)0), ==, RC_OK);
    munit_assert_int(matSet(umat, 1, 1, (uint16_t)65535), ==, RC_OK);
    munit_assert_uint16(matGet(umat, 0, 0, uint16_t), ==, UINT16_C(0));
    munit_assert_uint16(matGet(umat, 1, 1, uint16_t), ==, UINT16_C(65535));

    matFree(umat);
    return MUNIT_OK;
}

/* 32-bit signed/unsigned integers and float. */
static MunitResult test_get_set_32bit(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    Mat imat = NULL;
    munit_assert_int(matInitGreedyMat(&imat, 2, 2, sizeof(int32_t)), ==, RC_OK);
    munit_assert_not_null(imat);

    munit_assert_int(matSet(imat, 0, 0, INT32_C(-2147483647)), ==, RC_OK);
    munit_assert_int(matSet(imat, 1, 1, INT32_C(2147483647)), ==, RC_OK);
    munit_assert_int32(matGet(imat, 0, 0, int32_t), ==, INT32_C(-2147483647));
    munit_assert_int32(matGet(imat, 1, 1, int32_t), ==, INT32_C(2147483647));

    matFree(imat);

    Mat umat = NULL;
    munit_assert_int(matInitGreedyMat(&umat, 2, 2, sizeof(uint32_t)), ==, RC_OK);
    munit_assert_not_null(umat);

    munit_assert_int(matSet(umat, 0, 0, UINT32_C(0)), ==, RC_OK);
    munit_assert_int(matSet(umat, 1, 1, UINT32_C(4294967295)), ==, RC_OK);
    munit_assert_uint32(matGet(umat, 0, 0, uint32_t), ==, UINT32_C(0));
    munit_assert_uint32(matGet(umat, 1, 1, uint32_t), ==, UINT32_C(4294967295));

    matFree(umat);

    Mat fmat = NULL;
    munit_assert_int(matInitGreedyMat(&fmat, 2, 2, sizeof(float)), ==, RC_OK);
    munit_assert_not_null(fmat);

    munit_assert_int(matSet(fmat, 0, 0, -3.14159265f), ==, RC_OK);
    munit_assert_int(matSet(fmat, 1, 1, 3.14159265f), ==, RC_OK);
    munit_assert_float(matGet(fmat, 0, 0, float), ==, -3.14159265f);
    munit_assert_float(matGet(fmat, 1, 1, float), ==, 3.14159265f);

    matFree(fmat);
    return MUNIT_OK;
}

/* 64-bit signed/unsigned integers and double. */
static MunitResult test_get_set_64bit(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    Mat lmat = NULL;
    munit_assert_int(matInitGreedyMat(&lmat, 2, 2, sizeof(int64_t)), ==, RC_OK);
    munit_assert_not_null(lmat);

    munit_assert_int(matSet(lmat, 0, 0, INT64_C(-9223372036854775807)), ==, RC_OK);
    munit_assert_int(matSet(lmat, 1, 1, INT64_C(9223372036854775807)), ==, RC_OK);
    munit_assert_int64(matGet(lmat, 0, 0, int64_t), ==, INT64_C(-9223372036854775807));
    munit_assert_int64(matGet(lmat, 1, 1, int64_t), ==, INT64_C(9223372036854775807));

    matFree(lmat);

    Mat ulmat = NULL;
    munit_assert_int(matInitGreedyMat(&ulmat, 2, 2, sizeof(uint64_t)), ==, RC_OK);
    munit_assert_not_null(ulmat);

    munit_assert_int(matSet(ulmat, 0, 0, UINT64_C(0)), ==, RC_OK);
    munit_assert_int(matSet(ulmat, 1, 1, UINT64_C(18446744073709551615)), ==, RC_OK);
    munit_assert_uint64(matGet(ulmat, 0, 0, uint64_t), ==, UINT64_C(0));
    munit_assert_uint64(matGet(ulmat, 1, 1, uint64_t), ==, UINT64_C(18446744073709551615));

    matFree(ulmat);

    Mat dmat = NULL;
    munit_assert_int(matInitGreedyMat(&dmat, 2, 2, sizeof(double)), ==, RC_OK);
    munit_assert_not_null(dmat);

    munit_assert_int(matSet(dmat, 0, 0, -2.718281828459045), ==, RC_OK);
    munit_assert_int(matSet(dmat, 1, 1, 2.718281828459045), ==, RC_OK);
    munit_assert_double(matGet(dmat, 0, 0, double), ==, -2.718281828459045);
    munit_assert_double(matGet(dmat, 1, 1, double), ==, 2.718281828459045);

    matFree(dmat);
    return MUNIT_OK;
}

/* Boolean values. */
static MunitResult test_get_set_bool(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    Mat mat = NULL;
    munit_assert_int(matInitGreedyMat(&mat, 2, 2, sizeof(bool)), ==, RC_OK);
    munit_assert_not_null(mat);

    munit_assert_int(matSet(mat, 0, 0, (bool)true), ==, RC_OK);
    munit_assert_int(matSet(mat, 1, 1, (bool)false), ==, RC_OK);
    munit_assert_true(matGet(mat, 0, 0, bool));
    munit_assert_false(matGet(mat, 1, 1, bool));

    matFree(mat);
    return MUNIT_OK;
}

typedef struct
{
    int x;
    int y;
    int sum;
} Point;

/* Pointer values: store, retrieve, and mutate structs through the matrix. */
static MunitResult test_get_set_ptr(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    const int32_t count = 8;
    Point points[8] = {
        {0, 0, 0}, {1, 2, 0}, {3, 4, 0}, {5, 6, 0},
        {7, 8, 0}, {9, 10, 0}, {11, 12, 0}, {13, 14, 0},
    };

    Mat mat = NULL;
    munit_assert_int(matInitGreedyMat(&mat, 2, 4, sizeof(void *)), ==, RC_OK);
    munit_assert_not_null(mat);

    /* Store pointers to the structs in the matrix. */
    for (int32_t i = 0; i < count; i++)
    {
        int32_t x = i % matGetWidth(mat);
        int32_t y = i / matGetWidth(mat);
        munit_assert_int(matSetPtr(mat, x, y, &points[i]), ==, RC_OK);
    }

    /* Retrieve and verify initial values. */
    for (int32_t i = 0; i < count; i++)
    {
        int32_t x = i % matGetWidth(mat);
        int32_t y = i / matGetWidth(mat);
        Point *p = (Point *)matGetPtr(mat, x, y);
        munit_assert_ptr_equal(p, &points[i]);
        munit_assert_int32(p->x, ==, points[i].x);
        munit_assert_int32(p->y, ==, points[i].y);
    }

    /* Mutate the structs through the matrix pointers. */
    for (int32_t i = 0; i < count; i++)
    {
        int32_t x = i % matGetWidth(mat);
        int32_t y = i / matGetWidth(mat);
        Point *p = (Point *)matGetPtr(mat, x, y);
        p->sum = p->x + p->y;
    }

    /* Verify mutations landed in the original structs. */
    for (int32_t i = 0; i < count; i++)
    {
        munit_assert_int32(points[i].sum, ==, points[i].x + points[i].y);
    }

    matFree(mat);
    return MUNIT_OK;
}

/* Resizing changes the reported dimensions and new cells are usable. */
static MunitResult test_resize(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    Mat mat = NULL;
    munit_assert_int(matInitGreedyMat(&mat, 2, 3, sizeof(int32_t)), ==, RC_OK);
    munit_assert_not_null(mat);
    munit_assert_int32(matGetHeight(mat), ==, 2);
    munit_assert_int32(matGetWidth(mat), ==, 3);

    munit_assert_int(matResize(mat, 5, 7), ==, RC_OK);
    munit_assert_int32(matGetHeight(mat), ==, 5);
    munit_assert_int32(matGetWidth(mat), ==, 7);

    /* Writing and reading inside the new bounds works. */
    munit_assert_int(matSet(mat, 6, 4, (int32_t)12345), ==, RC_OK);
    munit_assert_int32(matGet(mat, 6, 4, int32_t), ==, INT32_C(12345));

    /* Resizing smaller also reports the new dimensions. */
    munit_assert_int(matResize(mat, 1, 1), ==, RC_OK);
    munit_assert_int32(matGetHeight(mat), ==, 1);
    munit_assert_int32(matGetWidth(mat), ==, 1);

    matFree(mat);
    return MUNIT_OK;
}

/* Free is safe for valid matrices and for NULL. */
static MunitResult test_free_null(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    matFree(NULL);

    Mat mat = NULL;
    munit_assert_int(matInitGreedyMat(&mat, 1, 1, sizeof(int32_t)), ==, RC_OK);
    munit_assert_not_null(mat);
    matFree(mat);

    return MUNIT_OK;
}

/* NULL handles are rejected by action functions and answered safely by the
 * query functions. */
static MunitResult test_null_safety(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    matFree(NULL);

    munit_assert_int(matResize(NULL, 5, 5), ==, RC_NOT_INITIALIZED);
    munit_assert_int(matSet(NULL, 0, 0, INT32_C(1)), ==, RC_NOT_INITIALIZED);

    munit_assert_int32(matGetWidth(NULL), ==, 0);
    munit_assert_int32(matGetHeight(NULL), ==, 0);
    munit_assert_int32(matGet(NULL, 0, 0, int32_t), ==, 0);
    munit_assert_int8(matGetB(NULL, 0, 0), ==, 0);
    munit_assert_uint8(matGetUB(NULL, 0, 0), ==, 0);
    munit_assert_int16(matGetS(NULL, 0, 0), ==, 0);
    munit_assert_uint16(matGetUS(NULL, 0, 0), ==, 0);
    munit_assert_uint32(matGetUI(NULL, 0, 0), ==, 0);
    munit_assert_int64(matGetL(NULL, 0, 0), ==, 0);
    munit_assert_uint64(matGetUL(NULL, 0, 0), ==, 0);
    munit_assert_float(matGetF(NULL, 0, 0), ==, 0.0f);
    munit_assert_double(matGetD(NULL, 0, 0), ==, 0.0);
    munit_assert_false(matGetBool(NULL, 0, 0));
    munit_assert_null(matGetPtr(NULL, 0, 0));

    return MUNIT_OK;
}

/* Input validation, bounds checks and element-size guards. */
static MunitResult test_errors(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    Mat mat = NULL;

    /* Init guards. */
    munit_assert_int(matInitGreedyMat(NULL, 2, 2, sizeof(int32_t)), ==, RC_BAD_INPUT_ARG);
    munit_assert_int(matInitGreedyMat(&mat, 0, 2, sizeof(int32_t)), ==, RC_BAD_INPUT_ARG);
    munit_assert_int(matInitGreedyMat(&mat, 2, 0, sizeof(int32_t)), ==, RC_BAD_INPUT_ARG);
    munit_assert_int(matInitGreedyMat(&mat, -1, 2, sizeof(int32_t)), ==, RC_BAD_INPUT_ARG);
    munit_assert_int(matInitGreedyMat(&mat, 2, 2, 0), ==, RC_BAD_INPUT_ARG);
    munit_assert_null(mat);

    /* Bounds checks on set and get. */
    munit_assert_int(matInitGreedyMat(&mat, 2, 2, sizeof(int32_t)), ==, RC_OK);
    munit_assert_not_null(mat);

    munit_assert_int(matSetI(mat, 2, 0, 1), ==, RC_BAD_INPUT_ARG);
    munit_assert_int(matSetI(mat, 0, 2, 1), ==, RC_BAD_INPUT_ARG);
    munit_assert_int(matSetI(mat, -1, 0, 1), ==, RC_BAD_INPUT_ARG);
    munit_assert_int32(matGetI(mat, 2, 0), ==, 0);
    munit_assert_int32(matGetI(mat, 0, -1), ==, 0);
    matFree(mat);

    /* Accessor value larger than the element size is rejected. */
    Mat tiny = NULL;
    munit_assert_int(matInitGreedyMat(&tiny, 2, 2, sizeof(uint8_t)), ==, RC_OK);
    munit_assert_not_null(tiny);
    munit_assert_int(matSetI(tiny, 0, 0, 5), ==, RC_BAD_INPUT_ARG);
    munit_assert_int32(matGetI(tiny, 0, 0), ==, 0);
    matFree(tiny);

    /* Resize guards leave the matrix unchanged. */
    Mat rmat = NULL;
    munit_assert_int(matInitGreedyMat(&rmat, 2, 2, sizeof(int32_t)), ==, RC_OK);
    munit_assert_int(matResize(rmat, 0, 2), ==, RC_BAD_INPUT_ARG);
    munit_assert_int(matResize(rmat, 2, 0), ==, RC_BAD_INPUT_ARG);
    munit_assert_int(matResize(rmat, -1, 2), ==, RC_BAD_INPUT_ARG);
    munit_assert_int(matResize(NULL, 2, 2), ==, RC_NOT_INITIALIZED);
    munit_assert_int32(matGetHeight(rmat), ==, 2);
    munit_assert_int32(matGetWidth(rmat), ==, 2);
    matFree(rmat);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
    {(char *)"/create", test_create, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/dimensions/null", test_dimensions_null, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/get-set/int32", test_get_set_int32, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/get-set/8bit", test_get_set_8bit, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/get-set/16bit", test_get_set_16bit, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/get-set/32bit", test_get_set_32bit, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/get-set/64bit", test_get_set_64bit, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/get-set/bool", test_get_set_bool, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/get-set/ptr", test_get_set_ptr, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/resize", test_resize, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/free/null", test_free_null, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/null-safety", test_null_safety, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/errors", test_errors, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {
    (char *)"crox",
    test_suite_tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)])
{
    return munit_suite_main(&test_suite, NULL, argc, argv);
}
