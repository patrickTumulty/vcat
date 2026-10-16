
#include "ascii_frame_mailbox.hpp"

AsciiFrameMailbox::~AsciiFrameMailbox()
{
    {
        std::scoped_lock<std::mutex> lock(_latestMutex);
        matrix_free(_latest.value_or(nullptr));
        _latest.reset();
    }
    {
        std::scoped_lock<std::mutex> lock(_availableMutex);
        while (!_available.empty())
        {
            matrix_free(_available.front());
            _available.pop();
        }
    }
}

std::optional<matrix_i8_t> AsciiFrameMailbox::acquireLatest()
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

void AsciiFrameMailbox::release(matrix_i8_t obj)
{
    std::scoped_lock<std::mutex> lock(_availableMutex);
    _available.push(obj);
}

std::optional<matrix_i8_t> AsciiFrameMailbox::acquireFree()
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

void AsciiFrameMailbox::publish(matrix_i8_t obj)
{
    // Only the newest frame is kept: a consumer that falls behind gets the latest
    // data on its next acquire instead of draining a growing backlog of stale frames.
    matrix_i8_t displaced = nullptr;
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
