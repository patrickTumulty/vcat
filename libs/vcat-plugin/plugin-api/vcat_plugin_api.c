
#include "vcat_plugin_api.h"
#include "crox_types.h"
#include "logging_facade.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

typedef InputArgsResult (*ProcessInputArgsCB)(int argc, char *argv[]);
typedef GstElement *(*GetElementCB)();

typedef const char *(*VCatPluginGetStringCB)();
typedef PluginType (*VCatPluginGetTypeCB)();
typedef rc_t (*RCFunctionCB)();

typedef struct
{
    VCatPluginGetStringCB getName;
    VCatPluginGetTypeCB getType;
    void *dlHandle;
} VCatPluginHeader;

typedef struct
{
    VCatPluginHeader header;
    VCatPluginGetStringCB getUriHelp;
    VCatPluginGetStringCB getDescription;
    ProcessInputArgsCB processInputArgs;
    RCFunctionCB initialize;
    RCFunctionCB cleanup;
    GetElementCB getVideoSourceElement;
    GetElementCB getVideoSourceBin;
} VideoSourcePluginInternal;

const char *vcatPluginGetName(VCatPluginHandle handle)
{
    VCatPluginHeader *header = (VCatPluginHeader *)handle;
    if (header == NULL)
    {
        return "NULL";
    }
    return header->getName();
}

PluginType vcatPluginGetType(VCatPluginHandle handle)
{
    VCatPluginHeader *header = (VCatPluginHeader *)handle;
    if (header == NULL)
    {
        return PLUGIN_TYPE_NONE;
    }
    return header->getType();
}

#define VALIDATE_VIDEO_SOURCE_PLUGIN_HANDLE_RRC(HANDLE)                        \
    if ((HANDLE) == NULL)                                                      \
        return RC_BAD_INPUT_ARG;                                               \
    if (((VCatPluginHeader *)(HANDLE))->getType() != PLUGIN_TYPE_VIDEO_SOURCE) \
        return RC_NOT_ALLOWED;

#define VALIDATE_VIDEO_SOURCE_PLUGIN_HANDLE_RPTR(HANDLE)                       \
    if ((HANDLE) == NULL)                                                      \
        return NULL;                                                           \
    if (((VCatPluginHeader *)(HANDLE))->getType() != PLUGIN_TYPE_VIDEO_SOURCE) \
        return NULL;

#define VALIDATE_VIDEO_SOURCE_PLUGIN_HANDLE_RSTR(HANDLE)                       \
    if ((HANDLE) == NULL)                                                      \
        return "UNKNOWN";                                                      \
    if (((VCatPluginHeader *)(HANDLE))->getType() != PLUGIN_TYPE_VIDEO_SOURCE) \
        return "UNKNOWN";

#define VALIDATE_VIDEO_SOURCE_PLUGIN_HANDLE_RRESULT(HANDLE)                    \
    if ((HANDLE) == NULL)                                                      \
        return ARG_PARSE_RESULT_ERROR;                                         \
    if (((VCatPluginHeader *)(HANDLE))->getType() != PLUGIN_TYPE_VIDEO_SOURCE) \
        return ARG_PARSE_RESULT_ERROR;

InputArgsResult vcatVideoSourceProcessInputArgs(VCatPluginHandle handle, int argc, char *argv[])
{
    VALIDATE_VIDEO_SOURCE_PLUGIN_HANDLE_RRESULT(handle);
    VideoSourcePluginInternal *internal = (VideoSourcePluginInternal *)handle;
    return internal->processInputArgs(argc, argv);
}

const char *vcatVideoSourceGetUriHelp(VCatPluginHandle handle)
{
    VALIDATE_VIDEO_SOURCE_PLUGIN_HANDLE_RSTR(handle);
    VideoSourcePluginInternal *internal = (VideoSourcePluginInternal *)handle;
    return internal->getUriHelp();
}

const char *vcatVideoSourceGetDescription(VCatPluginHandle handle)
{
    VALIDATE_VIDEO_SOURCE_PLUGIN_HANDLE_RSTR(handle);
    VideoSourcePluginInternal *internal = (VideoSourcePluginInternal *)handle;
    return internal->getDescription();
}

rc_t vcatVideoSourceInitialize(VCatPluginHandle handle)
{
    VALIDATE_VIDEO_SOURCE_PLUGIN_HANDLE_RRC(handle);
    VideoSourcePluginInternal *internal = (VideoSourcePluginInternal *)handle;
    return internal->initialize();
}

rc_t vcatVideoSourceCleanup(VCatPluginHandle handle)
{
    VALIDATE_VIDEO_SOURCE_PLUGIN_HANDLE_RRC(handle);
    VideoSourcePluginInternal *internal = (VideoSourcePluginInternal *)handle;
    return internal->cleanup();
}

GstElement *vcatVideoSourceGetSourceElement(VCatPluginHandle handle)
{
    VALIDATE_VIDEO_SOURCE_PLUGIN_HANDLE_RPTR(handle);
    VideoSourcePluginInternal *internal = (VideoSourcePluginInternal *)handle;
    return internal->getVideoSourceElement();
}

GstElement *vcatVideoSourceGetSourceBin(VCatPluginHandle handle)
{
    VALIDATE_VIDEO_SOURCE_PLUGIN_HANDLE_RPTR(handle);
    VideoSourcePluginInternal *internal = (VideoSourcePluginInternal *)handle;
    return internal->getVideoSourceBin();
}

