
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
};

static void drawNoDataMessage(AsciiTUIRenderer *renderer)
{
    static Point p, p1, p2, p3;
    // static Point points[] = {
    //     {0, 0},
    //     {1, 0},
    //     {2, 0},
    //     {2, 1},
    //     {2, 2},
    //     {1, 2},
    //     {0, 2},
    //     {0, 1}
    // };

    // static Point points[] = {
    //     {1, 0},
    //     {2, 0},
    //     {3, 1},
    //     {3, 2},
    //     {2, 3},
    //     {1, 3},
    //     {0, 2},
    //     {0, 1}
    // };

    // static Point points[] = {
    //     {1, 0},
    //     {3, 0},
    //     {4, 1},
    //     {4, 2},
    //     {3, 3},
    //     {1, 3},
    //     {0, 2},
    //     {0, 1}
    // };

    // static Point points[] = {
    //     {1, 0},
    //     {4, 0},
    //     {5, 1},
    //     {5, 2},
    //     {4, 3},
    //     {1, 3},
    //     {0, 2},
    //     {0, 1}
    // };

    // static Point points[] = { // 12
    //     {1, 0},
    //     {2, 0},
    //     {3, 0},
    //     {4, 0},
    //     {5, 1},
    //     {5, 2},
    //     {4, 3},
    //     {3, 3},
    //     {2, 3},
    //     {1, 3},
    //     {0, 2},
    //     {0, 1}
    // };

    // static Point points[] = {
    //     {2, 0},
    //     {3, 0},
    //     {4, 0},
    //     {5, 0},
    //     {6, 1},
    //     {6, 2},
    //     {5, 3},
    //     {4, 3},
    //     {3, 3},
    //     {2, 3},
    //     {1, 2},
    //     {1, 1}
    // };

    static int pointsIndex = 0;
    static u64 lastTime = 0;

    static const u64 DWELL_MS = 150;

    const int height = 6;
    const int width = 15;
    if (renderer->termSize.width < width || renderer->termSize.height < height)
    {
        // Terminal too small for the box; tuiUtilsFillRegion/mvaddstr are not bounds-guarded.
        return;
    }
    const Rectangle termSize = renderer->termSize;
    const Point offset = tuiUtilsGetCenteredOffset(height, width, termSize.height, termSize.width);
    // tuiUtilsDrawBox(offset.x, offset.y, height, width);
    // tuiUtilsFillRegion(offset.x + 1, offset.y + 1, height - 2, width - 2, ' ');

    mvaddstr(offset.y + p.y + 1, offset.x + p.x + 1 + 4, "@");
    mvaddstr(offset.y + p1.y + 1, offset.x + p1.x + 1 + 4, "@");
    mvaddstr(offset.y + p2.y + 1, offset.x + p2.x + 1 + 4, "@");
    mvaddstr(offset.y + p3.y + 1, offset.x + p3.x + 1 + 4, "@");

    u64 now = get_timestamp_ms();
    u64 delta = now - lastTime;
    if (delta >= DWELL_MS)
    {
        pointsIndex = (pointsIndex + 1) % 12;
        p3 = p2;
        p2 = p1;
        p1 = p;
        p = points[pointsIndex];
        lastTime = now;
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

    matrix_i8_t buffer = NULL;
    if (vcatAsciiFrameMailboxAcquireLatest(queueReader, &buffer))
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
        drawNoDataMessage(renderer);
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
