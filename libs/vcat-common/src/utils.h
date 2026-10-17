
#ifndef VCAT_UTILS_H
#define VCAT_UTILS_H

#include <stdint.h>
#include <stdio.h>

/**
 * @brief Expands its argument to a string literal.
 */
#define STR(V) (#V)

// Height of a character cell divided by its width. Terminal fonts are usually
// around 2.0, but the real value depends on the font the terminal is using.
#define CHAR_CELL_ASPECT 2.0f

// How far the character grid's aspect ratio may deviate from the source's,
// relative to the source, before we start trading picture size for accuracy.
#define MAX_ASPECT_REL_ERROR 0.005f

typedef struct
{
    uint8_t octet3;
    uint8_t octet2;
    uint8_t octet1;
    uint8_t octet0;
} Ip;

typedef struct
{
    int height;
    int width;
} Rectangle;

/**
 * @brief Packs the octets most significant first.
 */
uint32_t ipAddress(Ip ip);

/**
 * @brief The loopback address, 127.0.0.1.
 */
Ip ipLocalhost(void);

/**
 * @brief Renders dotted decimal into @p buffer, which must hold 16 bytes.
 */
void ipToStr(Ip ip, char buffer[16]);

/**
 * @brief Finds the largest grid inside `rec` whose aspect ratio is closest to `targetRatio`.
 *
 * Candidate grids can only be whole numbers of cells, so the ratio can rarely be hit
 * exactly. Candidates whose ratio is within `maxRelError` of the target (measured
 * relatively, so the tolerance means the same thing at any terminal size) are
 * preferred, and among those the one covering the most cells wins. If no candidate
 * lands inside the tolerance, the least-wrong ratio is returned.
 *
 * Only the widths bracketing targetRatio * height are examined, which is sufficient
 * because the ratio error is V-shaped in the width for any fixed height.
 *
 * @param[in] rec Available cells, in character cells.
 * @param[in] targetRatio Desired width / height of the grid.
 * @param[in] maxRelError Relative ratio error treated as acceptable, e.g. 0.005 for 0.5%.
 *
 * @return The best grid as {height, width}, or {0, 0} if there is nothing to fit into.
 */
Rectangle vcatFitDimensionsToRatio(Rectangle rec, float targetRatio, float maxRelError);

#endif // VCAT_UTILS_H
