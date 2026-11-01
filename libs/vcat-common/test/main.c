
#include "ascii_frame_mailbox.h"
#include "color_utils.h"
#include "greedy_matrix.h"
#include "matrix.h"
#include "munit.h"
#include "utils.h"
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>

/* Frames are distinguished by a marker written into cell (0,0) of the char
 * matrix. Each frame is created through the crox matrix API so the mailbox has
 * something real to own and free. */
static AsciiFrame makeFrame(char marker)
{
    AsciiFrame frame = {NULL, NULL};
    if (matrix_init_greedy_u32(&frame.chars, 1, 1) != RC_OK)
    {
        return (AsciiFrame){NULL, NULL};
    }
    if (matrix_init_greedy_u32(&frame.colors, 1, 1) != RC_OK)
    {
        matrix_free(frame.chars);
        return (AsciiFrame){NULL, NULL};
    }
    matrix_set_u32(frame.chars, 0, 0, marker);
    return frame;
}

static char markerOf(AsciiFrame frame)
{
    return (char)matrix_get_u32(frame.chars, 0, 0);
}

/* The sRGB decode curve is bounded and monotonic per byte. */
static MunitResult test_color_srgb_to_linear_bounds(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    munit_assert_double(vcatColorSrgbToLinear(0), ==, 0.0);
    munit_assert_double(vcatColorSrgbToLinear(255), ==, 1.0);

    float last = 0.0f;
    for (int i = 0; i <= 255; i++)
    {
        const float linear = vcatColorSrgbToLinear((uint8_t)i);
        munit_assert_double(linear, >=, last);
        munit_assert_double(linear, <=, 1.0);
        last = linear;
    }

    return MUNIT_OK;
}

/* The encode curve inverts decode for mid-channel values: every sRGB byte that
 * goes through linear light and back lands on itself (rounding allows 1). */
static MunitResult test_color_srgb_round_trip(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    for (int i = 0; i <= 255; i++)
    {
        const float linear = vcatColorSrgbToLinear((uint8_t)i);
        const int decoded = vcatColorLinearToSrgbByte(linear);
        munit_assert_int(abs(decoded - i), <=, 1);
    }

    return MUNIT_OK;
}

/* Black and white mixed in linear light land at ~188 sRGB gray, not 128: the
 * eye averages luminance, not sRGB bytes. This brightness bias is the point of
 * linear averaging. */
static MunitResult test_color_average_black_white_is_bright(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    VcatColorAverage average;
    vcatColorAverageInit(&average);
    vcatColorAverageAdd(&average, 0);
    vcatColorAverageAdd(&average, 0x00FFFFFF);

    const VcatColor mixed = vcatColorAverageFinish(&average);
    const int gray = (int)(mixed & 0xFF);
    munit_assert_int(gray, !=, 127);                        // Linear averaging is not byte averaging.
    munit_assert_int(gray, ==, (int)((mixed >> 8) & 0xFF)); // And black+white stays gray.
    munit_assert_int(gray, ==, (int)(mixed >> 16));
    munit_assert_int(gray, >=, 185);
    munit_assert_int(gray, <=, 190);

    return MUNIT_OK;
}

/* A gray window stays gray, and an empty window encodes to black. */
static MunitResult test_color_average_gray_stays_gray(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    VcatColorAverage average;
    vcatColorAverageInit(&average);
    munit_assert_uint(vcatColorAverageFinish(&average), ==, 0);

    vcatColorAverageInit(&average);
    vcatColorAverageAdd(&average, 0x00646464U); // sRGB 100
    vcatColorAverageAdd(&average, 0x00C8C8C8U); // sRGB 200
    const VcatColor mixed = vcatColorAverageFinish(&average);
    munit_assert_uint(mixed >> 16, ==, mixed & 0xFF);

    return MUNIT_OK;
}

/* Brightness weights like human vision: green dominates, white and black map
 * onto themselves, and equal-weight gray maps to its own byte. */
