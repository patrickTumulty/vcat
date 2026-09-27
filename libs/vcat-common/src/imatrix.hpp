
#pragma once

template <typename T> class imatrix
{
  public:
    virtual T get(int x, int y) const = 0;
    virtual void set(T v, int x, int y) = 0;
    virtual void resize(int height, int width) = 0;
    virtual int height() const = 0;
    virtual int width() const = 0;
};
