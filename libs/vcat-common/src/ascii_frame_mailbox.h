
#ifndef VCAT_ASCII_FRAME_MAILBOX_H
#define VCAT_ASCII_FRAME_MAILBOX_H

#include "crox_types.h"
#include "matrix.h"

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
bool vcatAsciiFrameMailboxAcquireLatest(AsciiFrameMailbox *mailbox, matrix_i8_t *outFrame);

/**
 * @brief Returns a frame to the free pool, oldest-release first.
 *
 * @param[in] mailbox Mailbox, may be NULL (a NULL mailbox discards @p frame).
 */
void vcatAsciiFrameMailboxRelease(AsciiFrameMailbox *mailbox, matrix_i8_t frame);

/**
 * @brief Takes a frame out of the free pool for writing.
 *
 * @param[in] mailbox Mailbox, may be NULL.
 * @param[out] outFrame Set to the frame when one was available.
 * @retval true  @p outFrame holds a frame the caller must publish back.
 * @retval false The pool is empty, is NULL, or @p outFrame is NULL.
 */
bool vcatAsciiFrameMailboxAcquireFree(AsciiFrameMailbox *mailbox, matrix_i8_t *outFrame);

/**
 * @brief Makes @p frame the newest frame; a displaced frame returns to the free pool.
 *
 * @param[in] mailbox Mailbox, may be NULL (a NULL mailbox frees @p frame).
 */
void vcatAsciiFrameMailboxPublish(AsciiFrameMailbox *mailbox, matrix_i8_t frame);

#endif // VCAT_ASCII_FRAME_MAILBOX_H
