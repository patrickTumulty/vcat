
#include "osd_menu_renderer.h"
#include "crox_utils.h"
#include "key_bindings.h"
#include "logging_facade.h"
#include "tui_utils.h"
#include "utils.h"
#include <glib.h>
#include <ncurses.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct VideoInfoData
{
    char str[VCAT_OSD_TUI_MAX_VIDEO_INFO_STR];
    int lines;
    int largestWidth;
};

struct OsdTUIRenderer
{
    ITUIRenderer base; // Must stay the first member: the session casts back.
    Rectangle termSize;

    bool displayHelp;
    bool displayOSD;
    bool displayVideoInfo;
    bool displayGradientMenu;

    const VcatKeyBinding *keyBindings;
    int keyBindingsLen;

    /// Gradient rows for the gradient menu; borrowed, must outlive the renderer.
    const AsciiGradient *gradients;
    int gradientsLen;
    int currentGradientIndex;
    u64 hideGradientMenuAtMs;

    struct VideoInfoData videoInfoData;
    pthread_mutex_t videoInfoDataMutex;
};

#define HELP_PROMPT_WIDTH 15
#define HELP_PROMPT_HEIGHT 3
#define HELP_OVERLAY_WIDTH 60
#define HELP_OVERLAY_HEIGHT 20
#define HELP_OVERLAY_SPACER 3
#define HELP_OVERLAY_PADDING 2
#define HELP_OVERLAY_INPUT_LEN 5
// 2 = bounding box, 1 = null
#define HELP_OVERLAY_DESCRIPTION_LEN \
    (HELP_OVERLAY_WIDTH - HELP_OVERLAY_PADDING - HELP_OVERLAY_INPUT_LEN - 2 - HELP_OVERLAY_SPACER - 1)

static void drawHelpPrompt(const Rectangle termSize)
{
    const int offsetX = termSize.width - HELP_PROMPT_WIDTH;
    const int offsetY = termSize.height - HELP_PROMPT_HEIGHT;
    if (offsetX < 0 || offsetY < 0)
    {
        return;
    }
    tuiUtilsDrawBox(offsetX, offsetY, HELP_PROMPT_HEIGHT, HELP_PROMPT_WIDTH);
    mvprintw(offsetY + 1, offsetX + 1, "show help [?]");
}

static void drawHelpOverlay(const OsdTUIRenderer *renderer)
{
    const Rectangle termSize = renderer->termSize;
    const int height = HELP_OVERLAY_HEIGHT;
    const int width = HELP_OVERLAY_WIDTH;
    if (termSize.width < width || termSize.height < height)
    {
        // Terminal too small for the box; tuiUtilsFillRegion/mvaddstr are not bounds-guarded.
        return;
    }
    const Point offset = tuiUtilsGetCenteredOffset(height, width, termSize.height, termSize.width);
    tuiUtilsDrawBox(offset.x, offset.y, height, width);
    tuiUtilsFillRegion(offset.x + 1, offset.y + 1, height - 2, width - 2, ' ');

    const char *title = "[ vcat ]";
    int titleOffsetX = offset.x + ((width - strlen(title)) / 2);
    mvaddstr(offset.y + 1, titleOffsetX, title);

    // The controller's binding table in menu form: the keys, and what they do.
    char optionLine[HELP_OVERLAY_WIDTH - 2 - HELP_OVERLAY_PADDING];
    for (int i = 0; i < renderer->keyBindingsLen; i++)
    {
        const VcatKeyBinding binding = renderer->keyBindings[i];
        (void)snprintf(optionLine, sizeof(optionLine), "%*s%*s%-*s", //
                       HELP_OVERLAY_INPUT_LEN, binding.input,        //
                       HELP_OVERLAY_SPACER, " ",                     //
                       HELP_OVERLAY_DESCRIPTION_LEN, binding.description);
        mvaddstr(offset.y + 3 + i, offset.x + 1 + HELP_OVERLAY_PADDING, optionLine);
    }

    char version[HELP_OVERLAY_WIDTH] = "";
    (void)snprintf(version, sizeof(version), "v%s", VCAT_VERSION);
    titleOffsetX = offset.x + ((width - strlen(version)) / 2);
    mvaddstr(offset.y + height - 2, titleOffsetX, version);
}

