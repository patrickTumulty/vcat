
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
        if (_latest.empty())
        {
            return std::nullopt;
        }
        auto v = _latest.front();
        _latest.pop();
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
        std::scoped_lock<std::mutex> lock(_latestMutex);
        _latest.push(obj);
    }

  private:
    std::mutex _availableMutex;
    std::queue<std::shared_ptr<T>> _available;
    std::mutex _latestMutex;
    std::queue<std::shared_ptr<T>> _latest;
};