static MunitResult test_color_luminance_weights(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    munit_assert_uint(vcatColorLuminance(0x00000000U), ==, 0);
    munit_assert_uint(vcatColorLuminance(0x00FFFFFFU), ==, 255);
    munit_assert_uint(vcatColorLuminance(0x00808080U), ==, 128);

    // Green carries far more luminance than blue at equal byte value.
    munit_assert_uint(vcatColorLuminance(0x0000FF00U), >, vcatColorLuminance(0x000000FFU));
    munit_assert_uint(vcatColorLuminance(0x0000FF00U), >, vcatColorLuminance(0x00FF0000U));

    return MUNIT_OK;
}

/* The CLI, the OSD and the renderer all brush names from the same source. */
static MunitResult test_color_mode_names(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    munit_assert_string_equal(vcatColorModeName(VCAT_COLOR_MODE_NO_COLOR), "no color");
    munit_assert_string_equal(vcatColorModeName(VCAT_COLOR_MODE_BLACK_WHITE), "black & white");
    munit_assert_string_equal(vcatColorModeName(VCAT_COLOR_MODE_COLOR), "color");
    munit_assert_string_equal(vcatColorModeName((VcatColorMode)-1), "invalid");
    munit_assert_string_equal(vcatColorModeName(VCAT_COLOR_MODE_COUNT), "invalid");

    return MUNIT_OK;
}

/* Exact palette corners answer themselves, and the system colors are never
 * touched. */
static MunitResult test_color_palette_corners(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    munit_assert_short(vcatColorNearestPaletteIndex(0x00000000U), ==, 16);  // Cube corner black.
    munit_assert_short(vcatColorNearestPaletteIndex(0x00FFFFFFU), ==, 231); // Cube corner white.
    munit_assert_short(vcatColorNearestPaletteIndex(0x00FF0000U), ==, 196); // Pure red.
    munit_assert_short(vcatColorNearestPaletteIndex(0x0000FF00U), ==, 46);  // Pure green.
    munit_assert_short(vcatColorNearestPaletteIndex(0x000000FFU), ==, 21);  // Pure blue.
    munit_assert_short(vcatColorNearestPaletteIndex(0x00808080U), ==, 244); // Exact gray step 12.
    munit_assert_short(vcatColorNearestPaletteIndex(0x00080808U), ==, 232); // Gray ramp bottom.

    return MUNIT_OK;
}

/* Repeated and querying on a second thread must agree (the per-thread cache
 * must behave like the exact search). */
typedef struct
{
    uint32_t color;
    short index;
} PaletteQuery;

static void *paletteQueryThread(void *arg)
{
    PaletteQuery *query = arg;
    query->index = vcatColorNearestPaletteIndex(query->color);
    return NULL;
}

static MunitResult test_color_palette_cache_cross_thread(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    // Warm this thread's cache with a neighbor color first.
    munit_assert_short(vcatColorNearestPaletteIndex(0x007F7F7FU), ==, 244);

    PaletteQuery mainThread = {.color = 0x00808080U};
    mainThread.index = vcatColorNearestPaletteIndex(mainThread.color);

    PaletteQuery otherThread = {.color = 0x00808080U};
    pthread_t thread;
    munit_assert_int(pthread_create(&thread, NULL, paletteQueryThread, &otherThread), ==, 0);
    munit_assert_int(pthread_join(thread, NULL), ==, 0);

    munit_assert_short(otherThread.index, ==, mainThread.index);
    munit_assert_short(otherThread.index, ==, 244);

    return MUNIT_OK;
}

/* Nothing to fit into (zero or negative extents) yields an empty grid. */
static MunitResult test_fit_rejects_nothing_to_fit(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    const Rectangle badHeight = vcatFitDimensionsToRatio((Rectangle){0, 10}, 2.0f, MAX_ASPECT_REL_ERROR);
    munit_assert_int(badHeight.width, ==, 0);
    munit_assert_int(badHeight.height, ==, 0);

    const Rectangle badWidth = vcatFitDimensionsToRatio((Rectangle){10, 0}, 2.0f, MAX_ASPECT_REL_ERROR);
    munit_assert_int(badWidth.width, ==, 0);
    munit_assert_int(badWidth.height, ==, 0);

    const Rectangle negativeHeight = vcatFitDimensionsToRatio((Rectangle){-3, 10}, 2.0f, MAX_ASPECT_REL_ERROR);
    munit_assert_int(negativeHeight.height, ==, 0);

    const Rectangle negativeWidth = vcatFitDimensionsToRatio((Rectangle){10, -3}, 2.0f, MAX_ASPECT_REL_ERROR);
    munit_assert_int(negativeWidth.width, ==, 0);

    return MUNIT_OK;
}

