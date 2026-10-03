
#pragma once

#include "imatrix.hpp"
// #include "tui_session.hpp"
#include "recycling_queue.hpp"
#include "utils.hpp"
#include <cstdint>
#include <memory>
#include <mutex>

struct pixel
{
#pragma pack(push, 1)
    uint8_t r;
    uint8_t g;
    uint8_t b;
#pragma pack(pop)

    /**
     * @brief Converts an RGB color to its perceptually accurate luminance (grayscale) value.
     *
     * This function calculates brightness using the ITU-R BT.709 standard coefficients,
     * which are optimized for modern sRGB digital displays and HDTVs. It weights the
     * channels based on human visual sensitivity, prioritizing green over red and blue.
     *
     * Formula: 0.2126 * r + 0.7152 * g + 0.0722 * b
     *
     * @return The calculated luminance value as a float, ranging from 0.0f (darkest) to 1.0f (brightest).
     */
    float luminance() const
    {
        return (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f;
    }
};

class Video2AsciiConverter
{
  public:
    Video2AsciiConverter();

    void processPixelBuffer(const imatrix<pixel> &buffer);
    void updateVideoBounds(Rectangle newSize);
    std::shared_ptr<IRecyclingQueueReader<imatrix<char>>> getAsciiDataQueue();

  private:
    float averagePixelsLuminance(int x, int y, int height, int width, const imatrix<pixel> &buffer);

    std::shared_ptr<RecyclingQueue<imatrix<char>>> _recyclingQueue;
    std::shared_ptr<IRecyclingQueueWriter<imatrix<char>>> _recyclingQueueWriter;

    Rectangle _pixelDimensions{};
    Rectangle _terminalSize{};
    Rectangle _asciiBounds{};
    Rectangle _prevAsciiBounds{};
    bool _terminalSizeChange = false;
    // std::shared_ptr<imatrix<char>> _asciiData;
    float _videoRatio = 1.0;
};
