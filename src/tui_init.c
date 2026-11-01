
#include "tui_init.h"
#include "ascii_tui_renderer.h"
#include "crox_types.h"
#include "key_bindings.h"
#include "logging_facade.h"
#include "osd_menu_renderer.h"
#include "tui_session.h"
#include "vcat_controller.h"
#include "video_manager.h"
#include <stdio.h>

static OsdTUIRenderer *osdTuiRenderer = NULL;
static AsciiTUIRenderer *asciiTuiRenderer = NULL;
static VcatController *controller = NULL;

static void onCapsChange(char *capsStr)
{
    if (osdTuiRenderer == NULL)
    {
        return;
    }

    char buffer[1025];
    snprintf(buffer, sizeof(buffer), "%s", capsStr);

    for (char *p = buffer; *p; ++p)
    {
        if (p[0] == ',' && p[1] == ' ')
        {
            *p++ = '\n';
            memmove(p, p + 1, strlen(p));
        }
    }

    vcatOsdTuiRendererSetVideoInfoString(osdTuiRenderer, buffer);
}

Rc tuiInit(VideoManager *videoManager, TUISession *tuiSession)
{
    vcatVideoManagerSetOnCapsChangeEvent(videoManager, onCapsChange);

    asciiTuiRenderer = vcatAsciiTuiRendererCreate(videoManager);
    if (asciiTuiRenderer == NULL)
    {
        lfLogError("Something went wrong! no ascii renderer");
        goto EXIT;
    }
    ITUIRenderer *asciiTuiRendererBase = vcatAsciiTuiRendererAsITUIRenderer(asciiTuiRenderer);
    vcatTuiSessionRegisterRenderer(tuiSession, asciiTuiRendererBase);

    osdTuiRenderer = vcatOsdTuiRendererCreate();
    if (osdTuiRenderer == NULL)
    {
        lfLogError("Something went wrong! no osd renderer");
        goto EXIT;
    }
    ITUIRenderer *osdTuiRendererBase = vcatOsdTuiRendererAsITUIRenderer(osdTuiRenderer);
    vcatTuiSessionRegisterRenderer(tuiSession, osdTuiRendererBase);

    char data[] = "resolution = 1920x1080\nSomething else = dank ass beats";
    vcatOsdTuiRendererSetVideoInfoString(osdTuiRenderer, data);

    controller = vcatVcatControllerCreate(tuiSession, videoManager, osdTuiRenderer, asciiTuiRenderer);
    if (controller == NULL)
    {
        lfLogError("Something went wrong! no vcat controller");
        goto EXIT;
    }
    vcatTuiSessionRegisterInputHandler(tuiSession, vcatVcatControllerAsITUIInputHandler(controller));

    // The same table dispatches keys and feeds the help menu, so the two cannot
    // drift apart.
    const VcatKeyBinding *keyBindings = NULL;
    int keyBindingsLen = 0;
    vcatVcatControllerGetKeyBindings(controller, &keyBindings, &keyBindingsLen);
    vcatOsdTuiRendererSetKeyBindings(osdTuiRenderer, keyBindings, keyBindingsLen);

    return RC_OK;

EXIT:

    tuiFree();

    return RC_ERROR;
}

Rc tuiFree(void)
{
    // The controller borrows the OSD renderer and the video manager, so it goes
    // away first.
    vcatVcatControllerFree(controller);
    controller = NULL;

    vcatOsdTuiRendererFree(osdTuiRenderer);
    osdTuiRenderer = NULL;

    vcatAsciiTuiRendererFree(asciiTuiRenderer);
    asciiTuiRenderer = NULL;

    return RC_OK;
}