/* A non-positive target ratio yields an empty grid. */
static MunitResult test_fit_rejects_non_positive_ratio(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    const Rectangle zeroRatio = vcatFitDimensionsToRatio((Rectangle){10, 10}, 0.0f, MAX_ASPECT_REL_ERROR);
    munit_assert_int(zeroRatio.width, ==, 0);
    munit_assert_int(zeroRatio.height, ==, 0);

    const Rectangle negativeRatio = vcatFitDimensionsToRatio((Rectangle){10, 10}, -1.5f, MAX_ASPECT_REL_ERROR);
    munit_assert_int(negativeRatio.width, ==, 0);

    return MUNIT_OK;
}

/* The exact ratio is reachable, so the full height is used. */
static MunitResult test_fit_exact_ratio_is_hit(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    const Rectangle fitted = vcatFitDimensionsToRatio((Rectangle){24, 80}, 2.0f, MAX_ASPECT_REL_ERROR);

    munit_assert_int(fitted.height, ==, 24);
    munit_assert_int(fitted.width, ==, 48);

    return MUNIT_OK;
}

/* A 2.0 ratio in 10x15 cells: the exact ratio is only reachable up to 7x14,
 * which beats the bigger but out-of-tolerance 10x15 (25% off) and 8x15 (6.25%
 * off) candidates. */
static MunitResult test_fit_prefers_largest_within_tolerance(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    const Rectangle fitted = vcatFitDimensionsToRatio((Rectangle){10, 15}, 2.0f, MAX_ASPECT_REL_ERROR);

    munit_assert_int(fitted.height, ==, 7);
    munit_assert_int(fitted.width, ==, 14);

    return MUNIT_OK;
}

/* 3.3 in 2 rows: 6/2 and 3/1 are both 9.1% off, 7/2 is 6.1% off and wins
 * despite the tiny grid. */
static MunitResult test_fit_falls_back_to_least_wrong(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    const Rectangle fitted = vcatFitDimensionsToRatio((Rectangle){2, 100}, 3.3f, MAX_ASPECT_REL_ERROR);

    munit_assert_int(fitted.height, ==, 2);
    munit_assert_int(fitted.width, ==, 7);

    return MUNIT_OK;
}

/* 16:9 video on an 80x24 terminal: the character grid ratio is 16/9 *
 * CHAR_CELL_ASPECT, and the grid must fit the terminal. */
static MunitResult test_fit_widescreen_into_standard_terminal(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    const float targetRatio = (16.0f / 9.0f) * CHAR_CELL_ASPECT;

    const Rectangle fitted = vcatFitDimensionsToRatio((Rectangle){24, 80}, targetRatio, MAX_ASPECT_REL_ERROR);

    munit_assert_int(fitted.height, ==, 22);
    munit_assert_int(fitted.width, ==, 78);
    munit_assert_int(fitted.width, <=, 80);
    munit_assert_int(fitted.height, <=, 24);

    return MUNIT_OK;
}

/* Acquiring from an empty mailbox hands out nothing. */
static MunitResult test_mailbox_empty_acquires_nothing(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    AsciiFrameMailbox *mailbox = vcatAsciiFrameMailboxCreate();
    munit_assert_not_null(mailbox);

    AsciiFrame frame = {NULL, NULL};
    munit_assert_false(vcatAsciiFrameMailboxAcquireLatest(mailbox, &frame));
    munit_assert_false(vcatAsciiFrameMailboxAcquireFree(mailbox, &frame));
    munit_assert_null(frame.chars);

    vcatAsciiFrameMailboxFree(mailbox);
    return MUNIT_OK;
}

