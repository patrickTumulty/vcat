
#ifndef VCAT_ASCII_TUI_RENDERER_H
#define VCAT_ASCII_TUI_RENDERER_H

#include "tui_session.h"
#include "video_manager.h"

/**
 * @brief Renders the ASCII frame stream as a ncurses grid.
 *
 * Owns the borrowed-frame bookkeeping: at most one frame is held between
 * updates, released back to the mailbox either when a newer one arrives or
 * when the stream goes stale.
 */
typedef struct AsciiTUIRenderer AsciiTUIRenderer;

/**
 * @param[in] videoManager Borrowed, must outlive the renderer.
 */
AsciiTUIRenderer *vcatAsciiTuiRendererCreate(VideoManager *videoManager);

/**
 * @brief Releases a still-held frame, then the renderer.
 */
void vcatAsciiTuiRendererFree(AsciiTUIRenderer *renderer);

/**
 * @brief The interface view to register with a TUISession; the base is the first member.
 */
ITUIRenderer *vcatAsciiTuiRendererAsITUIRenderer(AsciiTUIRenderer *renderer);

#endif // VCAT_ASCII_TUI_RENDERER_H
