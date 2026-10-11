
#ifndef VCAT_PLUGIN_API_H
#define VCAT_PLUGIN_API_H

#include "vcat_plugin_types.h"
#include <gst/gstelement.h>
#include <stdbool.h>


typedef struct
{
    char arg[25];
    char argShort[25];
    char description[150];
} InputArg;

typedef void *VCatPluginHandle;

// TODO: Documentation

/**
 * vcat plugin interface - general
 */

/**
 * @brief get plugin name
 *
 * @param[[in]] handle plugin handle
 * @return plugin name
 */
char *vcatPluginGetName(VCatPluginHandle handle);

/**
 * @brief get plugin type
 *
 * @param[[in]] handle plugin handle
 * @return plugin type
 */
PluginType vcatPluginGetType(VCatPluginHandle handle);

/**
 * @brief load plugin
 *
 * @param[[in]] pluginPath path to plugin library ("/path/to/mylib.so")
 * @return plugin handle
 */
VCatPluginHandle vcatPluginLoad(const char *pluginPath);

/**
 * @brief unload plugin
 *
 * @param[[in]] handle plugin handle
 * @return RC_OK if successful
 */
Rc vcatPluginUnload(VCatPluginHandle handle);

/**
 * vcat plugin interface - video source
 */

Rc vcatVideoSourceProcessInputArgs(VCatPluginHandle handle, int argc, char *argv[]);
bool vcatVideoSourceCanInitialize(VCatPluginHandle handle);
Rc vcatVideoSourceInitialize(VCatPluginHandle handle);
Rc vcatVideoSourceCleanup(VCatPluginHandle handle);
GstElement *vcatVideoSourceGetSourceElement(VCatPluginHandle handle);

#endif // VCAT_PLUGIN_API_H
