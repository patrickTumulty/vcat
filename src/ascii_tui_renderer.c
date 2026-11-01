
#define _XOPEN_SOURCE_EXTENDED 1 // Add wide char support. Must be at the very top of the file
#include "ascii_tui_renderer.h"
#include "color_utils.h"
#include "crox_utils.h"
#include "matrix.h"
#include "tui_utils.h"
#include "utils.h"
#include <ncurses.h>
#include <stdlib.h>
#include <wchar.h> // wint_t for the %lc cell formatter

// Frames without new data before the stream is considered dead and the "NO DATA"
// message replaces the stale picture. At 35 Hz refresh this is about 2 seconds.
#define STALE_FRAME_LIMIT 70

/**
 * @brief Maps one terminal palette index onto a color pair id.
 *
 * ncurses draws with color pairs, not raw colors, and pairs are a scarce
 * terminal-wide resource. The cache is keyed by the quantized xterm-256 palette
 * index (16..255), of which there are fewer than the usual 256 pairs a terminal
 * offers, so a full cache is theoretically out of reach.
 */
typedef struct
{
    short colorIndex; // xterm-256 palette index the pair is initialized for.
    short pair;       // ncurses color pair id; 0 means the default pair.
} ColorPairEntry;

/// xterm reserves 0-15 for user-configurable system colors; we stay above them.
#define COLOR_PALETTE_MIN_INDEX 16

/// One entry per usable palette index, plus room for the sentinels.
#define COLOR_PAIR_CACHE_LEN (256 - COLOR_PALETTE_MIN_INDEX + 1)

struct AsciiTUIRenderer
{
    ITUIRenderer base; // Must stay the first member: the session casts back.
    VideoManager *videoManager;
    Rectangle termSize;

    /// Borrowed from the mailbox: released back to it before this renderer goes away.
    /// chars holds one uint32_t Unicode codepoint per cell; colors one packed
    /// 0x00RRGGBB averaged color per cell. The color matrix is only read when
    /// colorMode is on.
    AsciiFrame frame;
    int staleFrameCounter;
    bool freezeFrame;

    /// Color support is decided once, after ncurses setup, and read-only after.
    bool colorSupported;
    bool colorMode;                                 // Runtime toggle: draw cells tinted or plain.
    ColorPairEntry pairCache[COLOR_PAIR_CACHE_LEN]; // colorIndex -> pair lookup.
    int pairCacheLen;
};

static void drawNoDataSpinner(AsciiTUIRenderer *renderer)
{
    static Point p0, p1, p2, p3;
    static Point points[] = {
        {2, 0},
        {5, 0},
        {7, 1},
        {7, 2},
        {5, 3},
        {2, 3},
        {0, 2},
        {0, 1}
    };
    static int pointsLen = sizeof(points) / sizeof(points[0]);
    static int pointsIndex = 0;
    static const int SPINNER_HEIGHT = 4;
    static const int SPINNER_WIDTH = 8;
    static const u64 DWELL_MS = 180;
    static u64 lastTime = 0;
    static bool firstUpdate = true;

    if (renderer->termSize.width < SPINNER_WIDTH || renderer->termSize.height < SPINNER_HEIGHT)
    {
        // Terminal too small for the spinner
        return;
    }

    const Rectangle termSize = renderer->termSize;
    const Point offset = tuiUtilsGetCenteredOffset(SPINNER_HEIGHT, SPINNER_WIDTH, termSize.height, termSize.width);

    u64 now = crox_time_get_epoch_timestamp_ms();
    u64 delta = now - lastTime;
    if (delta >= DWELL_MS)
    {
        pointsIndex = (pointsIndex + 1) % pointsLen;
        if (firstUpdate)
        {
            // This just looks a little nicer to have all points
            // start from the same original point rather than
            // some being at 0,0 for a couple frames
            p0 = points[pointsIndex];
            p3 = p0;
            p2 = p0;
            p1 = p0;
            firstUpdate = false;
        }
        else
        {
            p3 = p2;
            p2 = p1;
            p1 = p0;
            p0 = points[pointsIndex];
        }
        lastTime = now;
    }

    mvaddwstr(offset.y + p0.y, offset.x + p0.x, L"●");
    mvaddwstr(offset.y + p1.y, offset.x + p1.x, L"●");
    mvaddwstr(offset.y + p2.y, offset.x + p2.x, L"●");
    mvaddwstr(offset.y + p3.y, offset.x + p3.x, L"●");
}

/**
 * @brief Color pair for @p colorIndex, initializing a new pair on first sight.
 *
 * @return The pair id, or 0 (the default pair) when the palette entry cannot
 *         be represented, e.g. after cache corruption or a terminal reporting
 *         fewer pairs than its palette needs.
 */
static short colorPairFor(AsciiTUIRenderer *renderer, short colorIndex)
{
    for (int i = 0; i < renderer->pairCacheLen; i++)
    {
        if (renderer->pairCache[i].colorIndex == colorIndex)
        {
            return renderer->pairCache[i].pair;
        }
    }

    const short pair = (short)(renderer->pairCacheLen + 1);
    if (renderer->pairCacheLen >= COLOR_PAIR_CACHE_LEN || pair >= COLOR_PAIRS || init_pair(pair, colorIndex, -1) == ERR)
    {
        return 0;
    }
    renderer->pairCache[renderer->pairCacheLen].colorIndex = colorIndex;
    renderer->pairCache[renderer->pairCacheLen].pair = pair;
    renderer->pairCacheLen++;
    return pair;
}

