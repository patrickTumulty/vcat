
#include "tui_init.h"
#include "ascii_tui_renderer.h"
#include "crox_types.h"
#include "crox_utils.h"
#include "input_args.h"
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
static VideoSourceInfo gSourceInfo;

/// Composes the video info text: the source header, then the caps (may be empty).
static void formatVideoInfo(char *outStr, size_t outStrLen, const char *capsPart, VideoSourceInfo sourceInfo)
{
    snprintf(outStr, outStrLen, "Plugin: %s\nArgs: %s\n\n%s", //
             CROX_STR_OR_DEFAULT(sourceInfo.name, "NULL"),    //
             CROX_STR_OR_DEFAULT(sourceInfo.args, "NULL"),    //
             CROX_STR_OR_DEFAULT(capsPart, ""));
}

static void onCapsChange(const char *capsStr)
{
    if (osdTuiRenderer == NULL)
    {
        return;
    }

    char capsPart[VCAT_OSD_TUI_MAX_VIDEO_INFO_STR];
    snprintf(capsPart, sizeof(capsPart), "%s", CROX_STR_OR_DEFAULT(capsStr, ""));

    crox_str_replace_ip(capsPart, sizeof(capsPart), ", ", "\n");

    char videoInfo[sizeof(capsPart) + 1];
    formatVideoInfo(videoInfo, sizeof(videoInfo), capsPart, gSourceInfo);

    vcatOsdTuiRendererSetVideoInfoString(osdTuiRenderer, videoInfo);
}

rc_t tuiInit(VideoManager *videoManager, TUISession *tuiSession, const VideoSourceInfo *sourceInfoIn)
{
    if (sourceInfoIn != NULL)
    {
        gSourceInfo = *sourceInfoIn;
    }

    gSourceInfo.name = CROX_STR_OR_DEFAULT(gSourceInfo.name, "unknown");

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
    formatVideoInfo(videoInfo, sizeof(videoInfo), NULL, gSourceInfo);
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
    vcatOsdTuiRendererSetGradients(osdTuiRenderer, gAsciiGradients, gAsciiGradientsLen);

    return RC_OK;

EXIT:

    tuiFree();

    return RC_ERROR;
}

rc_t tuiFree(void)
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
