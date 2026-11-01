
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

/**
 * @brief User selections from [OPTIONS]: one field per thing a user can ask
 *        vcat to start differently. Extend it instead of new out-params on
 *        vcatProcessInputArgs().
 */
typedef struct VcatUserArgs
{
    /// Index into gAsciiGradients, or -1 when not requested.
    int gradientIndex;
} VcatUserArgs;

InputArgsResult vcatProcessInputArgs(int argc, char *argv[], VCatPluginHandle *plugins, const int pluginsLen,
                                     VCatPluginHandle *targetPlugin, VideoSourceInfo *sourceInfo,
                                     VcatUserArgs *userArgs);

#endif // INPUT_ARGS_H
