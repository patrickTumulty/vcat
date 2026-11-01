
#include "video2ascii_converter.h"
#include "ascii_gradients.h"
#include "greedy_matrix.h"
#include "logging_facade.h"
#include "matrix.h"
#include "utils.h"
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief One character ramp and how to read it.
 *
 * The terminal is usually dark-on-light or light-on-dark; invert() flips which
 * end of the ramp the darkest pixels map to. The flag is atomic: the TUI
 * thread flips it while the GStreamer streaming thread reads it.
 */
typedef struct AsciiGradientData
{
    AsciiGradient gradient;
    atomic_bool inverted;
} AsciiGradientData;

/**
 * @brief Converts an RGB color to its perceptually accurate luminance (grayscale) value.
 *
 * This function calculates brightness using the ITU-R BT.709 standard coefficients,
 * which are optimized for modern sRGB digital displays and HDTVs. It weights the
 * channels based on human visual sensitivity, prioritizing green over red and blue.
 *
 * Formula: 0.2126 * r + 0.7152 * g + 0.0722 * b
 *
 * @return The calculated luminance value as a float, ranging from 0.0f (darkest) to 1.0f
 * (brightest).
 */
static inline float pixelLuminance(const Pixel *p)
{
    return (0.2126f * p->r + 0.7152f * p->g + 0.0722f * p->b) / 255.0f;
}

static char gradientGet(const AsciiGradientData *g, float value)
{
    if (atomic_load(&g->inverted))
    {
        value = 1 - value;
    }
    int offset = g->gradient.gradientLen - 1;
    const int scaled = (int)(g->gradient.gradientLen * value);
    if (scaled < offset)
    {
        offset = scaled;
    }
    return g->gradient.gradient[offset];
}

static void gradientInvert(AsciiGradient *g)
{
    atomic_store(&g->inverted, !atomic_load(&g->inverted));
}

#define LUMINANCE_GAMMA 2.2f

struct Video2AsciiConverter
{
    AsciiFrameMailbox *frameMailbox;

    Rectangle frameSizePixels;
    Rectangle terminalSize;
    Rectangle asciiBounds;
    Rectangle prevAsciiBounds;
    bool terminalSizeChange;
    float videoRatio;
    AsciiGradient gradient;
};

Video2AsciiConverter *vcatVideo2AsciiConverterCreate(void)
{
    AsciiFrameMailbox *frameMailbox = vcatAsciiFrameMailboxCreate();
    if (frameMailbox == NULL)
    {
        lfLogError("unable to allocate ascii frame mailbox");
        return NULL;
    }

    Video2AsciiConverter *converter = calloc(1, sizeof(*converter));
    if (converter == NULL)
    {
        vcatAsciiFrameMailboxFree(frameMailbox);
        lfLogError("unable to allocate video2ascii converter");
        return NULL;
    }

    converter->frameMailbox = frameMailbox;
    converter->videoRatio = 1.0f;

    converter->gradient.gradient = GRADIENT2;
    converter->gradient.gradientLen = (int)strlen(GRADIENT2);
    atomic_init(&converter->gradient.inverted, false);

    gradientInvert(&converter->gradient);

    return converter;
}

void vcatVideo2AsciiConverterFree(Video2AsciiConverter *converter)
{
    if (converter == NULL)
    {
        return;
    }
    vcatAsciiFrameMailboxFree(converter->frameMailbox);
    free(converter);
}

AsciiFrameMailbox *vcatVideo2AsciiConverterGetFrameMailbox(Video2AsciiConverter *converter)
{
    return converter != NULL ? converter->frameMailbox : NULL;
}

void vcatVideo2AsciiConverterInvertGradient(Video2AsciiConverter *converter)
{
    if (converter != NULL)
    {
        gradientInvert(&converter->gradient);
    }
}

/**
 * @brief Average luminance of the window [x, x + width) * [y, y + height) in `buffer`.
 */
static float averagePixelsLuminance(int x, int y, int height, int width, matrix_t buffer)
{
    const float total = (float)(height * width);
    if (total <= 0.0f)
    {
        return 0.0f;
    }

    float luminanceSum = 0.0f;
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            const Pixel *p = matrix_get_cell(buffer, j + x, i + y);
            if (p != NULL)
            {
                luminanceSum += pixelLuminance(p);
            }
        }
    }
    return luminanceSum / total;
}

