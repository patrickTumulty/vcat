
#pragma once

#include "gst/gstelement.h"
#include <cmath>
#include <ncurses.h>
#include <string>

#define STR(V) (#V)

// Height of a character cell divided by its width. Terminal fonts are usually
// around 2.0, but the real value depends on the font the terminal is using.
constexpr float CHAR_CELL_ASPECT = 2.0f;

// How far the character grid's aspect ratio may deviate from the source's,
// relative to the source, before we start trading picture size for accuracy.
constexpr float MAX_ASPECT_REL_ERROR = 0.005f;

struct Ip
{
    uint8_t octet3{};
    uint8_t octet2{};
    uint8_t octet1{};
    uint8_t octet0{};

    constexpr uint32_t address() const
    {
        return (static_cast<uint32_t>(octet3) << 24) | (static_cast<uint32_t>(octet2) << 16) |
               (static_cast<uint32_t>(octet1) << 8) | static_cast<uint32_t>(octet0);
    }

    static Ip localhost()
    {
        return {127, 0, 0, 1};
    }

    std::string toStr()
    {
        char ipStr[16];
        (void)snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", octet3, octet2, octet1, octet0);
        return std::string(ipStr);
    }
};

enum VideoSourceType : uint8_t
{
    NONE = 0,
    UDP_MPEGTS = 1,
    TEST = 2
};

struct NetworkSource
{
    Ip ip{};
    int port;
};

struct Rectangle
{
    int height;
    int width;
};

void drawBox(int x, int y, int height, int width);
Rectangle fitDimensionsToRatio(const Rectangle rec, const float targetRatio,
                               const float maxRelError = MAX_ASPECT_REL_ERROR);
void verifyElement(GstElement *element, const char *elementName, std::string failMessage);
