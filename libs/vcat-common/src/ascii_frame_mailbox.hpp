
#pragma once

#include "imatrix.hpp"
#include <memory>
#include <mutex>
#include <optional>
#include <queue>

class IAsciiFrameMailboxReader
{
  public:
    virtual std::optional<std::shared_ptr<imatrix<char>>> acquireLatest() = 0;
    virtual void release(std::shared_ptr<imatrix<char>> obj) = 0;
};

class IAsciiFrameMailboxWriter
{
  public:
    virtual std::optional<std::shared_ptr<imatrix<char>>> acquireFree() = 0;
    virtual void publish(std::shared_ptr<imatrix<char>> obj) = 0;
};

class AsciiFrameMailbox : public IAsciiFrameMailboxReader, public IAsciiFrameMailboxWriter
{
  public:
    AsciiFrameMailbox() = default;

    std::optional<std::shared_ptr<imatrix<char>>> acquireLatest() override;
    void release(std::shared_ptr<imatrix<char>> obj) override;
    std::optional<std::shared_ptr<imatrix<char>>> acquireFree() override;
    void publish(std::shared_ptr<imatrix<char>> obj) override;

  private:
    std::mutex _availableMutex;
    std::queue<std::shared_ptr<imatrix<char>>> _available;
    std::mutex _latestMutex;
    std::optional<std::shared_ptr<imatrix<char>>> _latest;
};
