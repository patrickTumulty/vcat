
#ifndef VCAT_OSD_MENU_RENDERER_H
#define VCAT_OSD_MENU_RENDERER_H

#include "tui_session.h"
#include "video_manager.h"

/**
 * @brief Renders the on-screen display overlays: the help prompt and the help menu.
 *
 * Pure overlay drawing on top of whatever other renderers put on screen; it
 * holds no video frames of its own.
 */
typedef struct OsdTUIRenderer OsdTUIRenderer;

/**
 * @brief Creates the OSD renderer.
 *
 * @param[in] videoManager Borrowed, must outlive the renderer.
 */
OsdTUIRenderer *vcatOsdTuiRendererCreate(VideoManager *videoManager);

/**
 * @brief Releases the renderer.
 */
void vcatOsdTuiRendererFree(OsdTUIRenderer *renderer);

/**
 * @brief The interface view to register with a TUISession; the base is the first member.
 */
ITUIRenderer *vcatOsdTuiRendererAsITUIRenderer(OsdTUIRenderer *renderer);

#endif // VCAT_OSD_MENU_RENDERER_H
