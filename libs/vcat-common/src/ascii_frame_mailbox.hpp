
#pragma once

#include "matrix.h"
#include <mutex>
#include <optional>
#include <queue>

class IAsciiFrameMailboxReader
{
  public:
    virtual ~IAsciiFrameMailboxReader() = default;

    virtual std::optional<matrix_i8_t> acquireLatest() = 0;
    virtual void release(matrix_i8_t obj) = 0;
};

class IAsciiFrameMailboxWriter
{
  public:
    virtual ~IAsciiFrameMailboxWriter() = default;

    virtual std::optional<matrix_i8_t> acquireFree() = 0;
    virtual void publish(matrix_i8_t obj) = 0;
};

/// Hand-off point for ASCII frames between the converter (writer) and the renderer (reader).
///
/// The mailbox owns every frame it has been given: a frame handed out by acquireLatest() or
/// acquireFree() is only borrowed, and has to come back through release() or publish() so the
/// mailbox can free it. Any frame still held when the mailbox is destroyed is freed with it.
class AsciiFrameMailbox : public IAsciiFrameMailboxReader, public IAsciiFrameMailboxWriter
{
  public:
    AsciiFrameMailbox() = default;
    ~AsciiFrameMailbox() override;

    std::optional<matrix_i8_t> acquireLatest() override;
    void release(matrix_i8_t obj) override;
    std::optional<matrix_i8_t> acquireFree() override;
    void publish(matrix_i8_t obj) override;

  private:
    std::mutex _availableMutex;
    std::queue<matrix_i8_t> _available;
    std::mutex _latestMutex;
    std::optional<matrix_i8_t> _latest;
};
