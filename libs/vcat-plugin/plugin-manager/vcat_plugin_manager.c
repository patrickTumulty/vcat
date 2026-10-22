
#include "vcat_plugin_manager.h"
#include "file_utils.h"
#include "logging_facade.h"
#include "vcat_plugin_api.h"
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PLUGINS 10

typedef struct
{
    VCatPluginHandle loadedPlugins[MAX_PLUGINS];
    int loadedPluginsCount;
    bool pluginsLoaded;
} PluginManagerInternal;

#define CAST_TO_INTERNAL(HANDLE) ((PluginManagerInternal *)(HANDLE))

static PluginManagerInternal *internal = NULL;

// Internal cross-lib contract, defined in vcat_plugin_api.c (not in any public header).
extern void *vcatPluginInternalDlHandle(VCatPluginHandle handle);

// Host logging sink, defined by the application (logging.cpp). Weak so that consumers
// without a sink still link; plugins then keep their facade's printf fallback.
extern __attribute__((weak)) void vcatHostLog(LfLogLevel level, //
                                              const char *file, //
                                              int fileLen,      //
                                              int line,         //
                                              const char *msg,  //
                                              va_list args);

typedef void (*LfRegisterFn)(LoggingCallback callback);

/**
 * @brief Route the plugin's logging facade to the host sink.
 *
 * Every plugin statically links its own copy of the logging facade, whose callback must be
 * registered in that copy. Best effort: plugins without the facade (no lfRegister export)
 * or a host without vcatHostLog are skipped, keeping the printf fallback.
 */
static void tryRegisterLogging(VCatPluginHandle handle)
{
    if (vcatHostLog == NULL)
    {
        return;
    }

    void *dlHandle = vcatPluginInternalDlHandle(handle);
    if (dlHandle == NULL)
    {
        return;
    }

    LfRegisterFn lfRegisterFn = (LfRegisterFn)dlsym(dlHandle, "lfRegister");
    if (lfRegisterFn != NULL)
    {
        lfRegisterFn(vcatHostLog);
    }
}

Rc vcatPluginManagerInit()
{
    if (internal != NULL)
    {
        lfLogError("Plugin manager already initialized");
        return RC_ERROR; // Only one instance allowed
    }
    internal = (PluginManagerInternal *)calloc(1, sizeof(PluginManagerInternal));
    if (internal == NULL)
    {
        lfLogError("Failed to allocate plugin manager");
        return RC_MEM_ALLOC_ERROR;
    }
    lfLogDebug("Plugin manager initialized");
    return RC_OK;
}

Rc vcatPluginManagerFree()
{
    if (!internal)
    {
        return RC_OK;
    }
    free(internal);
    internal = NULL;
    lfLogDebug("Plugin manager freed");
    return RC_OK;
}

Rc vcatPluginManagerLoadPlugins()
{
    if (internal == NULL)
    {
        return RC_NOT_INITIALIZED;
    }

    if (internal->pluginsLoaded)
    {
        return RC_ALREADY_DONE;
    }

    const char *exePath = resolveExePath();
    if (exePath == NULL)
    {
        exePath = ".";
    }

    char dirPath[512] = "";
    char fullPath[1024] = "";
    struct stat pathStat = {0};
    snprintf(dirPath, sizeof(dirPath), "%s/plugins", exePath);

    DIR *dir = opendir(dirPath);
    if (dir == NULL)
    {
        lfLogError("Cannot open plugins directory '%s'", dirPath);
        return RC_ERROR;
    }

    lfLogDebug("Scanning '%s' for plugins", dirPath);

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL)
    {
        snprintf(fullPath, sizeof(fullPath), "%s/%.*s", dirPath, (int)sizeof(entry->d_name), entry->d_name);

        memset(&pathStat, 0, sizeof(pathStat));

        if (stat(fullPath, &pathStat) != 0 || !S_ISREG(pathStat.st_mode))
        {
            continue;
        }

        char *lastDot = strrchr(entry->d_name, '.');
        if (lastDot == NULL || lastDot == entry->d_name)
        {
            continue;
        }

        if (strcmp(lastDot, ".so") != 0)
        {
            continue;
        }

        VCatPluginHandle handle = vcatPluginLoad(fullPath);
        if (handle == NULL)
        {
            continue;
        }

        tryRegisterLogging(handle);

        if (internal->loadedPluginsCount >= MAX_PLUGINS)
        {
            lfLogWarn("Maximum plugin limit reached (%d), skipping remaining files", MAX_PLUGINS);
            break;
        }

        internal->loadedPlugins[internal->loadedPluginsCount++] = handle;
        lfLogInfo("Loaded plugin '%s'", vcatPluginGetName(handle));
    }

    internal->pluginsLoaded = true;

    closedir(dir);

    lfLogInfo("Plugin scan complete, %d plugin(s) loaded", internal->loadedPluginsCount);

    return RC_OK;
}

Rc vcatPluginManagerUnloadPlugins()
{
    lfLogInfo("Unloading %d plugin(s)", internal->loadedPluginsCount);
    for (int i = 0; i < internal->loadedPluginsCount; i++)
    {
        Rc rc = vcatVideoSourceCleanup(internal->loadedPlugins[i]);
        if (rc != RC_OK)
        {
            lfLogWarn("Cleanup of plugin '%s' failed", vcatPluginGetName(internal->loadedPlugins[i]));
        }
        internal->loadedPlugins[i] = NULL;
    }
    internal->loadedPluginsCount = 0;
    internal->pluginsLoaded = false;
    return RC_OK;
}

int vcatPluginManagerGetLoadedPluginsCount()
{
    return internal == NULL ? 0 : internal->loadedPluginsCount;
}

Rc vcatPluginManagerGetLoadedPlugins(VCatPluginHandle *handlesArray, int *handlesArrayLen, const int handlesArrayMaxLen)
{
    if (handlesArray == NULL || handlesArrayLen == NULL || handlesArrayMaxLen == 0)
    {
        return RC_BAD_INPUT_ARG;
    }

    if (internal == NULL || !internal->pluginsLoaded || internal->loadedPluginsCount == 0)
    {
        return RC_NOT_INITIALIZED;
    }

    int bytes = MIN(sizeof(VCatPluginHandle) * internal->loadedPluginsCount, //
                    sizeof(VCatPluginHandle) * handlesArrayMaxLen);
    memcpy(handlesArray, &internal->loadedPlugins, bytes);
    *handlesArrayLen = internal->loadedPluginsCount;

    return RC_OK;
}
