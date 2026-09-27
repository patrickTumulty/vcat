
#pragma once

#include "imatrix.hpp"
#include "tui_session.hpp"
#include "utils.hpp"
#include <cstdint>
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

class Video2AsciiConverter : public ITUISessionListener
{
  public:
    Video2AsciiConverter();

    void processPixelBuffer(const imatrix<pixel> &buffer);

    void onTerminalUpdate() override;
    void onTerminalSizeChange(Rectangle newSize) override;

  private:
    // Cells of margin between the picture and the border box drawn around it.
    static constexpr int BORDER_MARGIN = 1;
    // Cells reserved off each axis of the terminal for that box. The picture is fitted to the
    // space left over, so it can never be wider than usable - 1 once the margin is added back,
    // and the box always lands on screen.
    static constexpr int BORDER_RESERVED = 2 * BORDER_MARGIN;

    float averagePixelsLuminance(int x, int y, int height, int width, const imatrix<pixel> &buffer);

    Rectangle _terminalSize{};
    bool _terminalSizeChange = false;
    std::unique_ptr<imatrix<char>> _asciiData;
    std::mutex _asciiDataLock;
    float _videoRatio = 1.0;
    int _videoHeight = 0;
    int _videoWidth = 0;
};
