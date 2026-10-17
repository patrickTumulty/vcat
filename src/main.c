
#include "ascii_tui_renderer.h"
#include "logging.h"
#include "logging_facade.h"
#include "tui_session.h"
#include "video_manager.h"
#include <pthread.h>
#include <signal.h>

#include "vcat_plugin_api.h"
#include "vcat_plugin_manager.h"

static VideoManager *videoManager = NULL;
static TUISession *tuiSession = NULL;
static AsciiTUIRenderer *asciiTuiRenderer = NULL;

// Set by the signal handler; checked by the shutdown watcher thread.
static volatile sig_atomic_t shutdownRequested = false;

static void signalHandler(int signal)
{
    (void)signal;
    shutdownRequested = true;
}

/**
 * @brief Stops the TUI session when shutdown is requested.
 *
 * Waits in a sleep loop instead of a blocking call so that a signal arriving
 * before or after the loop still ends in the same place.
 */
static void *watchForShutdown(void *arg)
{
    (void)arg;
    while (!shutdownRequested)
    {
        struct timespec nap = {0, 50 * 1000 * 1000}; // 50 ms
        nanosleep(&nap, NULL);
    }

    lfLogInfo("Shutting down from signal interrupt");
    if (tuiSession != NULL)
    {
        vcatTuiSessionStop(tuiSession);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    vcatLogInit();

    vcatPluginManagerInit();

    vcatPluginManagerLoadPlugins();

    VCatPluginHandle handles[10] = {0};
    int handlesLen = 0;
    vcatPluginManagerGetLoadedPlugins(handles, &handlesLen, 10);

    if (handlesLen == 0)
    {
        return 0;
    }

    for (int i = 0; i < handlesLen; i++)
    {
        vcatVideoSourceProcessInputArgs(handles[i], argc, argv);
    }

    lfLogInfo("**** vcat: STARTING");

    // Registered after construction (and after initscr, so ncurses keeps its own
    // handlers): the watcher can then never fire on a null session.
    videoManager = vcatVideoManagerCreate();
    tuiSession = vcatTuiSessionCreate();

    if (videoManager == NULL || tuiSession == NULL)
    {
        lfLogError("Something went wrong! %s", videoManager == NULL ? "no video manager" : "no TUI session");
    }
    else
    {
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);

        pthread_t shutdownWatcher;
        if (pthread_create(&shutdownWatcher, NULL, watchForShutdown, NULL) == 0)
        {
            asciiTuiRenderer = vcatAsciiTuiRendererCreate(videoManager);
            if (asciiTuiRenderer != NULL)
            {
                vcatTuiSessionRegisterRenderer(tuiSession, vcatAsciiTuiRendererAsITUIRenderer(asciiTuiRenderer));
            }
            else
            {
                lfLogError("Something went wrong! no ascii renderer");
            }

            vcatVideoManagerRun(videoManager);
            vcatTuiSessionRun(tuiSession);

            // The watcher also exits on shutdown: a signal during the loop set
            // the flag, and a normal exit sets it here to join the thread.
            shutdownRequested = true;
            pthread_join(shutdownWatcher, NULL);
        }
        else
        {
            lfLogError("Something went wrong! no shutdown watcher");
        }

        vcatVideoManagerStop(videoManager);
    }

    vcatAsciiTuiRendererFree(asciiTuiRenderer);
    asciiTuiRenderer = NULL;
    vcatTuiSessionFree(tuiSession);
    tuiSession = NULL;
    vcatVideoManagerFree(videoManager);
    videoManager = NULL;

    for (int i = 0; i < handlesLen; i++)
    {
        vcatVideoSourceCleanup(handles[i]);
    }

    vcatPluginManagerUnloadPlugins();

    lfLogInfo("**** vcat: EXITING");

    vcatLogShutdown();

    return 0;
}
