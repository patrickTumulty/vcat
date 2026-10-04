
#include "ascii_frame_mailbox.hpp"

std::optional<std::shared_ptr<imatrix<char>>> AsciiFrameMailbox::acquireLatest()
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

void AsciiFrameMailbox::release(std::shared_ptr<imatrix<char>> obj)
{
    std::scoped_lock<std::mutex> lock(_availableMutex);
    _available.push(obj);
}

std::optional<std::shared_ptr<imatrix<char>>> AsciiFrameMailbox::acquireFree()
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

void AsciiFrameMailbox::publish(std::shared_ptr<imatrix<char>> obj)
{
    // Only the newest frame is kept: a consumer that falls behind gets the latest
    // data on its next acquire instead of draining a growing backlog of stale frames.
    std::shared_ptr<imatrix<char>> displaced = nullptr;
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