rc_t vcatPluginUnload(VCatPluginHandle handle)
{
    if (handle == NULL)
    {
        return RC_OK;
    }

    VCatPluginHeader *header = (VCatPluginHeader *)handle;
    lfLogInfo("Unloading plugin '%s'", header->getName());
    dlclose(header->dlHandle);
    header->dlHandle = NULL;
    free(header);
    return RC_OK;
}

/**
 * @brief Internal cross-lib contract for the plugin manager, so it can dlsym additional
 * optional symbols (e.g. lfRegister) from a loaded plugin. Deliberately NOT declared in
 * vcat_plugin_api.h: not part of the public API.
 */
void *vcatPluginInternalDlHandle(VCatPluginHandle handle)
{
    VCatPluginHeader *header = (VCatPluginHeader *)handle;
    return header == NULL ? NULL : header->dlHandle;
}

/**
 * @brief Look up a required symbol in a loaded plugin.
 *
 * @return the symbol address, or NULL after logging an error naming the
 *         symbol and plugin.
 */
static void *attemptLinkSymbol(void *dlHandle, const char *pluginName, const char *symbol)
{
    void *symbolAddress = dlsym(dlHandle, symbol);
    if (symbolAddress == NULL)
    {
        lfLogError("Missing symbol '%s' in plugin '%s'", symbol, pluginName);
        return NULL;
    }
    return symbolAddress;
}

static VCatPluginHandle initVideoSourcePlugin(const VCatPluginHeader *header)
{
    VideoSourcePluginInternal *internal = (VideoSourcePluginInternal *)malloc(sizeof(VideoSourcePluginInternal));
    if (internal == NULL)
    {
        lfLogError("Failed to allocate video source plugin '%s'", header->getName());
        return NULL;
    }

    memcpy(&internal->header, header, sizeof(VCatPluginHeader));

    void *dlHandle = internal->header.dlHandle;
    const char *name = internal->header.getName();

    internal->initialize = (RCFunctionCB)attemptLinkSymbol(dlHandle, name, "videoSourceInitialize");
    if (internal->initialize == NULL)
    {
        goto ERROR_OUT;
    }

    internal->cleanup = (RCFunctionCB)attemptLinkSymbol(dlHandle, name, "videoSourceCleanup");
    if (internal->cleanup == NULL)
    {
        goto ERROR_OUT;
    }

    internal->processInputArgs = (ProcessInputArgsCB)attemptLinkSymbol(dlHandle, name, "videoSourceProcessInputArgs");
    if (internal->processInputArgs == NULL)
    {
        goto ERROR_OUT;
    }

    internal->getVideoSourceElement = (GetElementCB)attemptLinkSymbol(dlHandle, name, "videoSourceGetSourceElement");
    if (internal->getVideoSourceElement == NULL)
    {
        goto ERROR_OUT;
    }

    internal->getVideoSourceBin = (GetElementCB)attemptLinkSymbol(dlHandle, name, "videoSourceGetSourceBin");
    if (internal->getVideoSourceBin == NULL)
    {
        goto ERROR_OUT;
    }

    internal->getUriHelp = (VCatPluginGetStringCB)attemptLinkSymbol(dlHandle, name, "videoSourceGetUriHelp");
    if (internal->getUriHelp == NULL)
    {
        goto ERROR_OUT;
    }

    internal->getDescription = (VCatPluginGetStringCB)attemptLinkSymbol(dlHandle, name, "videoSourceGetDescription");
    if (internal->getDescription == NULL)
    {
        goto ERROR_OUT;
    }

    lfLogDebug("Initialized video source plugin '%s'", internal->header.getName());

    return internal;

ERROR_OUT:

    if (internal != NULL)
    {
        free(internal);
    }

    return NULL;
}

VCatPluginHandle vcatPluginLoad(const char *pluginPath)
{
    rc_t rc = RC_OK;
    VCatPluginHeader header;
    VCatPluginHandle handle = NULL;

    lfLogInfo("Loading video source plugin at path: '%s'", pluginPath);

    void *dlHandle = dlopen(pluginPath, RTLD_NOW | RTLD_LOCAL);
    char *error;
    if (dlHandle == NULL)
    {
        lfLogError("Failed to dlopen '%s': %s", pluginPath, dlerror());
        return NULL;
    }

    header.dlHandle = dlHandle;

    dlerror(); // Clear any previous error

    header.getName = (VCatPluginGetStringCB)dlsym(dlHandle, "vcatPluginGetName");
    if (!header.getName)
    {
        lfLogError("Missing symbol 'vcatPluginGetName' in '%s'", pluginPath);
        rc = RC_ERROR;
        goto EXIT;
    }

    header.getType = (VCatPluginGetTypeCB)dlsym(dlHandle, "vcatPluginGetType");
    if (!header.getType)
    {
        lfLogError("Missing symbol 'vcatPluginGetType' in '%s'", pluginPath);
        rc = RC_ERROR;
        goto EXIT;
    }

    switch (header.getType())
    {
    case PLUGIN_TYPE_VIDEO_SOURCE:
        handle = initVideoSourcePlugin(&header);
        break;
    default:
        lfLogError("Unsupported plugin type %d in '%s'", header.getType(), pluginPath);
        rc = RC_ERROR;
        break;
    }

EXIT:

    error = dlerror();
    if (error != NULL)
    {
        lfLogError("dlsym error in '%s': %s", pluginPath, error);
        dlclose(dlHandle);
    }

    if (rc != RC_OK)
    {
        free(handle);
        handle = NULL;
    }

    return handle;
}
