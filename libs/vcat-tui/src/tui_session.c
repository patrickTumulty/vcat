
#include "tui_session.h"
#include "logging_facade.h"
#include <ncurses.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

#define TUI_REFRESH_RATE_HZ 35.0f

/// How many renderers the session drives; registering more fails.
#define MAX_RENDERERS 8

/// How many input handlers the session feeds; registering more fails.
#define MAX_INPUT_HANDLERS 4

struct TUISession
{
    Rectangle currentTermSize;
    long updateDeltaMillis;
    atomic_bool running;
    ITUIRenderer *renderers[MAX_RENDERERS];
    ITUIInputHandler *inputHandlers[MAX_INPUT_HANDLERS];
    SCREEN *screen;
    FILE *terminal;
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

    // Open a direct, independent handle to the actual terminal
    FILE *terminal = fopen("/dev/tty", "r+");
    if (terminal == NULL)
    {
        free(session);
        lfLogError("Failed to open /dev/tty");
        return NULL;
    }
    session->terminal = terminal;

    // Initialize ncurses using your custom terminal handle instead of initscr()
    SCREEN *screen = newterm(NULL, terminal, terminal);
    if (screen == NULL)
    {
        free(session);
        fclose(terminal);
        lfLogError("Unable to create new terminal");
        return NULL;
    }
    session->screen = screen;
    set_term(screen);

    /*
     * We need to do this so the NVIDIA low-level multimedia hardware-acceleration
     * libraries that print a bunch of stdout doesn't interfere with ncurses rendering.
     * This does however mean that this output is suppressed and discarded. Hope that
     * wasn't important.
     */
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // initscr();
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
    delscreen(session->screen);
    fclose(session->terminal);
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

bool vcatTuiSessionRegisterInputHandler(TUISession *session, ITUIInputHandler *handler)
{
    if (session == NULL || handler == NULL)
    {
        return false;
    }
    for (int i = 0; i < MAX_INPUT_HANDLERS; i++)
    {
        if (session->inputHandlers[i] == handler)
        {
            return true; // Already registered
        }
        if (session->inputHandlers[i] == NULL)
        {
            session->inputHandlers[i] = handler;
            return true;
        }
    }
    lfLogError("TUI session input handler list is full (%d)", MAX_INPUT_HANDLERS);
    return false;
}

void vcatTuiSessionUnregisterInputHandler(TUISession *session, ITUIInputHandler *handler)
{
    if (session == NULL || handler == NULL)
    {
        return;
    }
    for (int i = 0; i < MAX_INPUT_HANDLERS; i++)
    {
        if (session->inputHandlers[i] == handler)
        {
            session->inputHandlers[i] = NULL;
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

    clear();

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
        else
        {
            for (int i = 0; i < MAX_INPUT_HANDLERS; i++)
            {
                if (session->inputHandlers[i] != NULL)
                {
                    session->inputHandlers[i]->onKeyPressed(session->inputHandlers[i], ch);
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
