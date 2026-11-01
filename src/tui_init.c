
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

/// The running source, composed at startup; the header of the video info overlay.
static VideoSourceInfo sourceInfo;

/// Composes the video info text: the source header, then the caps (may be empty).
static void formatVideoInfo(char *out, size_t len, const char *capsPart)
{
    snprintf(out, len, "plugin=%s\nsource=%s\n%s", sourceInfo.name, sourceInfo.args,
             capsPart != NULL ? capsPart : "");
}

static void onCapsChange(const char *capsStr)
{
    if (osdTuiRenderer == NULL)
    {
        return;
    }

    char capsPart[1024];
    snprintf(capsPart, sizeof(capsPart), "%s", capsStr != NULL ? capsStr : "");

    for (char *p = capsPart; *p; ++p)
    {
        if (p[0] == ',' && p[1] == ' ')
        {
            *p++ = '\n';
            memmove(p, p + 1, strlen(p));
        }
    }

    char videoInfo[1025];
    formatVideoInfo(videoInfo, sizeof(videoInfo), capsPart);

    vcatOsdTuiRendererSetVideoInfoString(osdTuiRenderer, videoInfo);
}

Rc tuiInit(VideoManager *videoManager, TUISession *tuiSession, const VideoSourceInfo *sourceInfoIn)
{
    if (sourceInfoIn != NULL)
    {
        sourceInfo = *sourceInfoIn;
    }
    sourceInfo.name = sourceInfo.name != NULL ? sourceInfo.name : "unknown";

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

    // Seed the overlay with the source info before the first caps event arrives.
    char videoInfo[1025];
    formatVideoInfo(videoInfo, sizeof(videoInfo), NULL);
    vcatOsdTuiRendererSetVideoInfoString(osdTuiRenderer, videoInfo);

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
