
#ifndef VCAT_VIDEO2ASCII_CONVERTER_H
#define VCAT_VIDEO2ASCII_CONVERTER_H

#include "ascii_frame_mailbox.h"
#include "matrix.h"
#include "utils.h"
#include <stdint.h>

typedef struct
{
#pragma pack(push, 1)
    uint8_t r;
    uint8_t g;
    uint8_t b;
#pragma pack(pop)
} Pixel;

typedef struct Video2AsciiConverter Video2AsciiConverter;

Video2AsciiConverter *vcatVideo2AsciiConverterCreate(void);

void vcatVideo2AsciiConverterFree(Video2AsciiConverter *converter);

void vcatVideo2AsciiConverterProcessPixelBuffer(Video2AsciiConverter *converter, matrix_t buffer);

void vcatVideo2AsciiConverterUpdateVideoBounds(Video2AsciiConverter *converter, Rectangle newSize);

AsciiFrameMailbox *vcatVideo2AsciiConverterGetFrameMailbox(Video2AsciiConverter *converter);

void vcatVideo2AsciiConverterInvertGradient(Video2AsciiConverter *converter);

bool vcatVideo2AsciiConverterSetAsciiGradient(Video2AsciiConverter *converter, int index);

int vcatVideo2AsciiConverterGetAsciiGradientIndex(Video2AsciiConverter *converter);

#endif // VCAT_VIDEO2ASCII_CONVERTER_H
