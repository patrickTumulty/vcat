
#ifndef VCAT_OSD_MENU_RENDERER_H
#define VCAT_OSD_MENU_RENDERER_H

#include "ascii_gradients.h"
#include "color_utils.h"
#include "key_bindings.h"
#include "tui_session.h"

typedef struct OsdTUIRenderer OsdTUIRenderer;

OsdTUIRenderer *vcatOsdTuiRendererCreate(void);

void vcatOsdTuiRendererFree(OsdTUIRenderer *renderer);

ITUIRenderer *vcatOsdTuiRendererAsITUIRenderer(OsdTUIRenderer *renderer);

void vcatOsdTuiRendererToggleHelp(OsdTUIRenderer *renderer);

void vcatOsdTuiRendererToggleOverlays(OsdTUIRenderer *renderer);

#define VCAT_OSD_TUI_MAX_VIDEO_INFO_STR 2048

void vcatOsdTuiRendererSetVideoInfoString(OsdTUIRenderer *renderer, const char *videoInfoString);

void vcatOsdTuiRendererToggleVideoInfo(OsdTUIRenderer *renderer);

void vcatOsdTuiRendererSetKeyBindings(OsdTUIRenderer *renderer, const VcatKeyBinding *bindings, int bindingsLen);

/**
 * @brief Shows the gradient list with @p gradientIndex marked, until the
 *        auto-hide timeout passes (each call re-arms it).
 */
void vcatOsdTuiRendererShowGradientMenu(OsdTUIRenderer *renderer, int gradientIndex);

bool vcatOsdTuiRendererGradientMenuActive(OsdTUIRenderer *renderer);

void vcatOsdTuiRendererSetGradients(OsdTUIRenderer *renderer, const AsciiGradient *gradients, int gradientsLen);

/**
 * @brief Shows the color mode list with @p mode marked, until the auto-hide
 *        timeout passes (each call re-arms it).
 */
void vcatOsdTuiRendererShowColorModeMenu(OsdTUIRenderer *renderer, VcatColorMode mode);

bool vcatOsdTuiRendererColorModeMenuActive(OsdTUIRenderer *renderer);

#endif // VCAT_OSD_MENU_RENDERER_H
