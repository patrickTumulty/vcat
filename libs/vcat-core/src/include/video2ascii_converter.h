
#ifndef VCAT_VIDEO2ASCII_CONVERTER_H
#define VCAT_VIDEO2ASCII_CONVERTER_H

#include "ascii_frame_mailbox.h"
#include "matrix.h"
#include "utils.h"
#include <stdint.h>

/// One RGB pixel, exactly as an RGB video frame lays it out.
typedef struct
{
#pragma pack(push, 1)
    uint8_t r;
    uint8_t g;
    uint8_t b;
#pragma pack(pop)
} Pixel;


typedef struct Video2AsciiConverter Video2AsciiConverter;

/**
 * @brief Creates a converter with an empty frame mailbox behind it.
 */
Video2AsciiConverter *vcatVideo2AsciiConverterCreate(void);

/**
 * @brief Frees the converter and its mailbox. Frames a renderer still holds are
 * released through the mailbox before calling this.
 */
void vcatVideo2AsciiConverterFree(Video2AsciiConverter *converter);

/**
 * @brief Converts one pixel buffer into an ASCII frame and publishes it.
 *
 * @param[in] converter Converter.
 * @param[in] buffer Pixel buffer of `pixel` cells, owned by the caller for the
 *                   duration of the call.
 */
void vcatVideo2AsciiConverterProcessPixelBuffer(Video2AsciiConverter *converter, matrix_t buffer);

/**
 * @brief Notifies the converter that the terminal (and with it the target grid) changed size.
 */
void vcatVideo2AsciiConverterUpdateVideoBounds(Video2AsciiConverter *converter, Rectangle newSize);

/**
 * @brief The mailbox the converter publishes to; readers borrow it, the converter owns it.
 */
AsciiFrameMailbox *vcatVideo2AsciiConverterGetFrameMailbox(Video2AsciiConverter *converter);

void vcatVideo2AsciiConverterInvertGradient(Video2AsciiConverter *converter);

#endif // VCAT_VIDEO2ASCII_CONVERTER_H
