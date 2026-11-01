
#ifndef VCAT_ASCII_FRAME_MAILBOX_H
#define VCAT_ASCII_FRAME_MAILBOX_H

#include "crox_types.h"
#include "matrix.h"

/**
 * @brief One rendered ASCII frame: what to print, and how to tint it.
 *
 * chars and colors are parallel matrices of identical dimensions. Cells hold
 * one uint32_t each:
 *
 * - chars: a Unicode codepoint, rendered through the UTF-8 locale the TUI
 *   session sets up before ncurses initializes.
 * - colors: a packed RGB triple, 0x00RRGGBB, the averaged color of the video
 *   pixels the cell samples. Renderers that do not support or want color can
 *   ignore it.
 */
typedef struct AsciiFrame
{
    matrix_u32_t chars;  // Unicode codepoint per cell.
    matrix_u32_t colors; // 0x00RRGGBB averaged color per cell.
} AsciiFrame;

/**
 * Frees both matrices of @p frame. Safe to call on a partially built frame
 * (NULL members) and with @p frame == NULL.
 */
void vcatAsciiFrameFree(AsciiFrame *frame);

/**
 * Hand-off point for ASCII frames between the converter (writer) and the renderer (reader).
 *
 * The mailbox owns every frame it has been given: a frame handed out by acquireLatest() or
 * acquireFree() is only borrowed, and has to come back through release() or publish() so the
 * mailbox can free it. Any frame still held when the mailbox is destroyed is freed with it.
 *
 * The converter uses the writer side (acquireFree/publish), the renderer the reader side
 * (acquireLatest/release); there is one implementation, so the split is convention rather
 * than dynamic dispatch.
 */
typedef struct AsciiFrameMailbox AsciiFrameMailbox;

/**
 * @brief Creates a mailbox holding no frames.
 */
AsciiFrameMailbox *vcatAsciiFrameMailboxCreate(void);

/**
 * @brief Frees every frame the mailbox still owns, then the mailbox itself.
 */
void vcatAsciiFrameMailboxFree(AsciiFrameMailbox *mailbox);

/**
 * @brief Takes the newest published frame out of the mailbox.
 *
 * @param[in] mailbox Mailbox, may be NULL.
 * @param[out] outFrame Set to the frame when one was available.
 * @retval true  @p outFrame holds a frame the caller must release or publish back.
 * @retval false The mailbox is empty, is NULL, or @p outFrame is NULL.
 */
bool vcatAsciiFrameMailboxAcquireLatest(AsciiFrameMailbox *mailbox, AsciiFrame *outFrame);

/**
 * @brief Returns a frame to the free pool, oldest-release first.
 *
 * @param[in] mailbox Mailbox, may be NULL (a NULL mailbox frees @p frame).
 */
void vcatAsciiFrameMailboxRelease(AsciiFrameMailbox *mailbox, AsciiFrame frame);

/**
 * @brief Takes a frame out of the free pool for writing.
 *
 * @param[in] mailbox Mailbox, may be NULL.
 * @param[out] outFrame Set to the frame when one was available.
 * @retval true  @p outFrame holds a frame the caller must publish back.
 * @retval false The pool is empty, is NULL, or @p outFrame is NULL.
 */
bool vcatAsciiFrameMailboxAcquireFree(AsciiFrameMailbox *mailbox, AsciiFrame *outFrame);

/**
 * @brief Makes @p frame the newest frame; a displaced frame returns to the free pool.
 *
 * @param[in] mailbox Mailbox, may be NULL (a NULL mailbox frees @p frame).
 */
void vcatAsciiFrameMailboxPublish(AsciiFrameMailbox *mailbox, AsciiFrame frame);

#endif // VCAT_ASCII_FRAME_MAILBOX_H
