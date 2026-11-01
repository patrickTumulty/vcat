
#include "ascii_gradients.h"
#include "logging_facade.h"

#define AG_STANDARD_10L "@#*+=- "
#define AG_EXTENDED_70L "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. "
#define AG_HORIZONTAL "█▉▊▋▌▍▎▏ "
#define AG_VERTICAL "█▇▆▅▄▃▂  "
#define AG_DITHERED "█▓▒░ "

const AsciiGradient gAsciiGradients[] = {
    {.name = "Standard (10L)", .gradient = AG_STANDARD_10L},
    {.name = "Extended (70L)", .gradient = AG_EXTENDED_70L},
    {.name = "Horizontal",     .gradient = AG_HORIZONTAL  },
    {.name = "Vertical",       .gradient = AG_VERTICAL    },
    {.name = "Dithered",       .gradient = AG_DITHERED    },
};
const int gAsciiGradientsLen = sizeof(gAsciiGradients) / sizeof(gAsciiGradients[0]);

/**
 * @brief Decodes the UTF-8 sequence starting at @p s into one codepoint.
 * @return Number of bytes consumed (1-4), or 0 when the sequence is invalid.
 */
static int decodeCodepoint(const char *s, uint32_t *outPoint)
{
    const unsigned char lead = (unsigned char)s[0];
    int bytes;
    uint32_t point;
    if (lead < 0x80)
    {
        bytes = 1;
        point = lead;
    }
    else if ((lead & 0xE0) == 0xC0)
    {
        bytes = 2;
        point = lead & UINT32_C(0x1F);
    }
    else if ((lead & 0xF0) == 0xE0)
    {
        bytes = 3;
        point = lead & UINT32_C(0x0F);
    }
    else if ((lead & 0xF8) == 0xF0)
    {
        bytes = 4;
        point = lead & UINT32_C(0x07);
    }
    else
    {
        return 0;
    }
    for (int i = 1; i < bytes; i++)
    {
        const unsigned char cont = (unsigned char)s[i];
        if ((cont & 0xC0) != 0x80)
        {
            return 0;
        }
        point = (point << 6) | (cont & UINT32_C(0x3F));
    }
    if (point > UINT32_C(0x10FFFF))
    {
        return 0;
    }
    *outPoint = point;
    return bytes;
}

bool vcatAsciiGradientDecode(const AsciiGradient *def, uint32_t *outPoints, int *outLen)
{
    if (def == NULL || def->gradient == NULL || outPoints == NULL || outLen == NULL)
    {
        return false;
    }

    int decoded = 0;
    const char *cursor = def->gradient;
    while (*cursor != '\0')
    {
        uint32_t point;
        const int bytes = decodeCodepoint(cursor, &point);
        if (bytes == 0 || decoded >= ASCII_GRADIENT_MAX_LEN)
        {
            return false;
        }
        outPoints[decoded++] = point;
        cursor += bytes;
    }
    *outLen = decoded;
    return decoded > 0;
}
