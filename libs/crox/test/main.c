
#include "greedy_mat.h"
#include "mat.h"
#include "munit.h"

/* Example test: create a 4x4 matrix, write a value, and read it back. */
static MunitResult test_mat_create(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    Mat mat = matInitGreedyMat(4, 4, sizeof(int));
    munit_assert_not_null(mat);

    munit_assert_int32(matGetWidth(mat), ==, 4);
    munit_assert_int32(matGetHeight(mat), ==, 4);

    int counter = 0;
    for (int i = 0; i < matGetHeight(mat); i++)
    {
        for (int j = 0; j < matGetWidth(mat); j++)
        {
            counter++;
            matSet(mat, j, i, counter);
        }
    }

    counter = 0;
    for (int i = 0; i < matGetHeight(mat); i++)
    {
        for (int j = 0; j < matGetWidth(mat); j++)
        {
            counter++;
            int v = matGetI(mat, j, i);
            munit_assert_true(v == counter);
        }
    }

    matFree(mat);
    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
    {(char *)"/mat/create", test_mat_create, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}, //
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}                              //
};

static const MunitSuite test_suite = {
    (char *)"crox",         //
    test_suite_tests,       //
    NULL,                   //
    1,                      //
    MUNIT_SUITE_OPTION_NONE //
};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)])
{
    return munit_suite_main(&test_suite, NULL, argc, argv);
}