/* A published frame is handed out exactly once. */
static MunitResult test_mailbox_acquire_latest_returns_the_published_frame(const MunitParameter params[],
                                                                           void *user_data)
{
    (void)params;
    (void)user_data;

    AsciiFrameMailbox *mailbox = vcatAsciiFrameMailboxCreate();
    AsciiFrame frame = makeFrame('a');
    munit_assert_not_null(frame.chars);

    vcatAsciiFrameMailboxPublish(mailbox, frame);

    AsciiFrame acquired = {NULL, NULL};
    munit_assert_true(vcatAsciiFrameMailboxAcquireLatest(mailbox, &acquired));
    munit_assert_ptr_equal(acquired.chars, frame.chars);
    munit_assert_char(markerOf(acquired), ==, 'a');
    vcatAsciiFrameMailboxRelease(mailbox, acquired);

    AsciiFrame drained = {NULL, NULL};
    munit_assert_false(vcatAsciiFrameMailboxAcquireLatest(mailbox, &drained));

    vcatAsciiFrameMailboxFree(mailbox);
    return MUNIT_OK;
}

/* A consumer that falls behind must get the latest frame: publishing twice
 * keeps only the newest. */
static MunitResult test_mailbox_publish_twice_keeps_newest(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    AsciiFrameMailbox *mailbox = vcatAsciiFrameMailboxCreate();
    vcatAsciiFrameMailboxPublish(mailbox, makeFrame('a'));
    vcatAsciiFrameMailboxPublish(mailbox, makeFrame('b'));

    AsciiFrame acquired = {NULL, NULL};
    munit_assert_true(vcatAsciiFrameMailboxAcquireLatest(mailbox, &acquired));
    munit_assert_char(markerOf(acquired), ==, 'b');
    vcatAsciiFrameMailboxRelease(mailbox, acquired);

    vcatAsciiFrameMailboxFree(mailbox);
    return MUNIT_OK;
}

/* The frame displaced from the latest slot is recycled, not dropped. */
static MunitResult test_mailbox_displaced_frame_recycled(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    AsciiFrameMailbox *mailbox = vcatAsciiFrameMailboxCreate();
    AsciiFrame first = makeFrame('a');
    vcatAsciiFrameMailboxPublish(mailbox, first);
    vcatAsciiFrameMailboxPublish(mailbox, makeFrame('b'));

    AsciiFrame recycled = {NULL, NULL};
    munit_assert_true(vcatAsciiFrameMailboxAcquireFree(mailbox, &recycled));
    munit_assert_ptr_equal(recycled.chars, first.chars);
    vcatAsciiFrameMailboxRelease(mailbox, recycled);

    vcatAsciiFrameMailboxFree(mailbox);
    return MUNIT_OK;
}

/* Released frames come back from acquireFree oldest first. */
static MunitResult test_mailbox_release_order_fifo(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    AsciiFrameMailbox *mailbox = vcatAsciiFrameMailboxCreate();
    AsciiFrame first = makeFrame('a');
    AsciiFrame second = makeFrame('b');

    vcatAsciiFrameMailboxRelease(mailbox, first);
    vcatAsciiFrameMailboxRelease(mailbox, second);

    AsciiFrame acquiredFirst = {NULL, NULL};
    AsciiFrame acquiredSecond = {NULL, NULL};
    munit_assert_true(vcatAsciiFrameMailboxAcquireFree(mailbox, &acquiredFirst));
    munit_assert_true(vcatAsciiFrameMailboxAcquireFree(mailbox, &acquiredSecond));
    munit_assert_ptr_equal(acquiredFirst.chars, first.chars);
    munit_assert_ptr_equal(acquiredSecond.chars, second.chars);

    AsciiFrame drained = {NULL, NULL};
    munit_assert_false(vcatAsciiFrameMailboxAcquireFree(mailbox, &drained));

    vcatAsciiFrameMailboxRelease(mailbox, acquiredFirst);
    vcatAsciiFrameMailboxRelease(mailbox, acquiredSecond);
    vcatAsciiFrameMailboxFree(mailbox);
    return MUNIT_OK;
}

/* Publish three frames, read the newest and release it: all three frames must
 * come back through the free pool, each exactly once. */
