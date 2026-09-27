
#include "video2ascii_converter.hpp"
#include "greedy_matrix.hpp"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#include "utils.hpp"
#include <cmath>
#include <mutex>
#include <ncurses.h>

class AsciiGradient
{
  public:
    AsciiGradient(const char *gradient) : _gradient(gradient), _gradientLen(strlen(_gradient))
    {
    }

    char get(float value) const
    {
        if (_inverted)
        {
            value = 1 - value;
        }
        int offset = std::min(_gradientLen - 1, static_cast<int>(_gradientLen * value));
        return _gradient[offset];
    }

    void invert()
    {
        _inverted = !_inverted;
    }

  private:
    const char *_gradient;
    int _gradientLen;
    bool _inverted = false;
};

const char *GRADIENT1 = "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. ";
const char *GRADIENT2 = "@#*+=- ";

AsciiGradient gradient(GRADIENT2);

const float LUMINANCE_GAMMA = 2.2f;

Video2AsciiConverter::Video2AsciiConverter() : _asciiData(std::make_unique<greedy_matrix<char>>(25, 50)), _asciiDataLock{}
{
    gradient.invert();
}

void Video2AsciiConverter::processPixelBuffer(const imatrix<pixel> &buffer)
{
    bool resized = false;
    if (_videoHeight != buffer.height() || _videoWidth != buffer.width())
    {
        spdlog::info("Video buffer size change: {}x{} -> {}x{}", _videoWidth, _videoHeight, buffer.width(),
                     buffer.height());

        _videoHeight = buffer.height();
        _videoWidth = buffer.width();
        _videoRatio = _videoWidth / static_cast<float>(_videoHeight);

        auto rec = fitDimensionsToRatio(_terminalSize, _videoRatio * 2.0);
        int height = _asciiData->height();
        int width = _asciiData->width();
        {
            std::scoped_lock lock(_asciiDataLock);
            _asciiData->resize(rec.height, rec.width);
            resized = true;
        }
        spdlog::info("Resizing ascii buffer: video change {}x{} -> {}x{}", width, height, _asciiData->width(),
                     _asciiData->height());
    }

    if (_terminalSizeChange)
    {
        auto rec = fitDimensionsToRatio(_terminalSize, _videoRatio * 2.0);
        int height = _asciiData->height();
        int width = _asciiData->width();
        {
            std::scoped_lock lock(_asciiDataLock);
            _asciiData->resize(rec.height, rec.width);
            _terminalSizeChange = false;
            resized = true;
        }
        spdlog::info("Resizing ascii buffer: terminal change {}x{} -> {}x{}", width, height, _asciiData->width(),
                     _asciiData->height());
    }

    if (resized && _asciiData->width() > 0 && _asciiData->height() > 0)
    {
        _pixelStepWidth = _videoWidth / _asciiData->width();
        _pixelStepHeight = _videoHeight / _asciiData->height();
    }

    for (int i = 0; i < _asciiData->height(); i++)
    {
        int pixelIdxX = 0;
        int pixelIdxY = i * _pixelStepHeight;
        for (int j = 0; j < _asciiData->width(); j++)
        {
            float luminance = averagePixelsLuminance(pixelIdxX, pixelIdxY, _pixelStepHeight, _pixelStepWidth, buffer);
            luminance = std::pow(luminance, 1.0f / LUMINANCE_GAMMA); // gamma: spread mid-tones across ramp
            _asciiData->set(gradient.get(luminance), j, i);
            pixelIdxX += _pixelStepWidth;
        }
    }
}

void Video2AsciiConverter::onTerminalUpdate()
{
    std::scoped_lock lock(_asciiDataLock);

    int offsetX = std::max(1, (_terminalSize.width - _asciiData->width()) / 2);
    int offsetY = std::max(0, (_terminalSize.height - _asciiData->height()) / 2);

    for (int i = 0; i < _asciiData->height(); i++)
    {
        for (int j = 0; j < _asciiData->width(); j++)
        {
            mvaddch(i + offsetY + 1, j + offsetX + 1, _asciiData->get(j, i));
        }
    }

    drawBox(offsetX, offsetY, _asciiData->height() + 1, _asciiData->width() + 1);
}

float Video2AsciiConverter::averagePixelsLuminance(int x, int y, int height, int width, const imatrix<pixel> &buffer)
{
    float total = height * width;
    if (total <= 0.0f)
    {
        return 0.0f;
    }
    float luminanceSum = 0.0f;
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            luminanceSum += buffer.get(j + x, i + y).luminance();
        }
    }
    return luminanceSum / total;
}

void Video2AsciiConverter::onTerminalSizeChange(Rectangle newSize)
{
    _terminalSize = newSize;
    _terminalSize.height -= 2;
    _terminalSize.width -= 2;
    spdlog::info("Terminal size change h={} w={}", newSize.height, newSize.width);
    _terminalSizeChange = true;
}
