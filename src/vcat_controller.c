
#include "vcat_controller.h"
#include "video2ascii_converter.h"
#include <stdlib.h>

struct VcatController
{
    ITUIInputHandler base;      // Must stay the first member: the session casts back.
    VideoManager *videoManager; // Borrowed, must outlive the controller.
    OsdTUIRenderer *osd;        // Borrowed, must outlive the controller.
};

/// One row of the binding table the controller dispatches keys through.
struct KeyBinding
{
    int key;
    void (*action)(VcatController *controller);
};

static void actionToggleHelp(VcatController *controller)
{
    vcatOsdTuiRendererToggleHelp(controller->osd);
}

static void actionToggleOverlays(VcatController *controller)
{
    vcatOsdTuiRendererToggleOverlays(controller->osd);
}

static void actionInvertGradient(VcatController *controller)
{
    Video2AsciiConverter *converter = vcatVideoManagerGetAsciiConverter(controller->videoManager);
    vcatVideo2AsciiConverterInvertGradient(converter);
}

/// The single source of key handling: a key either lands here or does nothing.
static const struct KeyBinding KEY_BINDINGS[] = {
    {'?', actionToggleHelp    },
    {'o', actionToggleOverlays},
    {'i', actionInvertGradient},
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

VcatController *vcatVcatControllerCreate(VideoManager *videoManager, OsdTUIRenderer *osd)
{
    if (videoManager == NULL || osd == NULL)
    {
        return NULL;
    }

    VcatController *controller = calloc(1, sizeof(*controller));
    if (controller == NULL)
    {
        return NULL;
    }

    controller->videoManager = videoManager;
    controller->osd = osd;

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
