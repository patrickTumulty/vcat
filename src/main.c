
#include "ascii_tui_renderer.h"
#include "input_args.h"
#include "logging.h"
#include "logging_facade.h"
#include "osd_menu_renderer.h"
#include "tui_session.h"
#include "vcat_controller.h"
#include "vcat_plugin_framework.h"
#include "video_manager.h"
#include <getopt.h>
#include <pthread.h>
#include <signal.h>

#include "vcat_plugin_api.h"
#include "vcat_plugin_manager.h"

static VideoManager *videoManager = NULL;
static TUISession *tuiSession = NULL;

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
    // Declared before the first goto EXIT so the cleanup block always sees it initialized.
    OsdTUIRenderer *osdTuiRenderer = NULL;
    AsciiTUIRenderer *asciiTuiRenderer = NULL;
    VcatController *controller = NULL;

    vcatLogInit();

    vcatPluginManagerInit();

    vcatPluginManagerLoadPlugins();

    VCatPluginHandle plugins[10] = {0};
    int pluginsLen = 0;
    vcatPluginManagerGetLoadedPlugins(plugins, &pluginsLen, 10);

    VCatPluginHandle targetPlugin = NULL;

    InputArgsResult result = vcatProcessInputArgs(argc, argv, plugins, pluginsLen, &targetPlugin);
    if (result != ARG_PARSE_RESULT_SUCCESS)
    {
        return result == ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY ? 0 : -1;
    }

    lfLogInfo("**** vcat: STARTING");

    // Registered after construction (and after initscr, so ncurses keeps its own
    // handlers): the watcher can then never fire on a null session.
    videoManager = vcatVideoManagerCreate(targetPlugin);
    tuiSession = vcatTuiSessionCreate();

    if (videoManager == NULL || tuiSession == NULL)
    {
        lfLogError("Something went wrong! %s",
                   videoManager == NULL ? "no video manager" : "no TUI session");
        goto EXIT;
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    pthread_t shutdownWatcher;
    if (pthread_create(&shutdownWatcher, NULL, watchForShutdown, NULL) != 0)
    {
        lfLogError("Something went wrong! no shutdown watcher");
        goto EXIT;
    }

    asciiTuiRenderer = vcatAsciiTuiRendererCreate(videoManager);
    if (asciiTuiRenderer == NULL)
    {
        lfLogError("Something went wrong! no ascii renderer");
        goto EXIT;
    }
    ITUIRenderer *asciiTuiRendererBase = vcatAsciiTuiRendererAsITUIRenderer(asciiTuiRenderer);
    vcatTuiSessionRegisterRenderer(tuiSession, asciiTuiRendererBase);

    osdTuiRenderer = vcatOsdTuiRendererCreate();
    if (osdTuiRenderer == NULL)
    {
        lfLogError("Something went wrong! no osd renderer");
        goto EXIT;
    }
    ITUIRenderer *osdTuiRendererBase = vcatOsdTuiRendererAsITUIRenderer(osdTuiRenderer);
    vcatTuiSessionRegisterRenderer(tuiSession, osdTuiRendererBase);

    controller = vcatVcatControllerCreate(tuiSession, videoManager, osdTuiRenderer);
    if (controller == NULL)
    {
        lfLogError("Something went wrong! no vcat controller");
        goto EXIT;
    }
    vcatTuiSessionRegisterInputHandler(tuiSession,
                                       vcatVcatControllerAsITUIInputHandler(controller));

    // The same table dispatches keys and feeds the help menu, so the two cannot
    // drift apart.
    const VcatKeyBinding *keyBindings = NULL;
    int keyBindingsLen = 0;
    vcatVcatControllerGetKeyBindings(controller, &keyBindings, &keyBindingsLen);
    vcatOsdTuiRendererSetKeyBindings(osdTuiRenderer, keyBindings, keyBindingsLen);

    vcatVideoManagerRun(videoManager);
    vcatTuiSessionRun(tuiSession);

    // The watcher also exits on shutdown: a signal during the loop set
    // the flag, and a normal exit sets it here to join the thread.
    shutdownRequested = true;
    pthread_join(shutdownWatcher, NULL);

    vcatVideoManagerStop(videoManager);

EXIT:

    // The controller borrows the OSD renderer and the video manager, so it goes
    // away first.
    vcatVcatControllerFree(controller);
    controller = NULL;

    vcatOsdTuiRendererFree(osdTuiRenderer);
    osdTuiRenderer = NULL;

    vcatAsciiTuiRendererFree(asciiTuiRenderer);
    asciiTuiRenderer = NULL;

    vcatTuiSessionFree(tuiSession);
    tuiSession = NULL;
    vcatVideoManagerFree(videoManager);
    videoManager = NULL;

    for (int i = 0; i < pluginsLen; i++)
    {
        vcatVideoSourceCleanup(plugins[i]);
    }

    vcatPluginManagerUnloadPlugins();

    lfLogInfo("**** vcat: EXITING");

    vcatLogShutdown();

    return 0;
}
