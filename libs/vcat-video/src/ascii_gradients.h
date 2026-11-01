
#ifndef ASCII_GRADIENTS_H
#define ASCII_GRADIENTS_H

typedef struct
{
    const char *name;
    const char *gradient;
    int gradientLen;
} AsciiGradient;

extern const AsciiGradient gAsciiGradients[];
extern const int gAsciiGradientsLen;

#endif // ASCII_GRADIENTS_H
