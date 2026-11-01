
#include "color_utils.h"
#include <math.h>
#include <pthread.h>
#include <stdbool.h>

/// sRGB decode/encode constants from IEC 61966-2-1.
#define SRGB_ALPHA 1.055f
#define SRGB_BIAS 0.055f
#define SRGB_SLOPE 12.92f
/// Decode branches on the sRGB value, encode on the linear value.
#define SRGB_DECODE_THRESHOLD 0.04045f
#define SRGB_ENCODE_THRESHOLD 0.0031308f
#define SRGB_GAMMA 2.4f

/**
 * @brief LUT of sRGB byte -> linear light, filled once on first use.
 *
 * vcatColorSrgbToLinear() runs per pixel per frame in the converter, so the
 * piecewise sRGB curve (a powf) must not be evaluated per call. The fill is
 * guarded by pthread_once rather than a plain "initialized" flag: the converter
 * streams on a GStreamer thread while the TUI thread may query the palette
 * concurrently.
 */
static float srgbToLinearLut[256];

static void srgbToLinearLutInit(void)
{
    for (int i = 0; i < 256; i++)
    {
        const float srgb = (float)i / 255.0f;
        if (srgb <= SRGB_DECODE_THRESHOLD)
        {
            srgbToLinearLut[i] = srgb / SRGB_SLOPE;
        }
        else
        {
            srgbToLinearLut[i] = powf((srgb + SRGB_BIAS) / SRGB_ALPHA, SRGB_GAMMA);
        }
    }
}

static void srgbToLinearLutEnsureInit(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, srgbToLinearLutInit);
}

float vcatColorSrgbToLinear(uint8_t value)
{
    srgbToLinearLutEnsureInit();
    return srgbToLinearLut[value];
}

uint8_t vcatColorLinearToSrgbByte(float linear)
{
    if (linear <= 0.0f)
    {
        return 0;
    }
    if (linear >= 1.0f)
    {
        return 255;
    }

    float srgb;
    if (linear <= SRGB_ENCODE_THRESHOLD)
    {
        srgb = linear * SRGB_SLOPE;
    }
    else
    {
        srgb = SRGB_ALPHA * powf(linear, 1.0f / SRGB_GAMMA) - SRGB_BIAS;
    }
    return (uint8_t)(srgb * 255.0f + 0.5f);
}

void vcatColorAverageInit(VcatColorAverage *accumulator)
{
    accumulator->rSum = 0.0f;
    accumulator->gSum = 0.0f;
    accumulator->bSum = 0.0f;
    accumulator->count = 0;
}

void vcatColorAverageAdd(VcatColorAverage *accumulator, VcatColor color)
{
    accumulator->rSum += vcatColorSrgbToLinear((uint8_t)(color >> 16));
    accumulator->gSum += vcatColorSrgbToLinear((uint8_t)(color >> 8));
    accumulator->bSum += vcatColorSrgbToLinear((uint8_t)color);
    accumulator->count++;
}

VcatColor vcatColorAverageFinish(const VcatColorAverage *accumulator)
{
    if (accumulator->count <= 0)
    {
        return 0;
    }
    const float reciprocal = 1.0f / (float)accumulator->count;
    return (VcatColor)vcatColorLinearToSrgbByte(accumulator->rSum * reciprocal) << 16 | //
           (VcatColor)vcatColorLinearToSrgbByte(accumulator->gSum * reciprocal) << 8 |  //
           (VcatColor)vcatColorLinearToSrgbByte(accumulator->bSum * reciprocal);
}

/**
 * @brief The xterm-256 palette, entries 16..255 of @ref paletteRgb.
 *
 * Entries 16-231 are a 6x6x6 cube over the channel levels
 * {0, 95, 135, 175, 215, 255}; 232-255 are a 24-step grayscale ramp from 8 to
 * 238. 0-15 are the user-configurable system colors and are deliberately left
 * out: vcat never overrides what the user chose for their terminal.
 */
#define PALETTE_MIN_INDEX 16
#define PALETTE_LEN 240

static VcatColor paletteRgb[PALETTE_LEN];

