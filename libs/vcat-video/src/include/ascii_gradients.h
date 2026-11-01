
#ifndef ASCII_GRADIENTS_H
#define ASCII_GRADIENTS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Upper bound on the character count (codepoints) of any gradient in
 * gAsciiGradients. Decode buffers sized with this are always large enough.
 */
#define ASCII_GRADIENT_MAX_LEN 128

typedef struct
{
    const char *name; ///< Display name.
    /**
     * UTF-8 ramp, NUL-terminated. Every character is the glyph for one cell of
     * the ASCII grid, and a character may be longer than one UTF-8 byte: index
     * the ramp by character, not by byte. The field is kept readable (and
     * editable) as the single source of truth; the converter decodes it once
     * into codepoints with vcatAsciiGradientDecode().
     */
    const char *gradient;
} AsciiGradient;

extern const AsciiGradient gAsciiGradients[];
extern const int gAsciiGradientsLen;

/**
 * @brief Decodes a gradient's UTF-8 ramp into one uint32_t codepoint per glyph.
 *
 * @param[in] def        Gradient from gAsciiGradients; must not be NULL.
 * @param[out] outPoints Receives one codepoint per ramp character; must be able
 *                       to hold ASCII_GRADIENT_MAX_LEN values.
 * @param[out] outLen    Receives the decoded codepoint count; must not be NULL.
 * @return true if the ramp was fully decoded into @p outPoints.
 */
bool vcatAsciiGradientDecode(const AsciiGradient *def, uint32_t *outPoints, int *outLen);

#endif // ASCII_GRADIENTS_H
