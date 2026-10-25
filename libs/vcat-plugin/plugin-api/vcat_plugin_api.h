
#ifndef VCAT_PLUGIN_API_H
#define VCAT_PLUGIN_API_H

#include "crox_types.h"
#include "vcat_plugin_framework.h"
#include <gst/gstelement.h>
#include <gst/gstpipeline.h>
#include <stdbool.h>

typedef void *VCatPluginHandle;

/**
 * vcat plugin interface - general
 */

/**
 * @brief get plugin name
 *
 * @param[[in]] handle plugin handle
 * @return plugin name
 */
const char *vcatPluginGetName(VCatPluginHandle handle);

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
 *
 * A video source plugin must export string getters via the macros in
 * vcat_plugin_framework.h and implement the callbacks below. All of them are
 * resolved at load time; a plugin missing any of them fails to load.
 */

/**
 * @brief get the source's URI help string
 *
 * Shown in the main help under "Sources:". The host also derives the URI scheme
 * ("udp" from "udp://<host>:<port>") from it to match <source> tokens.
 *
 * @param[[in]] handle plugin handle
 * @return URI help string
 */
const char *vcatVideoSourceGetUriHelp(VCatPluginHandle handle);
const char *vcatVideoSourceGetDescription(VCatPluginHandle handle);

/**
 * @brief process the source's command line, e.g. "vcat udp://1.2.3.4:5 [OPTIONS]"
 *
 * The source owns its whole <SOURCE> [ARGS...] handling: it parses the source
 * URI token, consumes its own options with getopt_long (which starts scanning
 * at index 1), and prints its source-specific help on "-h"/"--help". A SUCCESS
 * return means the source is ready for videoSourceInitialize(); a
 * SUCCESS_EXIT_EARLY return means the host should stop right away (usually help
 * was printed); ERROR means invalid args and the host should exit non-zero —
 * the source has told the user what was wrong.
 *
 * @param[[in]] handle plugin handle
 * @param[[in]] argc count including the <source> token
 * @param[[in]] argv the <source> token and its options
 * @return InputArgsResult
 */
InputArgsResult vcatVideoSourceProcessInputArgs(VCatPluginHandle handle, int argc, char *argv[]);
Rc vcatVideoSourceInitialize(VCatPluginHandle handle, GstPipeline *pipelin);
Rc vcatVideoSourceCleanup(VCatPluginHandle handle);
GstElement *vcatVideoSourceGetSourceElement(VCatPluginHandle handle);

#endif // VCAT_PLUGIN_API_H