static void paletteInit(void)
{
    static const uint8_t levels[] = {0, 95, 135, 175, 215, 255};
    int entry = 0;
    for (int r = 0; r < 6; r++)
    {
        for (int g = 0; g < 6; g++)
        {
            for (int b = 0; b < 6; b++)
            {
                paletteRgb[entry++] = (VcatColor)levels[r] << 16 | (VcatColor)levels[g] << 8 | (VcatColor)levels[b];
            }
        }
    }
    for (int k = 0; k < 24; k++)
    {
        const VcatColor gray = (VcatColor)(8 + 10 * k);
        paletteRgb[entry++] = gray * 0x010101U;
    }
}

/**
 * @brief Squared "redmean" distance between two packed colors.
 *
 * A cheap approximation of perceptual RGB distance; the >> 8 scalings of the
 * red and blue terms are dropped because a uniform scale never changes which
 * candidate is nearest. Products stay far below 2^31.
 */
static int redmeanSquared(VcatColor a, VcatColor b)
{
    const int ar = (a >> 16) & 0xFF;
    const int ag = (a >> 8) & 0xFF;
    const int ab = a & 0xFF;
    const int br = (b >> 16) & 0xFF;
    const int bg = (b >> 8) & 0xFF;
    const int bb = b & 0xFF;

    const int rMean = (ar + br) / 2;
    const int dr = ar - br;
    const int dg = ag - bg;
    const int db = ab - bb;
    return (512 + rMean) * dr * dr + 4 * 256 * dg * dg + (767 - rMean) * db * db;
}

static short nearestPaletteIndexSearch(VcatColor color)
{
    int best = 0;
    int bestDist = redmeanSquared(color, paletteRgb[0]);
    for (int i = 1; i < PALETTE_LEN; i++)
    {
        const int dist = redmeanSquared(color, paletteRgb[i]);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = i;
        }
    }
    return (short)(best + PALETTE_MIN_INDEX);
}

/// Cache size must be a power of two; ~4k exact colors is far beyond what a
/// realistic grid displays, so probe misses are rare after warm-up.
#define PALETTE_CACHE_SIZE 4096

/**
 * @brief Per-thread cache of exact color -> palette index answers.
 *
 * Per-thread memory needs no locking and no atomics: the converter queries from
 * the streaming thread while the TUI threads query from render callbacks. The
 * xterm palette only spans 240 entries, so distinct colors repeat heavily
 * across frames and cells; probing into a crowded cache just falls back to the
 * exact search without thrash.
 */
typedef struct
{
    VcatColor color;
    uint8_t index; // Padding to 8 bytes; valid only when occupied.
    bool occupied;
} PaletteCacheEntry;

const char *vcatColorModeName(VcatColorMode mode)
{
    static const char *names[VCAT_COLOR_MODE_COUNT] = {"no color", "black & white", "color"};
    if (mode < 0 || mode >= VCAT_COLOR_MODE_COUNT)
    {
        return "invalid";
    }
    return names[mode];
}

uint8_t vcatColorLuminance(VcatColor color)
{
    const float r = (color >> 16) & 0xFF;
    const float g = (color >> 8) & 0xFF;
    const float b = color & 0xFF;
    return (uint8_t)((0.2126f * r + 0.7152f * g + 0.0722f * b) + 0.5f);
}

static short vcatColorNearestPaletteIndexImpl(VcatColor color)
{
    static _Thread_local PaletteCacheEntry cache[PALETTE_CACHE_SIZE] = {0};

    const uint32_t mask = PALETTE_CACHE_SIZE - 1;
    const uint32_t slot0 = (color * 2654435761U) >> (32 - 12);
    for (int probe = 0; probe < 8; probe++)
    {
        PaletteCacheEntry *entry = &cache[(slot0 + (uint32_t)probe) & mask];
        if (!entry->occupied)
        {
            const short index = nearestPaletteIndexSearch(color);
            entry->color = color;
            entry->index = (uint8_t)index;
            entry->occupied = true;
            return index;
        }
        if (entry->color == color)
        {
            return (short)entry->index;
        }
    }
    // Crowded region: answer exactly without disturbing the cache.
    return nearestPaletteIndexSearch(color);
}

short vcatColorNearestPaletteIndex(VcatColor color)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, paletteInit);
    return vcatColorNearestPaletteIndexImpl(color);
}
