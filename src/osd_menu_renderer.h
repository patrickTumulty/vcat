
#ifndef VCAT_OSD_MENU_RENDERER_H
#define VCAT_OSD_MENU_RENDERER_H

#include "key_bindings.h"
#include "tui_session.h"

typedef struct OsdTUIRenderer OsdTUIRenderer;

OsdTUIRenderer *vcatOsdTuiRendererCreate(void);

void vcatOsdTuiRendererFree(OsdTUIRenderer *renderer);

ITUIRenderer *vcatOsdTuiRendererAsITUIRenderer(OsdTUIRenderer *renderer);

void vcatOsdTuiRendererToggleHelp(OsdTUIRenderer *renderer);

void vcatOsdTuiRendererToggleOverlays(OsdTUIRenderer *renderer);

void vcatOsdTuiRendererSetVideoInfoString(OsdTUIRenderer *renderer, char *videoInfoString);

void vcatOsdTuiRendererToggleVideoInfo(OsdTUIRenderer *renderer);

void vcatOsdTuiRendererSetKeyBindings(OsdTUIRenderer *renderer, const VcatKeyBinding *bindings, int bindingsLen);

#endif // VCAT_OSD_MENU_RENDERER_H