static void drawVideoInfoOverlay(const struct VideoInfoData *videoInfo)
{
    int height = videoInfo->lines + 4;

    const char *title = "[ video info ]";
    const int titleLen = strlen(title);

    int width = MAX(16, videoInfo->largestWidth + 2 + 2);

    int overlayOffsetX = 1;
    int overlayOffsetY = 1;

    tuiUtilsFillRegion(overlayOffsetX, overlayOffsetY, height, width, ' ');
    tuiUtilsDrawBox(overlayOffsetX, overlayOffsetY, height, width);

    int row = overlayOffsetY + 3;
    char buffer[sizeof(videoInfo->str)];
    snprintf(buffer, sizeof(buffer), "%s", videoInfo->str);
    char *newPtr = NULL;
    char *currentPtr = buffer;

    int titleOffset = ((width - 2) / 2) - (titleLen / 2);
    mvaddstr(overlayOffsetY + 1, overlayOffsetX + titleOffset + 1, title);

    while ((newPtr = strchr(currentPtr, '\n')) != NULL)
    {
        *newPtr = '\0';
        mvaddstr(row++, overlayOffsetX + 2, currentPtr);
        currentPtr = newPtr + 1;
    }
    mvaddstr(row, overlayOffsetX + 2, currentPtr);
}

#define GRADIENT_MENU_TIMEOUT_MS 2500
#define GRADIENT_MENU_WIDTH 25
// Box borders, title row, blank row under the title.
#define GRADIENT_MENU_SPACER 4

static void drawGradientMenu(const OsdTUIRenderer *renderer)
{
    const int height = renderer->gradientsLen + GRADIENT_MENU_SPACER;
    const Rectangle termSize = renderer->termSize;
    if (renderer->gradients == NULL || termSize.width < GRADIENT_MENU_WIDTH || termSize.height < height)
    {
        // Terminal too small for the box; tuiUtilsFillRegion/mvaddstr are not bounds-guarded.
        return;
    }
    const Point offset = tuiUtilsGetCenteredOffset(height, GRADIENT_MENU_WIDTH, termSize.height, termSize.width);
    tuiUtilsDrawBox(offset.x, offset.y, height, GRADIENT_MENU_WIDTH);
    tuiUtilsFillRegion(offset.x + 1, offset.y + 1, height - 2, GRADIENT_MENU_WIDTH - 2, ' ');
    mvaddstr(offset.y + 1, offset.x + 5, "[ gradient ]");

    char row[GRADIENT_MENU_WIDTH + 1];
    for (int i = 0; i < renderer->gradientsLen; i++)
    {
        (void)snprintf(row, sizeof(row), "%s%d %s", //
                       i == renderer->currentGradientIndex ? "> " : "  ", i, renderer->gradients[i].name);
        mvaddstr(offset.y + 3 + i, offset.x + 2, row);
    }
}

static void update(struct ITUIRenderer *self)
{
    OsdTUIRenderer *renderer = (OsdTUIRenderer *)self;
    const Rectangle termSize = renderer->termSize;

    if (!renderer->displayOSD)
    {
        return;
    }

    if (renderer->displayHelp)
    {
        drawHelpOverlay(renderer);
    }

    if (renderer->displayVideoInfo)
    {
        pthread_mutex_lock(&renderer->videoInfoDataMutex);
        drawVideoInfoOverlay(&renderer->videoInfoData);
        pthread_mutex_unlock(&renderer->videoInfoDataMutex);
    }

    if (renderer->displayGradientMenu)
    {
        if (crox_time_get_epoch_timestamp_ms() >= renderer->hideGradientMenuAtMs)
        {
            renderer->displayGradientMenu = false;
        }
        else
        {
            drawGradientMenu(renderer);
        }
    }

    drawHelpPrompt(termSize);
}

static void onTerminalSizeChange(struct ITUIRenderer *self, Rectangle newSize)
{
    OsdTUIRenderer *renderer = (OsdTUIRenderer *)self;
    renderer->termSize = newSize;
}

/**
 * @brief Creates the OSD renderer.
 */
