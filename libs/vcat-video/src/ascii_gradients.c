
#include "ascii_gradients.h"
#include <string.h>

#define AG_STANDARD_10L "@#*+=- "
#define AG_EXTENDED_70L "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. "
#define AG_HORIZONTAL "█▉▊▋▌▍▎▏ "
#define AG_VERTICAL "█▇▆▅▄▃▂  "
#define AG_DITHERED "█▓▒░ "

const AsciiGradient gAsciiGradients[] = {
    {.name = "Standard (10L)", .gradient = AG_STANDARD_10L, .gradientLen = strlen(AG_STANDARD_10L)},
    {.name = "Extended (70L)", .gradient = AG_EXTENDED_70L, .gradientLen = strlen(AG_EXTENDED_70L)},
    {.name = "Horizontal",     .gradient = AG_HORIZONTAL,   .gradientLen = strlen(AG_HORIZONTAL)  },
    {.name = "Vertical",       .gradient = AG_VERTICAL,     .gradientLen = strlen(AG_VERTICAL)    },
    {.name = "Dithered",       .gradient = AG_DITHERED,     .gradientLen = strlen(AG_DITHERED)    },
};
const int gAsciiGradientsLen = sizeof(gAsciiGradients) / sizeof(gAsciiGradients[0]);
