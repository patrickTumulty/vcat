
#include "osd_menu_renderer.h"
#include "key_bindings.h"
#include "logging_facade.h"
#include "tui_utils.h"
#include "utils.h"
#include <glib.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct VideoInfoData
{
    char str[1024];
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

    /// The help menu rows; borrowed from the controller, must outlive the renderer.
    const VcatKeyBinding *keyBindings;
    int keyBindingsLen;
    struct VideoInfoData videoInfoData;
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

static void drawVideoInfoOverlay(const OsdTUIRenderer *renderer)
{
    int height = renderer->videoInfoData.lines + 3;
    int width = renderer->videoInfoData.largestWidth + 2;
    tuiUtilsFillRegion(1, 1, height, width, ' ');
    tuiUtilsDrawBox(1, 1, height, width);
    int row = 2;
    char buffer[sizeof(renderer->videoInfoData.str)];
    snprintf(buffer, sizeof(buffer), "%s", renderer->videoInfoData.str);
    char *newPtr = NULL;
    char *currentPtr = buffer;
    while ((newPtr = strchr(currentPtr, '\n')) != NULL)
    {
        *newPtr = '\0';
        mvaddstr(row++, 2, currentPtr);
        currentPtr = newPtr + 1;
    }
    mvaddstr(row, 2, currentPtr);
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
        drawVideoInfoOverlay(renderer);
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

void vcatOsdTuiRendererSetVideoInfoString(OsdTUIRenderer *renderer, char *videoInfoString)
{
    snprintf(renderer->videoInfoData.str, sizeof(renderer->videoInfoData.str), "%s", videoInfoString);
    renderer->videoInfoData.lines = 1;
    renderer->videoInfoData.largestWidth = 1;
    int lastLineStartIndex = 0;
    int index = 0;
    char c = '\0';
    while ((c = renderer->videoInfoData.str[index]) != '\0')
    {
        if (c == '\n')
        {
            int diff = index - lastLineStartIndex - 1;
            lastLineStartIndex = index + 1;
            renderer->videoInfoData.largestWidth = MAX(renderer->videoInfoData.largestWidth, diff);
            renderer->videoInfoData.lines++;
        }
        index++;
    }
    int diff = index - lastLineStartIndex;
    lastLineStartIndex = index;
    renderer->videoInfoData.largestWidth = MAX(renderer->videoInfoData.largestWidth, diff);
}

void vcatOsdTuiRendererToggleVideoInfo(OsdTUIRenderer *renderer)
{
    renderer->displayVideoInfo = !renderer->displayVideoInfo;
}