OsdTUIRenderer *vcatOsdTuiRendererCreate(void)
{
    OsdTUIRenderer *renderer = calloc(1, sizeof(*renderer));
    if (renderer == NULL)
    {
        return NULL;
    }

    renderer->displayHelp = false;
    renderer->displayOSD = true;

    renderer->base.update = update;
    renderer->base.onTerminalSizeChange = onTerminalSizeChange;

    renderer->videoInfoData.largestWidth = 1;
    renderer->videoInfoData.lines = 1;
    renderer->videoInfoData.str[0] = '\0';

    if (pthread_mutex_init(&renderer->videoInfoDataMutex, NULL) != 0)
    {
        lfLogError("Unable to init video info data mutex");
        free(renderer);
        return NULL;
    }

    return renderer;
}

void vcatOsdTuiRendererFree(OsdTUIRenderer *renderer)
{
    if (renderer == NULL)
    {
        return;
    }
    free(renderer);
}

ITUIRenderer *vcatOsdTuiRendererAsITUIRenderer(OsdTUIRenderer *renderer)
{
    return renderer != NULL ? &renderer->base : NULL;
}

void vcatOsdTuiRendererToggleHelp(OsdTUIRenderer *renderer)
{
    if (renderer == NULL)
    {
        return;
    }
    renderer->displayHelp = !renderer->displayHelp;
    renderer->displayOSD = true;
}

void vcatOsdTuiRendererToggleOverlays(OsdTUIRenderer *renderer)
{
    if (renderer == NULL)
    {
        return;
    }
    renderer->displayOSD = !renderer->displayOSD;
}

void vcatOsdTuiRendererSetKeyBindings(OsdTUIRenderer *renderer, const VcatKeyBinding *bindings, int bindingsLen)
{
    if (renderer == NULL)
    {
        return;
    }
    renderer->keyBindings = bindings;
    renderer->keyBindingsLen = bindingsLen;
}

void vcatOsdTuiRendererShowGradientMenu(OsdTUIRenderer *renderer, int gradientIndex)
{
    if (renderer == NULL)
    {
        return;
    }
    renderer->displayGradientMenu = true;
    renderer->currentGradientIndex = gradientIndex;
    renderer->hideGradientMenuAtMs = crox_time_get_epoch_timestamp_ms() + GRADIENT_MENU_TIMEOUT_MS;
}

bool vcatOsdTuiRendererGradientMenuActive(OsdTUIRenderer *renderer)
{
    if (renderer == NULL)
    {
        return false;
    }
    return renderer->displayGradientMenu;
}

void vcatOsdTuiRendererSetGradients(OsdTUIRenderer *renderer, const AsciiGradient *gradients, int gradientsLen)
{
    if (renderer == NULL)
    {
        return;
    }
    renderer->gradients = gradients;
    renderer->gradientsLen = gradients != NULL ? gradientsLen : 0;
}

void vcatOsdTuiRendererSetVideoInfoString(OsdTUIRenderer *renderer, const char *videoInfoString)
{
    if (renderer == NULL)
    {
        return;
    }

    /*
     * The video info string comes in from a gstreamer callback.
     * This means it is not in the UI thread. Use a mutex to
     * ensure serialized access to video info mutex
     */
    pthread_mutex_lock(&renderer->videoInfoDataMutex);
    struct VideoInfoData *vid = &renderer->videoInfoData;
    snprintf(vid->str, sizeof(vid->str), "%s", videoInfoString != NULL ? videoInfoString : "");
    pthread_mutex_unlock(&renderer->videoInfoDataMutex);

    vid->lines = 1;
    vid->largestWidth = 1;
    int lastLineStartIndex = 0;
    int index = 0;
    char c = '\0';
    while ((c = vid->str[index]) != '\0')
    {
        if (c == '\n')
        {
            int diff = index - lastLineStartIndex;
            lastLineStartIndex = index + 1;
            vid->largestWidth = MAX(vid->largestWidth, diff);
            vid->lines++;
        }
        index++;
    }
    int diff = index - lastLineStartIndex;
    lastLineStartIndex = index;
    vid->largestWidth = MAX(vid->largestWidth, diff);
}

void vcatOsdTuiRendererToggleVideoInfo(OsdTUIRenderer *renderer)
{
    renderer->displayVideoInfo = !renderer->displayVideoInfo;
}
