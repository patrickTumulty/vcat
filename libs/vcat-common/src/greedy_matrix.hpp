

#pragma once

#include "imatrix.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>

template <typename T> class greedy_matrix : public imatrix<T>
{
    // Cells are filled with memset and copied with memcpy, so only byte-like value types are safe.
    static_assert(std::is_trivially_copyable_v<T>,
                  "greedy_matrix stores cells with memcpy/memset, so T must be trivially copyable.");
    // The buffer comes from new uint8_t[], which cannot satisfy an over-aligned T.
    static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                  "greedy_matrix cannot store an over-aligned T.");

  public:
    explicit greedy_matrix(int height, int width)
        : _data(nullptr), _mat(nullptr), _height(height), _width(width), _allocHeight(0), _allocWidth(0), _allocBytes(0)
    {
        allocateMat(height, width);
        clear();
    }

    ~greedy_matrix()
    {
        if (_data != nullptr)
        {
            delete[] _data;
        }
        _data = nullptr;
        _mat = nullptr;
        _width = 0;
        _height = 0;
        _allocHeight = 0;
        _allocWidth = 0;
        _allocBytes = 0;
    }

    int height() const override
    {
        return _height;
    }

    int width() const override
    {
        return _width;
    }

    void resize(int height, int width) override
    {
        allocateMat(height, width);
    }

    T get(int x, int y) const override
    {
        if (!inBounds(x, y))
        {
            return {};
        }
        return _mat[y][x];
    }

    void set(T v, int x, int y) override
    {
        if (!inBounds(x, y))
        {
            return;
        }
        _mat[y][x] = v;
    }

  private:
    bool inBounds(int x, int y) const
    {
        return (x >= 0 && x < _width) && (y >= 0 && y < _height);
    }

    void clear()
    {
        for (int i = 0; i < _height; i++)
        {
            memset(_mat[i], ' ', sizeof(T) * _width);
        }
    }

    T **allocateMat(int height, int width)
    {
        size_t bytes = (sizeof(T *) * height) + (sizeof(T) * height * width);
        if (bytes == _allocBytes && _height == height && _width == width)
        {
            return _mat;
        }
        if (bytes > _allocBytes)
        {
            if (_data != nullptr)
            {
                delete[] _data;
                _mat = nullptr;
            }
            _data = new uint8_t[bytes];
            memset(_data, 0, bytes);
            _allocBytes = bytes;
            _allocHeight = height;
            _allocWidth = width;
        }
        else
        {
            _mat = nullptr;
            memset(_data, 0, bytes);
        }
        _mat = (T **)_data;
        uint8_t *ptr = _data;
        ptr += sizeof(T *) * height;
        int row = sizeof(T) * width;
        for (int i = 0; i < height; i++)
        {
            _mat[i] = (T *)ptr;
            ptr += row;
        }
        _height = height;
        _width = width;
        return _mat;
    }

    uint8_t *_data;
    T **_mat;
    int _height;
    int _width;
    int _allocHeight;
    int _allocWidth;
    std::size_t _allocBytes;
};