void vcatVideo2AsciiConverterProcessPixelBuffer(Video2AsciiConverter *converter, matrix_t buffer)
{
    const int bufferHeight = matrix_get_height(buffer);
    const int bufferWidth = matrix_get_width(buffer);
    if (bufferHeight <= 0 || bufferWidth <= 0)
    {
        lfLogError("invalid video bounds");
        return;
    }

    const bool videoChanged =
        (converter->frameSizePixels.width != bufferWidth || converter->frameSizePixels.height != bufferHeight);

    if (videoChanged)
    {
        converter->frameSizePixels.width = bufferWidth;
        converter->frameSizePixels.height = bufferHeight;
        converter->videoRatio = converter->frameSizePixels.width / (float)converter->frameSizePixels.height;
    }

    if (videoChanged || converter->terminalSizeChange)
    {
        // The grid is measured in character cells, so its aspect ratio is the video's ratio
        // scaled by the shape of a cell. A cell also has to cover at least one pixel, otherwise
        // it would be left with an empty sample window.
        Rectangle maxGrid = converter->terminalSize;
        if (maxGrid.width > converter->frameSizePixels.width)
        {
            maxGrid.width = converter->frameSizePixels.width;
        }
        if (maxGrid.height > converter->frameSizePixels.height)
        {
            maxGrid.height = converter->frameSizePixels.height;
        }

        const float targetRatio = converter->videoRatio * CHAR_CELL_ASPECT;
        converter->prevAsciiBounds = converter->asciiBounds;
        converter->asciiBounds = vcatFitDimensionsToRatio(maxGrid, targetRatio, MAX_ASPECT_REL_ERROR);
        converter->terminalSizeChange = false;

        // The partition splits the frame into cells that differ by at most one pixel, so the
        // smallest and largest cell are just the frame size divided by the grid size, rounded.
        const int minCellW = converter->frameSizePixels.width / converter->asciiBounds.width;
        const int maxCellW =
            (converter->frameSizePixels.width + converter->asciiBounds.width - 1) / converter->asciiBounds.width;
        const int minCellH = converter->frameSizePixels.height / converter->asciiBounds.height;
        const int maxCellH =
            (converter->frameSizePixels.height + converter->asciiBounds.height - 1) / converter->asciiBounds.height;
        const float relError =
            fabsf(converter->asciiBounds.width / (float)converter->asciiBounds.height - targetRatio) / targetRatio;

        lfLogInfo("Ascii grid: %dx%d -> %dx%d (%s), ratio %.4f target %.4f, error %.3f%%, cell "
                  "%d-%dx%d-%dpx, "
                  "cut off 0x0",
                  converter->prevAsciiBounds.width, converter->prevAsciiBounds.height, converter->asciiBounds.width,
                  converter->asciiBounds.height, videoChanged ? "video change" : "terminal change",
                  converter->asciiBounds.width / (float)converter->asciiBounds.height, targetRatio, relError * 100.0f,
                  minCellW, maxCellW, minCellH, maxCellH);
    }

    matrix_i8_t asciiBuffer = NULL;
    if (!vcatAsciiFrameMailboxAcquireFree(converter->frameMailbox, &asciiBuffer))
    {
        if (matrix_init_greedy_i8(&asciiBuffer, converter->asciiBounds.height, converter->asciiBounds.width) != RC_OK)
        {
            lfLogError("unable to allocate ascii frame %dx%d", converter->asciiBounds.height,
                       converter->asciiBounds.width);
            return;
        }
    }

    if (matrix_get_height(asciiBuffer) != converter->asciiBounds.height ||
        matrix_get_width(asciiBuffer) != converter->asciiBounds.width)
    {
        if (matrix_resize(asciiBuffer, converter->asciiBounds.height, converter->asciiBounds.width) != RC_OK)
        {
            lfLogError("unable to resize ascii frame %dx%d", converter->asciiBounds.height,
                       converter->asciiBounds.width);
            matrix_free(asciiBuffer);
            return;
        }
    }

    const int gridWidth = matrix_get_width(asciiBuffer);
    const int gridHeight = matrix_get_height(asciiBuffer);
    if (gridWidth <= 0 || gridHeight <= 0 || converter->frameSizePixels.width <= 0 ||
        converter->frameSizePixels.height <= 0)
    {
        matrix_free(asciiBuffer);
        return;
    }

    const AsciiGradient gradient = converter->gradient;

    // Cell (i, j) covers the pixels [i * videoHeight / gridHeight, (i + 1) * videoHeight /
    // gridHeight) rows by [j * videoWidth / gridWidth, (j + 1) * videoWidth / gridWidth) columns.
    // Those windows tile the frame exactly, so every pixel lands in exactly one cell: nothing is
    // cut off at the edges and nothing is read out of bounds, no matter how close the grid ratio is
    // to the video's. Only the window sizes vary, by at most one pixel.
    for (int64_t i = 0; i < gridHeight; i++)
    {
        const int64_t y0 = i * converter->frameSizePixels.height / gridHeight;
        const int64_t y1 = (i + 1) * converter->frameSizePixels.height / gridHeight;
        for (int64_t j = 0; j < gridWidth; j++)
        {
            const int64_t x0 = j * converter->frameSizePixels.width / gridWidth;
            const int64_t x1 = (j + 1) * converter->frameSizePixels.width / gridWidth;
            float luminance = averagePixelsLuminance((int)x0,        //
                                                     (int)y0,        //
                                                     (int)(y1 - y0), //
                                                     (int)(x1 - x0), //
                                                     buffer);
            luminance = powf(luminance, 1.0f / LUMINANCE_GAMMA); // gamma: spread mid-tones across ramp
            matrix_set_i8(asciiBuffer, (int)j, (int)i, gradientGet(&gradient, luminance));
        }
    }

    vcatAsciiFrameMailboxPublish(converter->frameMailbox, asciiBuffer);
}

void vcatVideo2AsciiConverterUpdateVideoBounds(Video2AsciiConverter *converter, Rectangle newSize)
{
    converter->terminalSize = newSize;
    lfLogInfo("Video bounds size change h=%d w=%d", newSize.height, newSize.width);
    converter->terminalSizeChange = true;
}
