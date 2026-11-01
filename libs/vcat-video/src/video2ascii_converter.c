
#include "video2ascii_converter.h"
#include "ascii_gradients.h"
#include "color_utils.h"
#include "greedy_matrix.h"
#include "logging_facade.h"
#include "matrix.h"
#include "utils.h"
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>

/**
 * @brief One character ramp and how to read it.
 *
 * The terminal is usually dark-on-light or light-on-dark; invert() flips which
 * end of the ramp the darkest pixels map to. The flag is atomic: the TUI
 * thread flips it while the GStreamer streaming thread reads it.
 */
typedef struct AsciiGradientData
{
    /**
     * One Unicode codepoint per ramp cell, decoded once from the gradient's
     * UTF-8 string with vcatAsciiGradientDecode(); pointLen counts codepoints.
     */
    uint32_t points[ASCII_GRADIENT_MAX_LEN];
    int pointLen;
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

static uint32_t gradientGet(const AsciiGradientData *g, float value)
{
    if (atomic_load(&g->inverted))
    {
        value = 1 - value;
    }
    int offset = g->pointLen - 1;
    const int scaled = (int)(g->pointLen * value);
    if (scaled < offset)
    {
        offset = scaled;
    }
    return g->points[offset];
}

static void gradientInvert(AsciiGradientData *g)
{
    atomic_store(&g->inverted, !atomic_load(&g->inverted));
}

/**
 * @brief Decodes the gAsciiGradients entry @p index into @p g.
 */
static bool gradientLoad(AsciiGradientData *g, int index)
{
    if (index < 0 || index >= gAsciiGradientsLen)
    {
        lfLogError("invalid ascii gradient index %d", index);
        return false;
    }
    int pointLen = 0;
    if (!vcatAsciiGradientDecode(&gAsciiGradients[index], g->points, &pointLen))
    {
        lfLogError("unable to decode ascii gradient \"%s\"", gAsciiGradients[index].name);
        return false;
    }
    g->pointLen = pointLen;
    return true;
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
    AsciiGradientData gradient;
    int gradientIndex;
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

    // Index 0 ("Standard (10L)") is the historical default and starts inverted:
    // dark-background terminals want the dark end of the ramp as the "full" cell.
    if (!gradientLoad(&converter->gradient, 0))
    {
        lfLogError("unable to load default ascii gradient");
        vcatAsciiFrameMailboxFree(frameMailbox);
        free(converter);
        return NULL;
    }
    converter->gradientIndex = 0;
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

bool vcatVideo2AsciiConverterSetAsciiGradient(Video2AsciiConverter *converter, int index)
{
    if (converter == NULL)
    {
        return false;
    }
    AsciiGradientData loaded;
    if (!gradientLoad(&loaded, index))
    {
        return false;
    }
    atomic_init(&loaded.inverted, atomic_load(&converter->gradient.inverted));
    converter->gradient = loaded;
    converter->gradientIndex = index;
    return true;
}

int vcatVideo2AsciiConverterGetAsciiGradientIndex(Video2AsciiConverter *converter)
{
    return converter != NULL ? converter->gradientIndex : -1;
}

/**
 * @brief Average color of the window [x, x + width) * [y, y + height) in `buffer`.
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

/**
 * @brief Average sRGB color of the window [x, x + width) * [y, y + height) in `buffer`.
 *
 * The average is taken in linear light (color_utils): pixel colors decode through
 * the sRGB LUT, and the mean re-encodes per cell. The packed result is what the
 * renderer maps onto the terminal palette.
 *
 * @return Packed 0x00RRGGBB, rounded, black when the window is empty.
 */
static uint32_t averagePixelsColor(int x, int y, int height, int width, matrix_t buffer)
{
    VcatColorAverage average;
    vcatColorAverageInit(&average);
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            const Pixel *p = matrix_get_cell(buffer, j + x, i + y);
            if (p != NULL)
            {
                vcatColorAverageAdd(&average, ((uint32_t)p->r << 16) | ((uint32_t)p->g << 8) | (uint32_t)p->b);
            }
        }
    }
    return vcatColorAverageFinish(&average);
}

