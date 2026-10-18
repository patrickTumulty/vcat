
#include "ascii_tui_renderer.h"
#include "matrix.h"
#include "tui_utils.h"
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
    const int height = 5;
    const int width = 15;
    if (renderer->termSize.width < width || renderer->termSize.height < height)
    {
        return; // Terminal too small for the box; writeRegion/mvaddstr are not bounds-guarded.
    }
    const Rectangle termSize = renderer->termSize;
    const int offsetX = termSize.width - width > 0 ? (termSize.width - width) / 2 : 1;
    const int offsetY = termSize.height - height > 0 ? (termSize.height - height) / 2 : 0;
    tuiUtilsDrawBox(offsetX, offsetY, height, width);
    tuiUtilsFillRegion(offsetX + 1, offsetY + 1, height - 2, width - 2, ' ');
    mvaddstr(offsetY + 2, offsetX + 4, "NO DATA");
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
    const int offsetX =
        renderer->termSize.width - matWidth > 0 ? (renderer->termSize.width - matWidth) / 2 : 1;
    const int offsetY =
        renderer->termSize.height - matHeight > 0 ? (renderer->termSize.height - matHeight) / 2 : 0;
    const int height =
        matHeight < renderer->termSize.height ? matHeight : renderer->termSize.height;
    const int width = matWidth < renderer->termSize.width ? matWidth : renderer->termSize.width;

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            mvaddch(i + offsetY, j + offsetX, matrix_get_i8(renderer->buffer, j, i));
        }
    }
}

static void onTerminalSizeChange(ITUIRenderer *self, Rectangle newSize)
{
    AsciiTUIRenderer *renderer = (AsciiTUIRenderer *)self;
    renderer->termSize = newSize;
    vcatVideoManagerUpdateVideoBounds(renderer->videoManager, newSize);
}

static void onKeyPressed(ITUIRenderer *self, int key)
{
    (void)self;
    (void)key;
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
    renderer->base.onKeyPressed = onKeyPressed;
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
