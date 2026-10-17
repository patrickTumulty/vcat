
#include "ascii_frame_mailbox.h"
#include "greedy_matrix.h"
#include "matrix.h"
#include "munit.h"
#include "utils.h"
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <string.h>

/* Frames are distinguished by a marker written into cell (0,0). Each frame is
 * created through the crox matrix API so the mailbox has something real to
 * own and free. */
static matrix_i8_t makeFrame(char marker)
{
    matrix_i8_t frame = NULL;
    if (matrix_init_greedy_i8(&frame, 1, 1) != RC_OK)
    {
        return NULL;
    }
    matrix_set_i8(frame, 0, 0, marker);
    return frame;
}

static char markerOf(matrix_i8_t frame)
{
    return (char)matrix_get_i8(frame, 0, 0);
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

/* The octets pack most significant first. */
static MunitResult test_ip_address_packs_msb_first(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    munit_assert_uint32(ipAddress((Ip){127, 0, 0, 1}), ==, 0x7F000001u);
    munit_assert_uint32(ipAddress((Ip){192, 168, 1, 7}), ==, 0xC0A80107u);
    munit_assert_uint32(ipAddress((Ip){0, 0, 0, 0}), ==, 0u);

    return MUNIT_OK;
}

/* localhost() is the loopback address. */
static MunitResult test_ip_localhost_is_loopback(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    munit_assert_uint32(ipAddress(ipLocalhost()), ==, ipAddress((Ip){127, 0, 0, 1}));

    char rendered[16];
    ipToStr(ipLocalhost(), rendered);
    munit_assert_string_equal(rendered, "127.0.0.1");

    return MUNIT_OK;
}

/* Dotted decimal covers the corner values. */
static MunitResult test_ip_to_str_renders_dotted_decimal(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    char rendered[16];

    ipToStr((Ip){0, 0, 0, 0}, rendered);
    munit_assert_string_equal(rendered, "0.0.0.0");

    ipToStr((Ip){255, 255, 255, 255}, rendered);
    munit_assert_string_equal(rendered, "255.255.255.255");

    ipToStr((Ip){8, 8, 4, 4}, rendered);
    munit_assert_string_equal(rendered, "8.8.4.4");

    return MUNIT_OK;
}

/* Acquiring from an empty mailbox hands out nothing. */
static MunitResult test_mailbox_empty_acquires_nothing(const MunitParameter params[], void *user_data)
{
    (void)params;
    (void)user_data;

    AsciiFrameMailbox *mailbox = vcatAsciiFrameMailboxCreate();
    munit_assert_not_null(mailbox);

    matrix_i8_t frame = NULL;
    munit_assert_false(vcatAsciiFrameMailboxAcquireLatest(mailbox, &frame));
    munit_assert_false(vcatAsciiFrameMailboxAcquireFree(mailbox, &frame));
    munit_assert_null(frame);

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
    matrix_i8_t frame = makeFrame('a');
    munit_assert_not_null(frame);

    vcatAsciiFrameMailboxPublish(mailbox, frame);

    matrix_i8_t acquired = NULL;
    munit_assert_true(vcatAsciiFrameMailboxAcquireLatest(mailbox, &acquired));
    munit_assert_ptr_equal(acquired, frame);
    munit_assert_char(markerOf(acquired), ==, 'a');
    vcatAsciiFrameMailboxRelease(mailbox, acquired);

    matrix_i8_t drained = NULL;
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

    matrix_i8_t acquired = NULL;
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
    matrix_i8_t first = makeFrame('a');
    vcatAsciiFrameMailboxPublish(mailbox, first);
    vcatAsciiFrameMailboxPublish(mailbox, makeFrame('b'));

    matrix_i8_t recycled = NULL;
    munit_assert_true(vcatAsciiFrameMailboxAcquireFree(mailbox, &recycled));
    munit_assert_ptr_equal(recycled, first);
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
    matrix_i8_t first = makeFrame('a');
    matrix_i8_t second = makeFrame('b');

    vcatAsciiFrameMailboxRelease(mailbox, first);
    vcatAsciiFrameMailboxRelease(mailbox, second);

    matrix_i8_t acquiredFirst = NULL;
    matrix_i8_t acquiredSecond = NULL;
    munit_assert_true(vcatAsciiFrameMailboxAcquireFree(mailbox, &acquiredFirst));
    munit_assert_true(vcatAsciiFrameMailboxAcquireFree(mailbox, &acquiredSecond));
    munit_assert_ptr_equal(acquiredFirst, first);
    munit_assert_ptr_equal(acquiredSecond, second);

    matrix_i8_t drained = NULL;
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
    matrix_i8_t a = makeFrame('a');
    matrix_i8_t b = makeFrame('b');
    matrix_i8_t c = makeFrame('c');

    vcatAsciiFrameMailboxPublish(mailbox, a);
    vcatAsciiFrameMailboxPublish(mailbox, b);
    vcatAsciiFrameMailboxPublish(mailbox, c);

    matrix_i8_t latest = NULL;
    munit_assert_true(vcatAsciiFrameMailboxAcquireLatest(mailbox, &latest));
    munit_assert_ptr_equal(latest, c);
    vcatAsciiFrameMailboxRelease(mailbox, latest);

    matrix_i8_t first = NULL;
    matrix_i8_t second = NULL;
    matrix_i8_t third = NULL;
    munit_assert_true(vcatAsciiFrameMailboxAcquireFree(mailbox, &first));
    munit_assert_true(vcatAsciiFrameMailboxAcquireFree(mailbox, &second));
    munit_assert_true(vcatAsciiFrameMailboxAcquireFree(mailbox, &third));
    munit_assert_ptr_equal(first, a);
    munit_assert_ptr_equal(second, b);
    munit_assert_ptr_equal(third, c);

    matrix_i8_t drained = NULL;
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

    matrix_i8_t frame = NULL;
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
        matrix_i8_t frame = NULL;
        if (!vcatAsciiFrameMailboxAcquireFree(context->mailbox, &frame))
        {
            frame = makeFrame(0);
        }
        matrix_set_i8(frame, 0, 0, marker);
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
        matrix_i8_t frame = NULL;
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
    {(char *)"/fit-dimensions/rejects-nothing-to-fit", test_fit_rejects_nothing_to_fit, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/fit-dimensions/rejects-non-positive-ratio", test_fit_rejects_non_positive_ratio, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/fit-dimensions/exact-ratio-is-hit", test_fit_exact_ratio_is_hit, NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char *)"/fit-dimensions/prefers-largest-within-tolerance", test_fit_prefers_largest_within_tolerance, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/fit-dimensions/falls-back-to-least-wrong", test_fit_falls_back_to_least_wrong, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/fit-dimensions/widescreen-into-standard-terminal", test_fit_widescreen_into_standard_terminal, NULL,
     NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/ip/address-packs-msb-first", test_ip_address_packs_msb_first, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/ip/localhost-is-loopback", test_ip_localhost_is_loopback, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/ip/to-str-dotted-decimal", test_ip_to_str_renders_dotted_decimal, NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char *)"/mailbox/empty-acquires-nothing", test_mailbox_empty_acquires_nothing, NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char *)"/mailbox/acquire-latest-returns-published", test_mailbox_acquire_latest_returns_the_published_frame, NULL,
     NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/mailbox/publish-twice-keeps-newest", test_mailbox_publish_twice_keeps_newest, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/mailbox/displaced-frame-recycled", test_mailbox_displaced_frame_recycled, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/mailbox/release-order-fifo", test_mailbox_release_order_fifo, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/mailbox/every-frame-accounted-for", test_mailbox_every_frame_accounted_for, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/mailbox/null-safety", test_mailbox_null_safety, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/mailbox/concurrent-producer-consumer", test_mailbox_concurrent_producer_consumer, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {(char *)"vcat-common", test_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)])
{
    return munit_suite_main(&test_suite, NULL, argc, argv);
}
