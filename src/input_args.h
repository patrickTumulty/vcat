
#ifndef INPUT_ARGS_H
#define INPUT_ARGS_H

#include "vcat_plugin_api.h"
#include "vcat_plugin_framework.h"
#include <stddef.h>

/// The running video source: which plugin it is, and the raw "<SOURCE> [ARGS...]"
/// tokens it was started with (joined with spaces). name is borrowed from the
/// plugin; args is owned by the struct.
typedef struct VideoSourceInfo
{
    const char *name;
    char args[128];
} VideoSourceInfo;

InputArgsResult vcatProcessInputArgs(int argc, char *argv[], VCatPluginHandle *plugins, const int pluginsLen,
                                     VCatPluginHandle *targetPlugin, VideoSourceInfo *sourceInfo,
                                     int *gradientIndex);

#endif // INPUT_ARGS_H
