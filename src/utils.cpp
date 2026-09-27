
#include "utils.hpp"
#include "gst/gstelement.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <stdexcept>

namespace
{

bool checkIfBetterArea(const float maxRelError, float errorA, uint64_t areaA, float errorB, uint64_t areaB)
{
    const bool withinA = errorA <= maxRelError;
    const bool withinB = errorB <= maxRelError;
    if (withinA != withinB)
    {
        return withinA; // Being inside the tolerance always beats being outside it
    }
    if (withinA)
    {
        return areaA > areaB; // Both are accurate enough, so show as much picture as possible
    }
    if (errorA != errorB)
    {
        return errorA < errorB; // Nothing is accurate enough, so be as close as possible
    }
    return areaA > areaB;
}

}; // namespace

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
 * @param rec Available cells, in character cells.
 * @param targetRatio Desired width / height of the grid.
 * @param maxRelError Relative ratio error treated as acceptable, e.g. 0.005 for 0.5%.
 *
 * @return The best grid as {height, width}, or {0, 0} if there is nothing to fit into.
 */
Rectangle fitDimensionsToRatio(const Rectangle rec, const float targetRatio, const float maxRelError)
{
    if (rec.width <= 0 || rec.height <= 0 || targetRatio <= 0.0f)
    {
        return {0, 0};
    }

    int bestWidth = 1;
    int bestHeight = 1;
    float bestError = 0.0f;
    uint64_t bestArea = 0;

    for (int height = 1; height <= rec.height; height++)
    {
        // For a fixed height the ratio error is V-shaped in the width, so the best width is
        // always one of the two bracketing targetRatio * height. Considering only the rounded
        // value would skip a wider grid that is still inside the tolerance.
        const int lower = static_cast<int>(targetRatio * height);
        for (int width : {lower, lower + 1})
        {
            const int w = std::clamp(width, 1, rec.width);
            const float error = std::fabs(w / static_cast<float>(height) - targetRatio) / targetRatio;
            const long long area = static_cast<uint64_t>(w) * height;

            if (bestArea == 0 || checkIfBetterArea(maxRelError, error, area, bestError, bestArea))
            {
                bestWidth = w;
                bestHeight = height;
                bestError = error;
                bestArea = area;
            }
        }
    }

    return {bestHeight, bestWidth};
}

void drawBox(int x, int y, int height, int width)
{
    if (!(width >= 2 && height >= 2 && x >= 0 && y >= 0 && x + width <= COLS && y + height <= LINES))
    {
        return;
    }

    mvhline(y, x + 1, ACS_HLINE, width - 2);              // Top Line
    mvhline(y + height - 1, x + 1, ACS_HLINE, width - 2); // Bottom Line

    mvvline(y + 1, x, ACS_VLINE, height - 2);             // Left Line
    mvvline(y + 1, x + width - 1, ACS_VLINE, height - 2); // Right Line

    mvaddch(y, x, ACS_ULCORNER);                          // Upper Left
    mvaddch(y, x + width - 1, ACS_URCORNER);              // Upper Right
    mvaddch(y + height - 1, x, ACS_LLCORNER);             // Lower Left
    mvaddch(y + height - 1, x + width - 1, ACS_LRCORNER); // Lower Right
}

void verifyElement(GstElement *element, const char *elementName, std::string failMessage)
{
    if (element == nullptr)
    {
        throw std::runtime_error(std::format("{} : unable to create '{}'", failMessage, elementName));
    }
}
