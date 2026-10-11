
#include "vcat_plugin_manager.h"
#include "vcat_plugin_api.h"
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

Rc vcatPluginManagerInit()
{
    if (internal != NULL)
    {
        return RC_FAIL; // Only one instance allowed
    }
    internal = (PluginManagerInternal *)calloc(1, sizeof(PluginManagerInternal));
    return internal == NULL ? RC_MEM_ALLOC_ERROR : RC_OK;
}

Rc vcatPluginManagerFree()
{
    if (!internal)
    {
        return RC_OK;
    }
    free(internal);
    internal = NULL;
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

    char cwd[256] = "";
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        return RC_FAIL;
    }

    char dirPath[512] = "";
    char fullPath[1024] = "";
    struct stat pathStat = {0};
    snprintf(dirPath, sizeof(dirPath), "%s/plugins", cwd);

    DIR *dir = opendir(dirPath);
    if (dir == NULL)
    {
        return RC_FAIL;
    }

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
            // Unable to load plugin
            continue;
        }

        if (internal->loadedPluginsCount >= MAX_PLUGINS)
        {
            fprintf(stderr, "Warning: Maximum plugin limit reached. Skipping remaining files. %d=10\n",
                    internal->loadedPluginsCount);
            break;
        }

        internal->loadedPlugins[internal->loadedPluginsCount++] = handle;
    }

    internal->pluginsLoaded = true;

    closedir(dir);

    return RC_OK;
}

Rc vcatPluginManagerUnloadPlugins()
{
    return RC_OK; // TODO
}

int vcatPluginManagerGetLoadedPluginsCount()
{
    return internal == NULL ? 0 : internal->loadedPluginsCount;
}

Rc vcatPluginManagerGetLoadedPlugins(VCatPluginHandle *handlesArray, int *handlesArrayLen, const int handlesArrayMaxLen)
{
    if (handlesArray == NULL || handlesArrayLen == NULL || handlesArrayMaxLen == 0)
    {
        return RC_BAD_INPUT_ARGS;
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
