
#include "ascii_tui_renderer.h"
#include "input_args.h"
#include "logging.h"
#include "logging_facade.h"
#include "osd_menu_renderer.h"
#include "tui_init.h"
#include "tui_session.h"
#include "vcat_controller.h"
#include "vcat_plugin_framework.h"
#include "video_manager.h"
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "vcat_plugin_api.h"
#include "vcat_plugin_manager.h"
#include "video2ascii_converter.h"

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
    vcatLogInit();

    lfLogInfo("**** vcat: STARTING - v%s", VCAT_VERSION);

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    vcatPluginManagerInit();

    vcatPluginManagerLoadPlugins();

    VCatPluginHandle plugins[10] = {0};
    int pluginsLen = 0;
    vcatPluginManagerGetLoadedPlugins(plugins, &pluginsLen, 10);

    VCatPluginHandle targetPlugin = NULL;

    VideoSourceInfo sourceInfo = {0};
    VcatUserArgs userArgs = {
        .gradientIndex = -1,
        .color = false //
    };
    InputArgsResult result = vcatProcessInputArgs(argc,          //
                                                  argv,          //
                                                  plugins,       //
                                                  pluginsLen,    //
                                                  &targetPlugin, //
                                                  &sourceInfo,   //
                                                  &userArgs);
    if (result != ARG_PARSE_RESULT_SUCCESS)
    {
        return result == ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY ? 0 : -1;
    }

    pthread_t shutdownWatcher;
    if (pthread_create(&shutdownWatcher, NULL, watchForShutdown, NULL) != 0)
    {
        lfLogError("Something went wrong! no shutdown watcher");
        goto EXIT;
    }

    videoManager = vcatVideoManagerCreate(targetPlugin);
    tuiSession = vcatTuiSessionCreate();

    if (videoManager == NULL || tuiSession == NULL)
    {
        lfLogError("Something went wrong! %s", videoManager == NULL ? "no video manager" : "no TUI session");
        goto EXIT;
    }

    bool gradientLoaded = vcatVideo2AsciiConverterSetAsciiGradient(vcatVideoManagerGetAsciiConverter(videoManager),
                                                                   userArgs.gradientIndex);
    if (userArgs.gradientIndex >= 0 && !gradientLoaded)
    {
        lfLogError("Unable to set gradient %d; keeping the default", userArgs.gradientIndex);
    }

    if (tuiInit(videoManager, tuiSession, &sourceInfo, &userArgs) != RC_OK)
    {
        goto EXIT;
    }

    vcatVideoManagerRun(videoManager);
    vcatTuiSessionRun(tuiSession);

    shutdownRequested = true;
    pthread_join(shutdownWatcher, NULL);

    vcatVideoManagerStop(videoManager);

EXIT:
    tuiFree();

    vcatTuiSessionFree(tuiSession);
    tuiSession = NULL;
    vcatVideoManagerFree(videoManager);
    videoManager = NULL;

    for (int i = 0; i < pluginsLen; i++)
    {
        vcatVideoSourceCleanup(plugins[i]);
    }

    vcatPluginManagerUnloadPlugins();

    lfLogInfo("**** vcat: EXITING - v%s", VCAT_VERSION);

    vcatLogShutdown();

    return 0;
}
