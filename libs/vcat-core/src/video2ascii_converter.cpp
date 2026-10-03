
#include "video2ascii_converter.hpp"
#include "greedy_matrix.hpp"
#include "imatrix.hpp"
#include "logging.hpp"
#include "recycling_queue.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

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

Video2AsciiConverter::Video2AsciiConverter()
    : _recyclingQueue(std::make_shared<RecyclingQueue<imatrix<char>>>()),                                     //
      _recyclingQueueWriter(std::dynamic_pointer_cast<IRecyclingQueueWriter<imatrix<char>>>(_recyclingQueue)) //
{
    gradient.invert();
}

void Video2AsciiConverter::processPixelBuffer(const imatrix<pixel> &buffer)
{
    if (buffer.height() <= 0 || buffer.width() <= 0)
    {
        logging::error("invalid video bounds");
        return;
    }

    const bool videoChanged = (_videoWidth != buffer.width() || _videoHeight != buffer.height());

    if (videoChanged)
    {
        _videoWidth = buffer.width();
        _videoHeight = buffer.height();
        _videoRatio = _videoWidth / static_cast<float>(_videoHeight);
    }

    if (videoChanged || _terminalSizeChange)
    {
        // The grid is measured in character cells, so its aspect ratio is the video's ratio
        // scaled by the shape of a cell. A cell also has to cover at least one pixel, otherwise
        // it would be left with an empty sample window.
        Rectangle maxGrid = _videoBounds;
        maxGrid.width = std::min(maxGrid.width, _videoWidth);
        maxGrid.height = std::min(maxGrid.height, _videoHeight);

        const float targetRatio = _videoRatio * CHAR_CELL_ASPECT;
        _prevAsciiBounds = _asciiBounds;
        _asciiBounds = fitDimensionsToRatio(maxGrid, targetRatio);
        _terminalSizeChange = false;

        // The partition splits the frame into cells that differ by at most one pixel, so the
        // smallest and largest cell are just the frame size divided by the grid size, rounded.
        const int minCellW = _videoWidth / _asciiBounds.width;
        const int maxCellW = (_videoWidth + _asciiBounds.width - 1) / _asciiBounds.width;
        const int minCellH = _videoHeight / _asciiBounds.height;
        const int maxCellH = (_videoHeight + _asciiBounds.height - 1) / _asciiBounds.height;
        const float relError =
            std::fabs(_asciiBounds.width / static_cast<float>(_asciiBounds.height) - targetRatio) / targetRatio;

        logging::info("Ascii grid: {}x{} -> {}x{} ({}), ratio {:.4f} target {:.4f}, error {:.3f}%, cell {}-{}x{}-{}px, "
                      "cut off 0x0",
                      _prevAsciiBounds.width, _prevAsciiBounds.height, _asciiBounds.width, _asciiBounds.height,
                      videoChanged ? "video change" : "terminal change",
                      _asciiBounds.width / static_cast<float>(_asciiBounds.height), targetRatio, relError * 100.0f,
                      minCellW, maxCellW, minCellH, maxCellH);
    }

    std::optional<std::shared_ptr<imatrix<char>>> asciiBufferOpt = _recyclingQueueWriter->acquireFree();
    std::shared_ptr<imatrix<char>> asciiBuffer = nullptr;
    if (!asciiBufferOpt.has_value())
    {
        asciiBuffer = std::make_shared<greedy_matrix<char>>(_asciiBounds.height, _asciiBounds.width);
    }
    else
    {
        asciiBuffer = asciiBufferOpt.value();
    }

    logging::info("b {}x{}", asciiBuffer->width(), asciiBuffer->height());

    if (asciiBuffer->height() != _asciiBounds.height || asciiBuffer->width() != _asciiBounds.width)
    {
        asciiBuffer->resize(_asciiBounds.height, _asciiBounds.width);
    }

    const int gridWidth = asciiBuffer->width();
    const int gridHeight = asciiBuffer->height();
    if (gridWidth <= 0 || gridHeight <= 0 || _videoWidth <= 0 || _videoHeight <= 0)
    {
        return;
    }

    // Cell (i, j) covers the pixels [i * videoHeight / gridHeight, (i + 1) * videoHeight / gridHeight) rows
    // by [j * videoWidth / gridWidth, (j + 1) * videoWidth / gridWidth) columns. Those windows tile the
    // frame exactly, so every pixel lands in exactly one cell: nothing is cut off at the edges and
    // nothing is read out of bounds, no matter how close the grid ratio is to the video's. Only the
    // window sizes vary, by at most one pixel.
    for (int64_t i = 0; i < gridHeight; i++)
    {
        const int64_t y0 = i * _videoHeight / gridHeight;
        const int64_t y1 = (i + 1) * _videoHeight / gridHeight;
        for (int64_t j = 0; j < gridWidth; j++)
        {
            const int64_t x0 = j * _videoWidth / gridWidth;
            const int64_t x1 = (j + 1) * _videoWidth / gridWidth;
            float luminance = averagePixelsLuminance(x0, y0, y1 - y0, x1 - x0, buffer);
            luminance = std::pow(luminance, 1.0f / LUMINANCE_GAMMA); // gamma: spread mid-tones across ramp
            asciiBuffer->set(gradient.get(luminance), j, i);
        }
    }

    _recyclingQueueWriter->publish(asciiBuffer);
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

void Video2AsciiConverter::updateVideoBounds(Rectangle newSize)
{
    _videoBounds = newSize;
    logging::info("Video bounds size change h={} w={}", newSize.height, newSize.width);
    _terminalSizeChange = true;
}

std::shared_ptr<IRecyclingQueueReader<imatrix<char>>> Video2AsciiConverter::getAsciiDataQueue()
{
    return std::dynamic_pointer_cast<IRecyclingQueueReader<imatrix<char>>>(_recyclingQueue);
}
