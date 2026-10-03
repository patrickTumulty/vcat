
#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <queue>

template <typename T> class IRecyclingQueueReader
{
  public:
    virtual std::optional<std::shared_ptr<T>> acquireLatest() = 0;
    virtual void release(std::shared_ptr<T> obj) = 0;
};

template <typename T> class IRecyclingQueueWriter
{
  public:
    virtual std::optional<std::shared_ptr<T>> acquireFree() = 0;
    virtual void publish(std::shared_ptr<T> obj) = 0;
};

template <typename T> class RecyclingQueue : public IRecyclingQueueReader<T>, public IRecyclingQueueWriter<T>
{
  public:
    RecyclingQueue() : _availableMutex(), _available(), _latestMutex(), _latest()
    {
    }

    std::optional<std::shared_ptr<T>> acquireLatest() override
    {
        std::scoped_lock<std::mutex> lock(_latestMutex);
        if (!_latest.has_value())
        {
            return std::nullopt;
        }
        auto v = _latest.value();
        _latest.reset();
        return v;
    }

    void release(std::shared_ptr<T> obj) override
    {
        std::scoped_lock<std::mutex> lock(_availableMutex);
        _available.push(obj);
    }

    std::optional<std::shared_ptr<T>> acquireFree() override
    {
        std::scoped_lock<std::mutex> lock(_availableMutex);
        if (_available.empty())
        {
            return std::nullopt;
        }
        auto v = _available.front();
        _available.pop();
        return v;
    }

    void publish(std::shared_ptr<T> obj) override
    {
        // Only the newest frame is kept: a consumer that falls behind gets the latest
        // data on its next acquire instead of draining a growing backlog of stale frames.
        std::shared_ptr<T> displaced = nullptr;
        {
            std::scoped_lock<std::mutex> lock(_latestMutex);
            displaced = _latest.value_or(nullptr);
            _latest = obj;
        }
        if (displaced != nullptr)
        {
            release(displaced); // Lock released first: keeps lock ordering one-way.
        }
    }

  private:
    std::mutex _availableMutex;
    std::queue<std::shared_ptr<T>> _available;
    std::mutex _latestMutex;
    std::optional<std::shared_ptr<T>> _latest;
};