static MunitResult test_mailbox_every_frame_accounted_for(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    AsciiFrameMailbox *mailbox = vcatAsciiFrameMailboxCreate();
    AsciiFrame a = makeFrame('a');
    AsciiFrame b = makeFrame('b');
    AsciiFrame c = makeFrame('c');

    vcatAsciiFrameMailboxPublish(mailbox, a);
    vcatAsciiFrameMailboxPublish(mailbox, b);
    vcatAsciiFrameMailboxPublish(mailbox, c);

    AsciiFrame latest = {NULL, NULL};
    munit_assert_true(vcatAsciiFrameMailboxAcquireLatest(mailbox, &latest));
    munit_assert_ptr_equal(latest.chars, c.chars);
    vcatAsciiFrameMailboxRelease(mailbox, latest);

    AsciiFrame first = {NULL, NULL};
    AsciiFrame second = {NULL, NULL};
    AsciiFrame third = {NULL, NULL};
    munit_assert_true(vcatAsciiFrameMailboxAcquireFree(mailbox, &first));
    munit_assert_true(vcatAsciiFrameMailboxAcquireFree(mailbox, &second));
    munit_assert_true(vcatAsciiFrameMailboxAcquireFree(mailbox, &third));
    munit_assert_ptr_equal(first.chars, a.chars);
    munit_assert_ptr_equal(second.chars, b.chars);
    munit_assert_ptr_equal(third.chars, c.chars);

    AsciiFrame drained = {NULL, NULL};
    munit_assert_false(vcatAsciiFrameMailboxAcquireFree(mailbox, &drained));

    vcatAsciiFrameMailboxRelease(mailbox, first);
    vcatAsciiFrameMailboxRelease(mailbox, second);
    vcatAsciiFrameMailboxRelease(mailbox, third);
    vcatAsciiFrameMailboxFree(mailbox);
    return MUNIT_OK;
}

/* NULL mailbox safety: acquiring answers false, releasing and publishing free
 * what they are given so no frame leaks into the void. */
static MunitResult test_mailbox_null_safety(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    AsciiFrame frame = {NULL, NULL};
    munit_assert_false(vcatAsciiFrameMailboxAcquireLatest(NULL, &frame));
    munit_assert_false(vcatAsciiFrameMailboxAcquireFree(NULL, &frame));
    munit_assert_false(vcatAsciiFrameMailboxAcquireLatest(NULL, NULL));
    munit_assert_false(vcatAsciiFrameMailboxAcquireFree(NULL, NULL));

    vcatAsciiFrameMailboxRelease(NULL, makeFrame('x'));
    vcatAsciiFrameMailboxPublish(NULL, makeFrame('y'));

    vcatAsciiFrameMailboxFree(NULL);

    return MUNIT_OK;
}

/* The writer publishes frames while the reader drains them at its own pace.
 * The reader must eventually observe the newest frame, however many it
 * skipped meanwhile. */
typedef struct
{
    AsciiFrameMailbox *mailbox;
} MailboxStressContext;

static void *mailboxStressProducer(void *arg)
{
    MailboxStressContext *context = arg;
    const char lastMarker = 99;

    for (char marker = 0; marker <= lastMarker; marker++)
    {
        AsciiFrame frame = {NULL, NULL};
        if (!vcatAsciiFrameMailboxAcquireFree(context->mailbox, &frame))
        {
            frame = makeFrame(0);
        }
        matrix_set_u32(frame.chars, 0, 0, marker);
        vcatAsciiFrameMailboxPublish(context->mailbox, frame);
    }
    return NULL;
}

static void *mailboxStressConsumer(void *arg)
{
    MailboxStressContext *context = arg;
    char lastSeen = 0;
    int spinsLeft = 1000000;

    while (lastSeen != 99 && spinsLeft-- > 0)
    {
        AsciiFrame frame = {NULL, NULL};
        if (!vcatAsciiFrameMailboxAcquireLatest(context->mailbox, &frame))
        {
            sched_yield();
            continue;
        }
        lastSeen = markerOf(frame);
        vcatAsciiFrameMailboxRelease(context->mailbox, frame);
    }

    munit_assert_char(lastSeen, ==, 99);
    return NULL;
}

