
#ifndef VCAT_COLOR_UTILS_H
#define VCAT_COLOR_UTILS_H

#include <stdint.h>

/// A packed RGB triple, 0x00RRGGBB, matching the AsciiFrame color matrix.
typedef uint32_t VcatColor;

/**
 * @brief Decodes an sRGB channel value into linear light.
 *
 * Backed by a 256-entry table: the converter calls this per pixel per frame,
 * so the piecewise sRGB curve must not be evaluated (powf) per call.
 */
float vcatColorSrgbToLinear(uint8_t value);

/**
 * @brief Encodes a linear light value into an sRGB channel byte.
 *
 * @param[in] linear Linear intensity, clamped to [0, 1].
 */
uint8_t vcatColorLinearToSrgbByte(float linear);

/**
 * @brief Accumulates a window's pixels for a linear-light average.
 *
 * Pixel colors are decoded to linear light and averaged channel-wise, then
 * re-encoded to sRGB on finish. Linear averaging weights as the eye does, so
 * detailed (high pixel variance) regions keep their color instead of washing
 * out the way plain sRGB byte means do.
 */
typedef struct VcatColorAverage
{
    float rSum;
    float gSum;
    float bSum;
    int count;
} VcatColorAverage;

/**
 * @brief Resets @p accumulator to an empty window.
 */
void vcatColorAverageInit(VcatColorAverage *accumulator);

/**
 * @brief Adds one packed color to @p accumulator.
 */
void vcatColorAverageAdd(VcatColorAverage *accumulator, VcatColor color);

/**
 * @brief Encodes the channel-wise linear mean as a packed color.
 *
 * @return Packed 0x00RRGGBB, rounded, black for an empty accumulator.
 */
VcatColor vcatColorAverageFinish(const VcatColorAverage *accumulator);

/**
 * @brief Nearest xterm-256 palette entry (16..255) for @p color.
 *
 * Candidates are scored with the "redmean" approximation of perceptual distance,
 * which weights the sensitive green channel higher and corrects red bias near
 * mid-range, where naive squared-RGB matching snaps mid-tones to visibly wrong
 * hues. The system-reserved entries 0-15 are never returned.
 *
 * Exact searches are memoized per thread on a fixed-size hash cache: cell
 * colors repeat heavily frame to frame, so steady-state cost is one probe per
 * color and no thread sees coherent shared state.
 */
short vcatColorNearestPaletteIndex(VcatColor color);

#endif // VCAT_COLOR_UTILS_H
