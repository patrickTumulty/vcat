
#ifndef VCAT_VCAT_CONTROLLER_H
#define VCAT_VCAT_CONTROLLER_H

#include "ascii_tui_renderer.h"
#include "key_bindings.h"
#include "osd_menu_renderer.h"
#include "tui_session.h"
#include "video_manager.h"

typedef struct VcatController VcatController;

VcatController *vcatVcatControllerCreate(TUISession *tuiSession,
                                         VideoManager *videoManager,
                                         OsdTUIRenderer *osd,
                                         AsciiTUIRenderer *asciiRenderer);

void vcatVcatControllerFree(VcatController *controller);

ITUIInputHandler *vcatVcatControllerAsITUIInputHandler(VcatController *controller);

void vcatVcatControllerGetKeyBindings(const VcatController *controller,
                                      const VcatKeyBinding **bindings,
                                      int *bindingsLen);

#endif // VCAT_VCAT_CONTROLLER_H