/**
 * @brief Draws one cell, tinting it through its color pair when color is on.
 */
static void drawCell(AsciiTUIRenderer *renderer, int i, int j, int y, int x)
{
    const uint32_t codepoint = matrix_get_u32(renderer->frame.chars, j, i);

    short pair = 0;
    if (renderer->colorSupported && renderer->colorMode)
    {
        pair = colorPairFor(renderer, vcatColorNearestPaletteIndex(matrix_get_u32(renderer->frame.colors, j, i)));
    }

    if (pair != 0)
    {
        attron(COLOR_PAIR(pair));
    }
    // Cells hold one Unicode codepoint each; %lc renders it through the
    // UTF-8 locale the session sets up before ncurses initializes.
    mvprintw(y, x, "%lc", (wint_t)codepoint);
    if (pair != 0)
    {
        attroff(COLOR_PAIR(pair));
    }
}

static void update(ITUIRenderer *self)
{
    AsciiTUIRenderer *renderer = (AsciiTUIRenderer *)self;
    AsciiFrameMailbox *queueReader = vcatVideoManagerGetFrameMailbox(renderer->videoManager);
    if (queueReader == NULL)
    {
        return;
    }

    AsciiFrame frame = {NULL, NULL};
    if (renderer->freezeFrame)
    {
    }
    else if (vcatAsciiFrameMailboxAcquireLatest(queueReader, &frame))
    {
        if (renderer->frame.chars != NULL)
        {
            vcatAsciiFrameMailboxRelease(queueReader, renderer->frame);
        }
        renderer->frame = frame;
        renderer->staleFrameCounter = 0;
    }
    else if (renderer->frame.chars != NULL)
    {
        renderer->staleFrameCounter++;
        if (renderer->staleFrameCounter >= STALE_FRAME_LIMIT)
        {
            vcatAsciiFrameMailboxRelease(queueReader, renderer->frame);
            renderer->frame = (AsciiFrame){NULL, NULL};
        }
    }

    if (renderer->frame.chars == NULL)
    {
        drawNoDataSpinner(renderer);
        return;
    }

    const int matWidth = matrix_get_width(renderer->frame.chars);
    const int matHeight = matrix_get_height(renderer->frame.chars);
    const Rectangle termSize = renderer->termSize;
    const Point offset = tuiUtilsGetCenteredOffset(matHeight,       //
                                                   matWidth,        //
                                                   termSize.height, //
                                                   termSize.width);
    const int height = matHeight < termSize.height ? matHeight : termSize.height;
    const int width = matWidth < termSize.width ? matWidth : termSize.width;

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            drawCell(renderer, i, j, i + offset.y, j + offset.x);
        }
    }
}

static void onTerminalSizeChange(ITUIRenderer *self, Rectangle newSize)
{
    AsciiTUIRenderer *renderer = (AsciiTUIRenderer *)self;
    renderer->termSize = newSize;
    vcatVideoManagerUpdateVideoBounds(renderer->videoManager, newSize);
}

AsciiTUIRenderer *vcatAsciiTuiRendererCreate(VideoManager *videoManager)
{
    if (videoManager == NULL)
    {
        return NULL;
    }

    AsciiTUIRenderer *renderer = calloc(1, sizeof(*renderer));
    if (renderer == NULL)
    {
        return NULL;
    }

    renderer->base.update = update;
    renderer->base.onTerminalSizeChange = onTerminalSizeChange;
    renderer->videoManager = videoManager;
    renderer->termSize.height = 0;
    renderer->termSize.width = 0;
    renderer->frame = (AsciiFrame){NULL, NULL};
    renderer->staleFrameCounter = 0;
    renderer->freezeFrame = false;
    renderer->colorSupported = has_colors();
    renderer->colorMode = false;
    renderer->pairCacheLen = 0;

    return renderer;
}

void vcatAsciiTuiRendererFree(AsciiTUIRenderer *renderer)
{
    if (renderer == NULL)
    {
        return;
    }

    if (renderer->frame.chars != NULL)
    {
        AsciiFrameMailbox *queueReader = vcatVideoManagerGetFrameMailbox(renderer->videoManager);
        if (queueReader != NULL)
        {
            vcatAsciiFrameMailboxRelease(queueReader, renderer->frame);
        }
        renderer->frame = (AsciiFrame){NULL, NULL};
    }
    free(renderer);
}

ITUIRenderer *vcatAsciiTuiRendererAsITUIRenderer(AsciiTUIRenderer *renderer)
{
    return renderer != NULL ? &renderer->base : NULL;
}

void vcatAsciiTuiRendererToggleFreezeFrame(AsciiTUIRenderer *renderer)
{
    renderer->freezeFrame = !renderer->freezeFrame;
}

void vcatAsciiTuiRendererToggleColor(AsciiTUIRenderer *renderer)
{
    renderer->colorMode = !renderer->colorMode;
}

void vcatAsciiTuiRendererSetColor(AsciiTUIRenderer *renderer, bool enabled)
{
    renderer->colorMode = enabled;
}
