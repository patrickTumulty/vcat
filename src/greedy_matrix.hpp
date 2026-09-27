

#pragma once

#include "glib.h"
#include "imatrix.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

template <typename T> class greedy_matrix : public imatrix<T>
{
  public:
    explicit greedy_matrix(int height, int width) : _height(height), _width(width), _allocHeight(0), _allocWidth(0)
    {
        allocateMat(height, width);
        clear();
    }

    ~greedy_matrix()
    {
        if (_data != nullptr)
        {
            free(_data);
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
        int bytes = (sizeof(T *) * height) + (sizeof(T) * height * width);
        if (bytes == _allocBytes)
        {
            return _mat;
        }
        if (bytes > _allocBytes)
        {
            if (_data != nullptr)
            {
                free(_data);
                _mat = nullptr;
            }
            _data = (uint8_t *)malloc(bytes);
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
        for (int i = 0; i < height; i++)
        {
            _mat[i] = (T *)ptr;
            ptr += (sizeof(T) * width);
        }
        _height = height;
        _width = width;
        return _mat;
    }

    T **_mat = nullptr;
    int _height = 0;
    int _width = 0;
    uint8_t *_data = nullptr;
    int _allocBytes = 0;
    int _allocHeight = 0;
    int _allocWidth = 0;
};
