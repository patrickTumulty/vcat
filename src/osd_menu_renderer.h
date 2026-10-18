
#ifndef OSD_MENU_RENDERER_H
#define OSD_MENU_RENDERER_H

#include "tui_session.h"
#include "video_manager.h"

/**
 * @brief Renders the ASCII frame stream as a ncurses grid.
 *
 * Owns the borrowed-frame bookkeeping: at most one frame is held between
 * updates, released back to the mailbox either when a newer one arrives or
 * when the stream goes stale.
 */
typedef struct OsdTUIRenderer OsdTUIRenderer;

/**
 * @param[in] videoManager Borrowed, must outlive the renderer.
 */
OsdTUIRenderer *vcatOsdTuiRendererCreate(VideoManager *videoManager);

/**
 * @brief Releases a still-held frame, then the renderer.
 */
void vcatOsdTuiRendererFree(OsdTUIRenderer *renderer);

/**
 * @brief The interface view to register with a TUISession; the base is the first member.
 */
ITUIRenderer *vcatOsdTuiRendererAsITUIRenderer(OsdTUIRenderer *renderer);

#endif // OSD_MENU_RENDERER_H
