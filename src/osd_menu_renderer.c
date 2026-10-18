
#include "osd_menu_renderer.h"
#include "ascii_tui_renderer.h"
#include "tui_utils.h"
#include "utils.h"
#include "video2ascii_converter.h"
#include "video_manager.h"
#include <ncurses.h>
#include <stdio.h>
#include <string.h>

struct OsdTUIRenderer
{
    ITUIRenderer base; // Must stay the first member: the session casts back.
    VideoManager *videoManager;
    Rectangle termSize;

    bool displayHelp;
    bool displayOSD;
};

#define HELP_OVERLAY_WIDTH 60
#define HELP_OVERLAY_HEIGHT 20
#define HELP_OVERLAY_SPACER 3
#define HELP_OVERLAY_PADDING 2
#define HELP_OVERLAY_INPUT_LEN 5
// 2 = bounding box, 1 = null
#define HELP_OVERLAY_DESCRIPTION_LEN                                          \
    (HELP_OVERLAY_WIDTH - HELP_OVERLAY_PADDING - HELP_OVERLAY_INPUT_LEN - 2 - \
     HELP_OVERLAY_SPACER - 1)

struct Option
{
    char input[HELP_OVERLAY_INPUT_LEN];
    char description[HELP_OVERLAY_DESCRIPTION_LEN];
};

static void drawHelpPrompt(const Rectangle termSize)
{
    const int width = 15;
    const int height = 3;
    const int offsetX = termSize.width - width;
    const int offsetY = termSize.height - height;
    if (offsetX < 0 || offsetY < 0)
    {
        return;
    }
    tuiUtilsDrawBox(offsetX, offsetY, height, width);
    mvprintw(offsetY + 1, offsetX + 1, "show help [?]");
}

static void drawHelpOverlay(const Rectangle termSize)
{
    const int height = HELP_OVERLAY_HEIGHT;
    const int width = HELP_OVERLAY_WIDTH;
    if (termSize.width < width || termSize.height < height)
    {
        return; // Terminal too small for the box; writeRegion/mvaddstr are not bounds-guarded.
    }
    const Point offset = tuiUtilsGetCenteredOffset(height, width, termSize.height, termSize.width);
    tuiUtilsDrawBox(offset.x, offset.y, height, width);
    tuiUtilsFillRegion(offset.x + 1, offset.y + 1, height - 2, width - 2, ' ');

    const char *title = "[ vcat ]";
    int titalOffsetX = offset.x + ((width - strlen(title)) / 2);
    mvaddstr(offset.y + 1, titalOffsetX, title);

    struct Option options[] = {
        {"ESC", "Exit program"           },
        {"?",   "Toggle help overlay"    },
        {"o",   "Toggle all overlays"    },
        {"i",   "Invert gradient mapping"},
    };
    int optionsLen = sizeof(options) / sizeof(struct Option);

    char optionLine[HELP_OVERLAY_WIDTH - 2 - HELP_OVERLAY_PADDING];
    for (int i = 0; i < optionsLen; i++)
    {
        const struct Option opt = options[i];
        (void)snprintf(optionLine, sizeof(optionLine), "%*s%*s%-*s", //
                       HELP_OVERLAY_INPUT_LEN, opt.input,            //
                       HELP_OVERLAY_SPACER, " ",                     //
                       HELP_OVERLAY_DESCRIPTION_LEN, opt.description);
        mvaddstr(offset.y + 3 + i, offset.x + 1 + HELP_OVERLAY_PADDING, optionLine);
    }

    char version[HELP_OVERLAY_WIDTH] = "";
    (void)snprintf(version, sizeof(version), "v%s", VCAT_VERSION);
    titalOffsetX = offset.x + ((width - strlen(version)) / 2);
    mvaddstr(offset.y + height - 2, titalOffsetX, version);
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
        drawHelpOverlay(termSize);
    }

    drawHelpPrompt(termSize);
}

static void invertGradient(struct ITUIRenderer *self)
{
    OsdTUIRenderer *renderer = (OsdTUIRenderer *)self;
    Video2AsciiConverter *converter = vcatVideoManagerGetAsciiConverter(renderer->videoManager);
    vcatVideo2AsciiConverterInvertGradient(converter);
}

static void onTerminalSizeChange(struct ITUIRenderer *self, Rectangle newSize)
{
    OsdTUIRenderer *renderer = (OsdTUIRenderer *)self;
    renderer->termSize = newSize;
}

static void onKeyPressed(struct ITUIRenderer *self, int key)
{
    OsdTUIRenderer *renderer = (OsdTUIRenderer *)self;
    switch (key)
    {
    case '?':
        renderer->displayHelp = !renderer->displayHelp;
        renderer->displayOSD = true;
        break;
    case 'o':
        renderer->displayOSD = !renderer->displayOSD;
        break;
    case 'i':
        invertGradient(self);
        break;
    }
}

/**
 * @param[in] videoManager Borrowed, must outlive the renderer.
 */
OsdTUIRenderer *vcatOsdTuiRendererCreate(VideoManager *videoManager)
{
    if (videoManager == NULL)
    {
        return NULL;
    }

    OsdTUIRenderer *renderer = calloc(1, sizeof(*renderer));
    if (renderer == NULL)
    {
        return NULL;
    }

    renderer->videoManager = videoManager;

    renderer->displayHelp = false;
    renderer->displayOSD = true;

    renderer->base.update = update;
    renderer->base.onTerminalSizeChange = onTerminalSizeChange;
    renderer->base.onKeyPressed = onKeyPressed;

    return renderer;
}

/**
 * @brief Releases a still-held frame, then the renderer.
 */
void vcatOsdTuiRendererFree(OsdTUIRenderer *renderer)
{
    if (renderer == NULL)
    {
        return;
    }
    free(renderer);
}

/**
 * @brief The interface view to register with a TUISession; the base is the first member.
 */
ITUIRenderer *vcatOsdTuiRendererAsITUIRenderer(OsdTUIRenderer *renderer)
{
    return renderer != NULL ? &renderer->base : NULL;
}
