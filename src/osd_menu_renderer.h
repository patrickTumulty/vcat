
#ifndef VCAT_OSD_MENU_RENDERER_H
#define VCAT_OSD_MENU_RENDERER_H

#include "tui_session.h"

/**
 * @brief Renders the on-screen display overlays: the help prompt and the help menu.
 *
 * Pure overlay drawing on top of whatever other renderers put on screen; it
 * holds no video frames of its own. The controller owns its key bindings and
 * drives it through the toggles below.
 */
typedef struct OsdTUIRenderer OsdTUIRenderer;

/**
 * @brief Creates the OSD renderer.
 */
OsdTUIRenderer *vcatOsdTuiRendererCreate(void);

/**
 * @brief Releases the renderer.
 */
void vcatOsdTuiRendererFree(OsdTUIRenderer *renderer);

/**
 * @brief The interface view to register with a TUISession; the base is the first member.
 */
ITUIRenderer *vcatOsdTuiRendererAsITUIRenderer(OsdTUIRenderer *renderer);

/**
 * @brief Shows or hides the help overlay; showing it also turns overlays back on.
 */
void vcatOsdTuiRendererToggleHelp(OsdTUIRenderer *renderer);

/**
 * @brief Shows or hides every OSD overlay.
 */
void vcatOsdTuiRendererToggleOverlays(OsdTUIRenderer *renderer);

#endif // VCAT_OSD_MENU_RENDERER_H
