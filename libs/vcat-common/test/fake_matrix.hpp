
#pragma once

#include "imatrix.hpp"
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace vcat::test
{

/// Value type used to show the interface is agnostic to what a cell holds. Trivial on purpose,
/// like the pixel struct greedy_matrix is used with, since it fills cells with memset.
struct cell
{
    uint8_t byte;
    int16_t small;

    friend bool operator==(const cell &lhs, const cell &rhs) = default;
};

/// A cell whose every byte is set to `byte`, the way clear() and allocateMat() fill whole cells.
template <typename T> T filledCell(unsigned char byte)
{
    T filled{};
    unsigned char *bytes = reinterpret_cast<unsigned char *>(&filled);
    for (std::size_t i = 0; i < sizeof(T); i++)
    {
        bytes[i] = byte;
    }
    return filled;
}

/// What a greedy_matrix is expected to leave in a cell when it is built: a space in every byte.
template <typename T> T blankCell()
{
    return filledCell<T>(' ');
}

/// Deliberately naive imatrix implementation, independent of greedy_matrix, used to exercise the
/// interface itself. Cells live in one flat row-major vector, out of range reads yield a default
/// constructed value and out of range writes are dropped.
template <typename T> class flat_matrix : public imatrix<T>
{
  public:
    explicit flat_matrix(int height, int width) : _height(height), _width(width)
    {
        _data.assign(cellCount(height, width), T{});
    }

    int height() const override
    {
        return _height;
    }

    int width() const override
    {
        return _width;
    }

    T get(int x, int y) const override
    {
        if (!inBounds(x, y))
        {
            return T{};
        }
        return _data[index(x, y)];
    }

    void set(T value, int x, int y) override
    {
        if (!inBounds(x, y))
        {
            return;
        }
        _data[index(x, y)] = value;
    }

    void resize(int height, int width) override
    {
        _height = height;
        _width = width;
        _data.assign(cellCount(height, width), T{});
    }

  private:
    static std::size_t cellCount(int height, int width)
    {
        if (height <= 0 || width <= 0)
        {
            return 0;
        }
        return static_cast<std::size_t>(height) * static_cast<std::size_t>(width);
    }

    static std::size_t flatIndex(int x, int y, int width)
    {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
    }

    bool inBounds(int x, int y) const
    {
        return x >= 0 && x < _width && y >= 0 && y < _height;
    }

    std::size_t index(int x, int y) const
    {
        return flatIndex(x, y, _width);
    }

    int _height;
    int _width;
    std::vector<T> _data;
};

/// imatrix implementation that logs every call it receives, used to prove which member functions
/// the interface routes to an implementation and in what order.
template <typename T> class recording_matrix : public imatrix<T>
{
  public:
    explicit recording_matrix(int height, int width) : reportedHeight(height), reportedWidth(width)
    {
        calls.push_back(std::format("ctor({},{})", height, width));
    }

    mutable std::vector<std::string> calls;
    int reportedHeight;
    int reportedWidth;
    T value{};

    int height() const override
    {
        calls.push_back("height()");
        return reportedHeight;
    }

    int width() const override
    {
        calls.push_back("width()");
        return reportedWidth;
    }

    T get(int x, int y) const override
    {
        calls.push_back(std::format("get({},{})", x, y));
        return value;
    }

    void set(T newValue, int x, int y) override
    {
        calls.push_back(std::format("set({},{})", x, y));
        value = newValue;
    }

    void resize(int height, int width) override
    {
        calls.push_back(std::format("resize({},{})", height, width));
        reportedHeight = height;
        reportedWidth = width;
    }

    void clearCalls()
    {
        calls.clear();
    }
};

} // namespace vcat::test
