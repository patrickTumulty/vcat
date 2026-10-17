
#include "vcat_plugin_api.h"
#include "crox_types.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

typedef InputArgsResult (*ProcessInputArgsCB)(int argc, char *argv[]);
typedef GstElement *(*GetElementCB)();

typedef const char *(*VCatPluginGetStringCB)();
typedef PluginType (*VCatPluginGetTypeCB)();
typedef Rc (*RCFunctionCB)();

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

Rc vcatVideoSourceInitialize(VCatPluginHandle handle)
{
    VALIDATE_VIDEO_SOURCE_PLUGIN_HANDLE_RRC(handle);
    VideoSourcePluginInternal *internal = (VideoSourcePluginInternal *)handle;
    return internal->initialize();
}

Rc vcatVideoSourceCleanup(VCatPluginHandle handle)
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

Rc vcatPluginUnload(VCatPluginHandle handle)
{
    if (handle == NULL)
    {
        return RC_OK;
    }

    VCatPluginHeader *header = (VCatPluginHeader *)handle;
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

static VCatPluginHandle initVideoSourcePlugin(const VCatPluginHeader *header)
{
    VideoSourcePluginInternal *internal = (VideoSourcePluginInternal *)malloc(sizeof(VideoSourcePluginInternal));
    if (internal == NULL)
    {
        return NULL;
    }

    memcpy(&internal->header, header, sizeof(VCatPluginHeader));

    void *dlHandle = internal->header.dlHandle;

    internal->initialize = (RCFunctionCB)dlsym(dlHandle, "videoSourceInitialize");
    if (!internal->initialize)
    {
        goto ERROR_OUT;
    }

    internal->cleanup = (RCFunctionCB)dlsym(dlHandle, "videoSourceCleanup");
    if (!internal->cleanup)
    {
        goto ERROR_OUT;
    }

    internal->processInputArgs = (ProcessInputArgsCB)dlsym(dlHandle, "videoSourceProcessInputArgs");
    if (!internal->processInputArgs)
    {
        goto ERROR_OUT;
    }

    internal->getVideoSourceElement = (GetElementCB)dlsym(dlHandle, "videoSourceGetSourceElement");
    if (!internal->getVideoSourceElement)
    {
        goto ERROR_OUT;
    }

    internal->getVideoSourceBin = (GetElementCB)dlsym(dlHandle, "videoSourceGetSourceBin");
    if (!internal->getVideoSourceBin)
    {
        goto ERROR_OUT;
    }

    internal->getUriHelp = (VCatPluginGetStringCB)dlsym(dlHandle, "videoSourceGetUriHelp");
    if (!internal->getUriHelp)
    {
        goto ERROR_OUT;
    }

    internal->getDescription = (VCatPluginGetStringCB)dlsym(dlHandle, "videoSourceGetDescription");
    if (!internal->getDescription)
    {
        goto ERROR_OUT;
    }

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
    Rc rc = RC_OK;
    VCatPluginHeader header;
    VCatPluginHandle handle;

    void *dlHandle = dlopen(pluginPath, RTLD_NOW | RTLD_LOCAL);
    char *error;
    if (dlHandle == NULL)
    {
        return NULL;
    }

    header.dlHandle = dlHandle;

    dlerror(); // Clear any previous error

    header.getName = (VCatPluginGetStringCB)dlsym(dlHandle, "vcatPluginGetName");
    if (!header.getName)
    {
        rc = RC_ERROR;
        goto EXIT;
    }

    header.getType = (VCatPluginGetTypeCB)dlsym(dlHandle, "vcatPluginGetType");
    if (!header.getType)
    {
        rc = RC_ERROR;
        goto EXIT;
    }

    switch (header.getType())
    {
    case PLUGIN_TYPE_VIDEO_SOURCE:
        handle = initVideoSourcePlugin(&header);
        break;
    default:
        // Unknown type
        break;
    }

EXIT:

    error = dlerror();
    if (error != NULL)
    {
        fprintf(stderr, "dlsym: %s\n", error);
        dlclose(dlHandle);
    }

    if (rc != RC_OK)
    {
        free(handle);
        handle = NULL;
    }

    return handle;
}
