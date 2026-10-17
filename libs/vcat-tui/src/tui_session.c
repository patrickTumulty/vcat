
#include "tui_session.h"
#include "logging.h"
#include "logging_facade.h"
#include <ncurses.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define TUI_REFRESH_RATE_HZ 35.0f

/// How many renderers the session drives; registering more fails.
#define MAX_RENDERERS 8

struct TUISession
{
    Rectangle currentTermSize;
    long updateDeltaMillis;
    atomic_bool running;
    ITUIRenderer *renderers[MAX_RENDERERS];
};

static long refreshDeltaMillis(void)
{
    return (long)(1000.0f / TUI_REFRESH_RATE_HZ);
}

TUISession *vcatTuiSessionCreate(void)
{
    TUISession *session = calloc(1, sizeof(*session));
    if (session == NULL)
    {
        return NULL;
    }

    session->updateDeltaMillis = refreshDeltaMillis();
    atomic_init(&session->running, false);

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    getmaxyx(stdscr, session->currentTermSize.height, session->currentTermSize.width);

    return session;
}

void vcatTuiSessionFree(TUISession *session)
{
    if (session == NULL)
    {
        return;
    }
    endwin();
    free(session);
}

static void onTerminalSizeChange(TUISession *session)
{
    lfLogInfo("on terminal size change");
    getmaxyx(stdscr, session->currentTermSize.height, session->currentTermSize.width);
    for (int i = 0; i < MAX_RENDERERS; i++)
    {
        if (session->renderers[i] != NULL)
        {
            session->renderers[i]->onTerminalSizeChange(session->renderers[i], session->currentTermSize);
        }
    }
}

bool vcatTuiSessionRegisterRenderer(TUISession *session, ITUIRenderer *renderer)
{
    if (session == NULL || renderer == NULL)
    {
        return false;
    }
    for (int i = 0; i < MAX_RENDERERS; i++)
    {
        if (session->renderers[i] == renderer)
        {
            return true; // Already registered
        }
        if (session->renderers[i] == NULL)
        {
            session->renderers[i] = renderer;
            return true;
        }
    }
    lfLogError("TUI session renderer list is full (%d)", MAX_RENDERERS);
    return false;
}

void vcatTuiSessionUnregisterRenderer(TUISession *session, ITUIRenderer *renderer)
{
    if (session == NULL || renderer == NULL)
    {
        return;
    }
    for (int i = 0; i < MAX_RENDERERS; i++)
    {
        if (session->renderers[i] == renderer)
        {
            session->renderers[i] = NULL;
            return;
        }
    }
}

void vcatTuiSessionRun(TUISession *session)
{
    if (session == NULL)
    {
        return;
    }

    atomic_store(&session->running, true);

    lfLogInfo("** Starting TUI session");

    onTerminalSizeChange(session);

    struct timespec nextFrame;
    clock_gettime(CLOCK_MONOTONIC, &nextFrame);

    while (atomic_load(&session->running))
    {
        // Refresh deadline: the frame time is added to the last target, and a
        // target in the past is pulled back to now so a slow frame does not
        // send the loop into a catch-up spin.
        nextFrame.tv_sec += session->updateDeltaMillis / 1000;
        nextFrame.tv_nsec += (session->updateDeltaMillis % 1000) * 1000000L;
        if (nextFrame.tv_nsec >= 1000000000L)
        {
            nextFrame.tv_sec += 1;
            nextFrame.tv_nsec -= 1000000000L;
        }

        werase(stdscr);

        for (int i = 0; i < MAX_RENDERERS; i++)
        {
            if (session->renderers[i] != NULL)
            {
                session->renderers[i]->update(session->renderers[i]);
            }
        }

        wnoutrefresh(stdscr);
        doupdate();

        int ch = getch();
        if (ch == ERR)
        {
            // Do nothing - no key pressed
        }
        else if (ch == KEY_RESIZE)
        {
            onTerminalSizeChange(session);
        }
        else if (ch == 27) // ESC
        {
            lfLogInfo("Escape key hit: exiting UI loop");
            atomic_store(&session->running, false);
            break;
        }
        else
        {
            for (int i = 0; i < MAX_RENDERERS; i++)
            {
                if (session->renderers[i] != NULL)
                {
                    session->renderers[i]->onKeyPressed(session->renderers[i], ch);
                }
            }
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (nextFrame.tv_sec < now.tv_sec || (nextFrame.tv_sec == now.tv_sec && nextFrame.tv_nsec < now.tv_nsec))
        {
            nextFrame = now;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &nextFrame, NULL);
    }

    lfLogInfo("TUI session exit");
}

void vcatTuiSessionStop(TUISession *session)
{
    if (session == NULL)
    {
        return;
    }
    if (atomic_load(&session->running))
    {
        lfLogInfo("** Stopping TUI session");
        atomic_store(&session->running, false);
    }
}