/**
 * @brief Makes both frame matrices exist at the requested size.
 *
 * Frames recycled from the mailbox's free pool only need a resize when the
 * grid changed; a fresh frame is allocated on first use. Both matrices are
 * u32 stores: Unicode codepoints and packed RGB.
 *
 * @retval true  Both matrices exist at height x width.
 * @retval false Allocation failed; @p frame is fully freed.
 */
static bool frameEnsureSize(AsciiFrame *frame, int height, int width)
{
    if (frame->chars == NULL)
    {
        if (matrix_init_greedy_u32(&frame->chars, height, width) != RC_OK)
        {
            vcatAsciiFrameFree(frame);
            return false;
        }
    }
    else if (matrix_get_height(frame->chars) != height || matrix_get_width(frame->chars) != width)
    {
        if (matrix_resize(frame->chars, height, width) != RC_OK)
        {
            vcatAsciiFrameFree(frame);
            return false;
        }
    }

    if (frame->colors == NULL)
    {
        if (matrix_init_greedy_u32(&frame->colors, height, width) != RC_OK)
        {
            vcatAsciiFrameFree(frame);
            return false;
        }
    }
    else if (matrix_get_height(frame->colors) != height || matrix_get_width(frame->colors) != width)
    {
        if (matrix_resize(frame->colors, height, width) != RC_OK)
        {
            vcatAsciiFrameFree(frame);
            return false;
        }
    }
    return true;
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

    const bool videoChanged = converter->frameSizePixels.width != bufferWidth || //
                              converter->frameSizePixels.height != bufferHeight;

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

    AsciiFrame asciiFrame = {NULL, NULL};
    vcatAsciiFrameMailboxAcquireFree(converter->frameMailbox, &asciiFrame);
    // When nothing was recycled, the frame is built from scratch below.

    if (!frameEnsureSize(&asciiFrame, converter->asciiBounds.height, converter->asciiBounds.width))
    {
        lfLogError("unable to allocate ascii frame %dx%d", converter->asciiBounds.height, converter->asciiBounds.width);
        return;
    }

    matrix_u32_t asciiBuffer = asciiFrame.chars;
    const int gridWidth = matrix_get_width(asciiBuffer);
    const int gridHeight = matrix_get_height(asciiBuffer);
    if (gridWidth <= 0 || gridHeight <= 0 || converter->frameSizePixels.width <= 0 ||
        converter->frameSizePixels.height <= 0)
    {
        vcatAsciiFrameFree(&asciiFrame);
        return;
    }

    const AsciiGradientData gradient = converter->gradient;

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
            const int cellH = (int)(y1 - y0);
            const int cellW = (int)(x1 - x0);
            float luminance = averagePixelsLuminance((int)x0, (int)y0, cellH, cellW, buffer);
            luminance = powf(luminance, 1.0f / LUMINANCE_GAMMA); // gamma: spread mid-tones across ramp
            u32 avgAsciiChar = gradientGet(&gradient, luminance);
            matrix_set_u32(asciiBuffer, (int)j, (int)i, avgAsciiChar);
            u32 avgColor = averagePixelsColor((int)x0, (int)y0, cellH, cellW, buffer);
            matrix_set_u32(asciiFrame.colors, (int)j, (int)i, avgColor);
        }
    }

    vcatAsciiFrameMailboxPublish(converter->frameMailbox, asciiFrame);
}

void vcatVideo2AsciiConverterUpdateVideoBounds(Video2AsciiConverter *converter, Rectangle newSize)
{
    converter->terminalSize = newSize;
    lfLogInfo("Video bounds size change h=%d w=%d", newSize.height, newSize.width);
    converter->terminalSizeChange = true;
}
