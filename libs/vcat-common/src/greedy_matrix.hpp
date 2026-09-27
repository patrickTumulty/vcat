

#pragma once

#include "imatrix.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

template <typename T> class greedy_matrix : public imatrix<T>
{
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

    greedy_matrix(const greedy_matrix<T> &other)
        : _data(nullptr), _mat(nullptr), _height(0), _width(0), _allocHeight(0), _allocWidth(0), _allocBytes(0)
    {
        allocateMat(other.height(), other.width());
        int row = sizeof(T) * _width;
        for (int i = 0; i < _height; i++)
        {
            memcpy(_mat[i], other._mat[i], row);
        }
    }

    greedy_matrix<T> &operator=(const greedy_matrix<T> &other)
    {
        if (this == &other) // Prevent self-assignment
        {
            return *this;
        }
        allocateMat(other.height(), other.width());
        int row_bytes = sizeof(T) * _width;
        for (int i = 0; i < _height; i++)
        {
            memcpy(_mat[i], other._mat[i], row_bytes);
        }
        return *this;
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

    void copy_from(const imatrix<T> &destination) override
    {
        if (auto *derived = dynamic_cast<const greedy_matrix<T> *>(&destination))
        {
            *this = *derived; // Triggers the copy assignment operator
        }
        else
        {
            throw std::invalid_argument("greedy_matrix::copy_from - destination is not a greedy_matrix instance.");
        }
    }

    uint8_t *_data;
    T **_mat;
    int _height;
    int _width;
    int _allocHeight;
    int _allocWidth;
    int _allocBytes;
};
