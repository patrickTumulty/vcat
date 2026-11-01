
#define _XOPEN_SOURCE_EXTENDED 1 // Add wide char support. Must be at the very top of the file
#include "ascii_tui_renderer.h"
#include "crox_utils.h"
#include "matrix.h"
#include "tui_utils.h"
#include "utils.h"
#include <ncurses.h>
#include <stdlib.h>

// Frames without new data before the stream is considered dead and the "NO DATA"
// message replaces the stale picture. At 35 Hz refresh this is about 2 seconds.
#define STALE_FRAME_LIMIT 70

struct AsciiTUIRenderer
{
    ITUIRenderer base; // Must stay the first member: the session casts back.
    VideoManager *videoManager;
    Rectangle termSize;

    /// Borrowed from the mailbox: released back to it before this renderer goes away.
    matrix_i8_t buffer;
    int staleFrameCounter;
    bool freezeFrame;
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

    u64 now = get_timestamp_ms();
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

static void update(ITUIRenderer *self)
{
    AsciiTUIRenderer *renderer = (AsciiTUIRenderer *)self;
    AsciiFrameMailbox *queueReader = vcatVideoManagerGetFrameMailbox(renderer->videoManager);
    if (queueReader == NULL)
    {
        return;
    }

    matrix_i8_t buffer = NULL;
    if (renderer->freezeFrame)
    {
    }
    else if (vcatAsciiFrameMailboxAcquireLatest(queueReader, &buffer))
    {
        if (renderer->buffer != NULL)
        {
            vcatAsciiFrameMailboxRelease(queueReader, renderer->buffer);
        }
        renderer->buffer = buffer;
        renderer->staleFrameCounter = 0;
    }
    else if (renderer->buffer != NULL)
    {
        renderer->staleFrameCounter++;
        if (renderer->staleFrameCounter >= STALE_FRAME_LIMIT)
        {
            vcatAsciiFrameMailboxRelease(queueReader, renderer->buffer);
            renderer->buffer = NULL;
        }
    }

    if (renderer->buffer == NULL)
    {
        drawNoDataSpinner(renderer);
        return;
    }

    const int matWidth = matrix_get_width(renderer->buffer);
    const int matHeight = matrix_get_height(renderer->buffer);
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
            mvaddch(i + offset.y, j + offset.x, matrix_get_i8(renderer->buffer, j, i));
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
    renderer->buffer = NULL;
    renderer->staleFrameCounter = 0;
    renderer->freezeFrame = false;

    return renderer;
}

void vcatAsciiTuiRendererFree(AsciiTUIRenderer *renderer)
{
    if (renderer == NULL)
    {
        return;
    }

    if (renderer->buffer != NULL)
    {
        AsciiFrameMailbox *queueReader = vcatVideoManagerGetFrameMailbox(renderer->videoManager);
        if (queueReader != NULL)
        {
            vcatAsciiFrameMailboxRelease(queueReader, renderer->buffer);
        }
        renderer->buffer = NULL;
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