static MunitResult test_mailbox_concurrent_producer_consumer(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    AsciiFrameMailbox *mailbox = vcatAsciiFrameMailboxCreate();
    MailboxStressContext context = {.mailbox = mailbox};

    pthread_t producer;
    pthread_t consumer;
    munit_assert_int(pthread_create(&producer, NULL, mailboxStressProducer, &context), ==, 0);
    munit_assert_int(pthread_create(&consumer, NULL, mailboxStressConsumer, &context), ==, 0);

    munit_assert_int(pthread_join(producer, NULL), ==, 0);
    munit_assert_int(pthread_join(consumer, NULL), ==, 0);

    vcatAsciiFrameMailboxFree(mailbox);
    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
    {(char *)"/fit-dimensions/rejects-nothing-to-fit",            test_fit_rejects_nothing_to_fit,                         NULL, NULL,
     MUNIT_TEST_OPTION_NONE,                                                                                                                                   NULL},
    {(char *)"/fit-dimensions/rejects-non-positive-ratio",        test_fit_rejects_non_positive_ratio,                     NULL, NULL,
     MUNIT_TEST_OPTION_NONE,                                                                                                                                   NULL},
    {(char *)"/fit-dimensions/exact-ratio-is-hit",                test_fit_exact_ratio_is_hit,                             NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL                                                                                                                                                          },
    {(char *)"/fit-dimensions/prefers-largest-within-tolerance",  test_fit_prefers_largest_within_tolerance,               NULL, NULL,
     MUNIT_TEST_OPTION_NONE,                                                                                                                                   NULL},
    {(char *)"/fit-dimensions/falls-back-to-least-wrong",         test_fit_falls_back_to_least_wrong,                      NULL, NULL,
     MUNIT_TEST_OPTION_NONE,                                                                                                                                   NULL},
    {(char *)"/fit-dimensions/widescreen-into-standard-terminal", test_fit_widescreen_into_standard_terminal,              NULL,
     NULL,                                                                                                                             MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/mailbox/empty-acquires-nothing",                   test_mailbox_empty_acquires_nothing,                     NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL                                                                                                                                                          },
    {(char *)"/mailbox/acquire-latest-returns-published",         test_mailbox_acquire_latest_returns_the_published_frame, NULL,
     NULL,                                                                                                                             MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/mailbox/publish-twice-keeps-newest",               test_mailbox_publish_twice_keeps_newest,                 NULL, NULL,
     MUNIT_TEST_OPTION_NONE,                                                                                                                                   NULL},
    {(char *)"/mailbox/displaced-frame-recycled",                 test_mailbox_displaced_frame_recycled,                   NULL, NULL,
     MUNIT_TEST_OPTION_NONE,                                                                                                                                   NULL},
    {(char *)"/mailbox/release-order-fifo",                       test_mailbox_release_order_fifo,                         NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/mailbox/every-frame-accounted-for",                test_mailbox_every_frame_accounted_for,                  NULL, NULL,
     MUNIT_TEST_OPTION_NONE,                                                                                                                                   NULL},
    {(char *)"/mailbox/null-safety",                              test_mailbox_null_safety,                                NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/mailbox/concurrent-producer-consumer",             test_mailbox_concurrent_producer_consumer,               NULL, NULL,
     MUNIT_TEST_OPTION_NONE,                                                                                                                                   NULL},
    {(char *)"/mailbox/concurrent-producer-consumer",             test_mailbox_concurrent_producer_consumer,               NULL, NULL,
     MUNIT_TEST_OPTION_NONE,                                                                                                                                   NULL},
    {(char *)"/color/srgb-to-linear-bounds",                      test_color_srgb_to_linear_bounds,                        NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL                                                                                                                                                          },
    {(char *)"/color/srgb-round-trip",                            test_color_srgb_round_trip,                              NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/color/average-black-white-is-bright",              test_color_average_black_white_is_bright,                NULL, NULL,
     MUNIT_TEST_OPTION_NONE,                                                                                                                                   NULL},
    {(char *)"/color/average-gray-stays-gray",                    test_color_average_gray_stays_gray,                      NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL                                                                                                                                                          },
    {(char *)"/color/palette-corners",                            test_color_palette_corners,                              NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/color/palette-cache-cross-thread",                 test_color_palette_cache_cross_thread,                   NULL, NULL,
     MUNIT_TEST_OPTION_NONE,                                                                                                                                   NULL},
    {(char *)"/color/luminance-weights",                          test_color_luminance_weights,                            NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/color/mode-names",                                 test_color_mode_names,                                   NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL,                                                        NULL,                                                    NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}
};

static const MunitSuite test_suite = {(char *)"vcat-common", test_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)])
{
    return munit_suite_main(&test_suite, NULL, argc, argv);
}
