
#include "vcat_controller.h"
#include "ascii_tui_renderer.h"
#include "logging_facade.h"
#include "osd_menu_renderer.h"
#include "video2ascii_converter.h"
#include <stdlib.h>

struct VcatController
{
    ITUIInputHandler base;       // Must stay the first member: the session casts back.
    TUISession *tuiSession;      // Borrowed, must outlive the controller.
    VideoManager *videoManager;  // Borrowed, must outlive the controller.
    OsdTUIRenderer *osdRenderer; // Borrowed, must outlive the controller.
    AsciiTUIRenderer *asciiRenderer;
};

static void actionQuit(VcatController *controller)
{
    lfLogInfo("Quit key hit: exiting UI loop");
    vcatTuiSessionStop(controller->tuiSession);
}

static void actionToggleHelp(VcatController *controller)
{
    vcatOsdTuiRendererToggleHelp(controller->osdRenderer);
}

static void actionToggleOverlays(VcatController *controller)
{
    vcatOsdTuiRendererToggleOverlays(controller->osdRenderer);
}

static void actionInvertGradient(VcatController *controller)
{
    Video2AsciiConverter *converter = vcatVideoManagerGetAsciiConverter(controller->videoManager);
    vcatVideo2AsciiConverterInvertGradient(converter);
}

static void actionToggleFreezeFrame(VcatController *controller)
{
    vcatAsciiTuiRendererToggleFreezeFrame(controller->asciiRenderer);
}

static void actionToggleVideoInfo(VcatController *controller)
{
    vcatOsdTuiRendererToggleVideoInfo(controller->osdRenderer);
}

/// The single source of key handling: a key either lands here or does nothing.
/// The same rows render as the OSD help menu, so the two cannot drift apart.
static const VcatKeyBinding KEY_BINDINGS[] = {
    {27,  "ESC", "Exit program",           actionQuit             },
    {'q', "q",   "Exit program",           actionQuit             },
    {'?', "?",   "Toggle help overlay",    actionToggleHelp       },
    {'o', "o",   "Toggle all overlays",    actionToggleOverlays   },
    {'s', "s",   "Toggle show video info", actionToggleVideoInfo  },
    {'i', "i",   "Ivert gradient mapping", actionInvertGradient   },
    {'f', "f",   "Toggle freeze frame",    actionToggleFreezeFrame},
};

static void onKeyPressed(struct ITUIInputHandler *self, int key)
{
    VcatController *controller = (VcatController *)self;
    const int bindingsLen = (int)(sizeof(KEY_BINDINGS) / sizeof(KEY_BINDINGS[0]));
    for (int i = 0; i < bindingsLen; i++)
    {
        if (KEY_BINDINGS[i].key == key)
        {
            KEY_BINDINGS[i].action(controller);
            return;
        }
    }
}

VcatController *vcatVcatControllerCreate(TUISession *tuiSession,
                                         VideoManager *videoManager,
                                         OsdTUIRenderer *osd,
                                         AsciiTUIRenderer *asciiRenderer)
{
    if (tuiSession == NULL || videoManager == NULL || osd == NULL)
    {
        return NULL;
    }

    VcatController *controller = calloc(1, sizeof(*controller));
    if (controller == NULL)
    {
        return NULL;
    }

    controller->tuiSession = tuiSession;
    controller->videoManager = videoManager;
    controller->osdRenderer = osd;
    controller->asciiRenderer = asciiRenderer;
    controller->base.onKeyPressed = onKeyPressed;

    return controller;
}

void vcatVcatControllerFree(VcatController *controller)
{
    if (controller == NULL)
    {
        return;
    }
    free(controller);
}

ITUIInputHandler *vcatVcatControllerAsITUIInputHandler(VcatController *controller)
{
    return controller != NULL ? &controller->base : NULL;
}

void vcatVcatControllerGetKeyBindings(const VcatController *controller,
                                      const VcatKeyBinding **bindings,
                                      int *bindingsLen)
{
    if (bindings != NULL)
    {
        *bindings = controller != NULL ? KEY_BINDINGS : NULL;
    }
    if (bindingsLen != NULL)
    {
        *bindingsLen = controller != NULL ?                                        //
                           (int)(sizeof(KEY_BINDINGS) / sizeof(KEY_BINDINGS[0])) : //
                           0;
    }
}
