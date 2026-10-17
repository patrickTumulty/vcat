
#include "utils.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @brief Checks whether candidate A fits better than candidate B.
 */
static bool checkIfBetterArea(const float maxRelError, float errorA, uint64_t areaA, float errorB, uint64_t areaB)
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

uint32_t ipAddress(Ip ip)
{
    return ((uint32_t)ip.octet3 << 24) | ((uint32_t)ip.octet2 << 16) | ((uint32_t)ip.octet1 << 8) | (uint32_t)ip.octet0;
}

Ip ipLocalhost(void)
{
    const Ip localhost = {127, 0, 0, 1};
    return localhost;
}

void ipToStr(Ip ip, char buffer[16])
{
    (void)snprintf(buffer, 16, "%d.%d.%d.%d", ip.octet3, ip.octet2, ip.octet1, ip.octet0);
}

Rectangle vcatFitDimensionsToRatio(Rectangle rec, float targetRatio, float maxRelError)
{
    if (rec.width <= 0 || rec.height <= 0 || targetRatio <= 0.0f)
    {
        const Rectangle nothing = {0, 0};
        return nothing;
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
        const int lower = (int)(targetRatio * height);
        const int candidates[2] = {lower, lower + 1};
        for (int i = 0; i < 2; i++)
        {
            int w = candidates[i];
            if (w < 1)
            {
                w = 1;
            }
            if (w > rec.width)
            {
                w = rec.width;
            }
            const float error = fabsf(w / (float)height - targetRatio) / targetRatio;
            const uint64_t area = (uint64_t)w * (uint64_t)height;

            if (bestArea == 0 || checkIfBetterArea(maxRelError, error, area, bestError, bestArea))
            {
                bestWidth = w;
                bestHeight = height;
                bestError = error;
                bestArea = area;
            }
        }
    }

    const Rectangle best = {bestHeight, bestWidth};
    return best;
}
