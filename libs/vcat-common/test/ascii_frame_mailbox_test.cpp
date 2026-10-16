
#include "ascii_frame_mailbox.hpp"
#include "greedy_matrix.h"
#include "matrix.h"
#include <optional>
#include <thread>

#include <gtest/gtest.h>

namespace
{

/// Frames are distinguished by a marker written into cell (0,0). Each frame is created
/// through the crox matrix API so the mailbox has something real to own and free.
matrix_i8_t makeFrame(char marker)
{
    matrix_i8_t frame = nullptr;
    if (matrix_init_greedy_i8(&frame, 1, 1) != RC_OK)
    {
        return nullptr;
    }
    matrix_set_i8(frame, 0, 0, marker);
    return frame;
}

char markerOf(matrix_i8_t frame)
{
    return static_cast<char>(matrix_get_i8(frame, 0, 0));
}

TEST(AsciiFrameMailbox, AcquireLatestOnAnEmptyMailboxReturnsNothing)
{
    AsciiFrameMailbox mailbox;

    EXPECT_EQ(mailbox.acquireLatest(), std::nullopt);
}

TEST(AsciiFrameMailbox, AcquireFreeOnAnEmptyMailboxReturnsNothing)
{
    AsciiFrameMailbox mailbox;

    EXPECT_EQ(mailbox.acquireFree(), std::nullopt);
}

TEST(AsciiFrameMailbox, AcquireLatestReturnsThePublishedFrame)
{
    AsciiFrameMailbox mailbox;
    auto frame = makeFrame('a');

    mailbox.publish(frame);
    auto acquired = mailbox.acquireLatest();

    ASSERT_TRUE(acquired.has_value());
    EXPECT_EQ(*acquired, frame);
    EXPECT_EQ(markerOf(*acquired), 'a');
    mailbox.release(*acquired);
}

TEST(AsciiFrameMailbox, AcquireLatestDrainsTheMailbox)
{
    AsciiFrameMailbox mailbox;
    mailbox.publish(makeFrame('a'));

    auto acquired = mailbox.acquireLatest();
    ASSERT_TRUE(acquired.has_value());
    mailbox.release(*acquired);
    EXPECT_EQ(mailbox.acquireLatest(), std::nullopt) << "a published frame is handed out exactly once";
}

TEST(AsciiFrameMailbox, PublishTwiceKeepsOnlyTheNewestFrame)
{
    AsciiFrameMailbox mailbox;
    mailbox.publish(makeFrame('a'));
    mailbox.publish(makeFrame('b'));

    auto acquired = mailbox.acquireLatest();

    ASSERT_TRUE(acquired.has_value());
    EXPECT_EQ(markerOf(*acquired), 'b') << "a consumer that falls behind must get the latest frame";
    mailbox.release(*acquired);
}

TEST(AsciiFrameMailbox, ADisplacedFrameGoesBackToTheFreePool)
{
    AsciiFrameMailbox mailbox;
    auto first = makeFrame('a');
    mailbox.publish(first);
    mailbox.publish(makeFrame('b'));

    auto recycled = mailbox.acquireFree();

    ASSERT_TRUE(recycled.has_value());
    EXPECT_EQ(*recycled, first) << "the frame displaced from the latest slot is recycled, not dropped";
    mailbox.release(*recycled);
}

TEST(AsciiFrameMailbox, ReleasedFramesComeBackFromAcquireFreeInOrder)
{
    AsciiFrameMailbox mailbox;
    auto first = makeFrame('a');
    auto second = makeFrame('b');

    mailbox.release(first);
    mailbox.release(second);

    auto acquiredFirst = mailbox.acquireFree();
    auto acquiredSecond = mailbox.acquireFree();

    ASSERT_TRUE(acquiredFirst.has_value());
    ASSERT_TRUE(acquiredSecond.has_value());
    EXPECT_EQ(*acquiredFirst, first);
    EXPECT_EQ(*acquiredSecond, second);
    EXPECT_EQ(mailbox.acquireFree(), std::nullopt);
    mailbox.release(*acquiredFirst);
    mailbox.release(*acquiredSecond);
}

TEST(AsciiFrameMailbox, EveryFrameStaysAccountedFor)
{
    // Publish three frames, read the newest and release it: all three frames must come back
    // through the free pool, each exactly once.
    AsciiFrameMailbox mailbox;
    auto a = makeFrame('a');
    auto b = makeFrame('b');
    auto c = makeFrame('c');

    mailbox.publish(a);
    mailbox.publish(b);
    mailbox.publish(c);

    auto latest = mailbox.acquireLatest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(*latest, c);
    mailbox.release(*latest);

    auto first = mailbox.acquireFree();
    auto second = mailbox.acquireFree();
    auto third = mailbox.acquireFree();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(*first, a);
    EXPECT_EQ(*second, b);
    EXPECT_EQ(*third, c);
    EXPECT_EQ(mailbox.acquireFree(), std::nullopt) << "no frame may be handed out twice";
    mailbox.release(*first);
    mailbox.release(*second);
    mailbox.release(*third);
}

TEST(AsciiFrameMailbox, IsUsableThroughTheReaderAndWriterInterfaces)
{
    AsciiFrameMailbox mailbox;
    IAsciiFrameMailboxWriter *writer = &mailbox;
    IAsciiFrameMailboxReader *reader = &mailbox;
    auto frame = makeFrame('a');

    writer->publish(frame);
    auto acquired = reader->acquireLatest();

    ASSERT_TRUE(acquired.has_value());
    EXPECT_EQ(*acquired, frame);
    reader->release(*acquired);
    auto recycled = writer->acquireFree();
    ASSERT_TRUE(recycled.has_value());
    writer->publish(*recycled);
    auto drained = reader->acquireLatest();
    ASSERT_TRUE(drained.has_value());
    reader->release(*drained);
}

TEST(AsciiFrameMailbox, SurvivesAConcurrentProducerAndConsumer)
{
    // The writer publishes frames while the reader drains them at its own pace. The reader must
    // eventually observe the newest frame, however many it skipped meanwhile.
    constexpr char kLastMarker = 99;
    AsciiFrameMailbox mailbox;
    IAsciiFrameMailboxWriter *writer = &mailbox;
    IAsciiFrameMailboxReader *reader = &mailbox;

    std::thread producer(
        [writer]
        {
            for (char marker = 0; marker <= kLastMarker; marker++)
            {
                auto acquired = writer->acquireFree();
                matrix_i8_t frame = acquired.has_value() ? *acquired : makeFrame(0);
                matrix_set_i8(frame, 0, 0, marker);
                writer->publish(frame);
            }
        });

    std::optional<char> lastSeen;
    std::thread consumer(
        [&]
        {
            int spinsLeft = 1000000;
            while (lastSeen != kLastMarker && spinsLeft-- > 0)
            {
                auto frame = reader->acquireLatest();
                if (!frame.has_value())
                {
                    std::this_thread::yield();
                    continue;
                }
                lastSeen = markerOf(*frame);
                reader->release(*frame);
            }
        });

    producer.join();
    consumer.join();

    ASSERT_TRUE(lastSeen.has_value()) << "the consumer never received a frame";
    EXPECT_EQ(lastSeen, kLastMarker) << "the consumer must end up with the newest frame";
}

} // namespace
