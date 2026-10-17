
#include "video2ascii_converter.h"
#include "greedy_matrix.h"
#include "logging.h"
#include "logging_facade.h"
#include "matrix.h"
#include "utils.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief One character ramp and how to read it.
 *
 * The terminal is usually dark-on-light or light-on-dark; invert() flips which
 * end of the ramp the darkest pixels map to.
 */
typedef struct AsciiGradient
{
    const char *gradient;
    int gradientLen;
    bool inverted;
} AsciiGradient;

static char gradientGet(const AsciiGradient *g, float value)
{
    if (g->inverted)
    {
        value = 1 - value;
    }
    int offset = g->gradientLen - 1;
    const int scaled = (int)(g->gradientLen * value);
    if (scaled < offset)
    {
        offset = scaled;
    }
    return g->gradient[offset];
}

static void gradientInvert(AsciiGradient *g)
{
    g->inverted = !g->inverted;
}

static const char *GRADIENT1 = "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. ";
static const char *GRADIENT2 = "@#*+=- ";

#define LUMINANCE_GAMMA 2.2f

// One global ramp, flipped at converter creation: the ramp runs dark to
// light, but the terminal is light on dark.
static AsciiGradient gradient = {NULL, 0, false};

struct Video2AsciiConverter
{
    AsciiFrameMailbox *frameMailbox;

    Rectangle pixelDimensions;
    Rectangle terminalSize;
    Rectangle asciiBounds;
    Rectangle prevAsciiBounds;
    bool terminalSizeChange;
    float videoRatio;
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

    if (gradient.gradient == NULL)
    {
        gradient.gradient = GRADIENT2;
        gradient.gradientLen = (int)strlen(GRADIENT2);
    }
    gradientInvert(&gradient);

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
            const pixel *p = matrix_get_cell(buffer, j + x, i + y);
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
        (converter->pixelDimensions.width != bufferWidth || converter->pixelDimensions.height != bufferHeight);

    if (videoChanged)
    {
        converter->pixelDimensions.width = bufferWidth;
        converter->pixelDimensions.height = bufferHeight;
        converter->videoRatio = converter->pixelDimensions.width / (float)converter->pixelDimensions.height;
    }

    if (videoChanged || converter->terminalSizeChange)
    {
        // The grid is measured in character cells, so its aspect ratio is the video's ratio
        // scaled by the shape of a cell. A cell also has to cover at least one pixel, otherwise
        // it would be left with an empty sample window.
        Rectangle maxGrid = converter->terminalSize;
        if (maxGrid.width > converter->pixelDimensions.width)
        {
            maxGrid.width = converter->pixelDimensions.width;
        }
        if (maxGrid.height > converter->pixelDimensions.height)
        {
            maxGrid.height = converter->pixelDimensions.height;
        }

        const float targetRatio = converter->videoRatio * CHAR_CELL_ASPECT;
        converter->prevAsciiBounds = converter->asciiBounds;
        converter->asciiBounds = vcatFitDimensionsToRatio(maxGrid, targetRatio, MAX_ASPECT_REL_ERROR);
        converter->terminalSizeChange = false;

        // The partition splits the frame into cells that differ by at most one pixel, so the
        // smallest and largest cell are just the frame size divided by the grid size, rounded.
        const int minCellW = converter->pixelDimensions.width / converter->asciiBounds.width;
        const int maxCellW =
            (converter->pixelDimensions.width + converter->asciiBounds.width - 1) / converter->asciiBounds.width;
        const int minCellH = converter->pixelDimensions.height / converter->asciiBounds.height;
        const int maxCellH =
            (converter->pixelDimensions.height + converter->asciiBounds.height - 1) / converter->asciiBounds.height;
        const float relError =
            fabsf(converter->asciiBounds.width / (float)converter->asciiBounds.height - targetRatio) / targetRatio;

        lfLogInfo("Ascii grid: %dx%d -> %dx%d (%s), ratio %.4f target %.4f, error %.3f%%, cell %d-%dx%d-%dpx, "
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
    if (gridWidth <= 0 || gridHeight <= 0 || converter->pixelDimensions.width <= 0 ||
        converter->pixelDimensions.height <= 0)
    {
        matrix_free(asciiBuffer);
        return;
    }

    // Cell (i, j) covers the pixels [i * videoHeight / gridHeight, (i + 1) * videoHeight / gridHeight) rows
    // by [j * videoWidth / gridWidth, (j + 1) * videoWidth / gridWidth) columns. Those windows tile the
    // frame exactly, so every pixel lands in exactly one cell: nothing is cut off at the edges and
    // nothing is read out of bounds, no matter how close the grid ratio is to the video's. Only the
    // window sizes vary, by at most one pixel.
    for (int64_t i = 0; i < gridHeight; i++)
    {
        const int64_t y0 = i * converter->pixelDimensions.height / gridHeight;
        const int64_t y1 = (i + 1) * converter->pixelDimensions.height / gridHeight;
        for (int64_t j = 0; j < gridWidth; j++)
        {
            const int64_t x0 = j * converter->pixelDimensions.width / gridWidth;
            const int64_t x1 = (j + 1) * converter->pixelDimensions.width / gridWidth;
            float luminance = averagePixelsLuminance((int)x0, (int)y0, (int)(y1 - y0), (int)(x1 - x0), buffer);
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
