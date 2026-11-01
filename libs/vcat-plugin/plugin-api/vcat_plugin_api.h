
#ifndef VCAT_PLUGIN_API_H
#define VCAT_PLUGIN_API_H

#include "crox_types.h"
#include "vcat_plugin_framework.h"
#include <gst/gstelement.h>
#include <stdbool.h>

/**
 * @brief Opaque handle to one loaded plugin.
 *
 * Obtained from vcatPluginLoad() or handed out by the plugin manager. It
 * identifies the plugin to address in every function below; the video source
 * functions additionally only accept handles whose type is
 * PLUGIN_TYPE_VIDEO_SOURCE.
 */
typedef void *VCatPluginHandle;

/**
 * vcat plugin interface - general
 */

/**
 * @brief Get the plugin's display name.
 *
 * @param[in] handle plugin handle
 * @return the plugin's display name, never NULL ("NULL" is returned if handle is NULL)
 */
const char *vcatPluginGetName(VCatPluginHandle handle);

/**
 * @brief Get the plugin's type.
 *
 * Tells which optional interface the plugin implements and therefore which
 * group of functions below (vcatVideoSource*) are valid for it.
 *
 * @param[in] handle plugin handle
 * @return the plugin type (PLUGIN_TYPE_NONE if handle is NULL)
 */
PluginType vcatPluginGetType(VCatPluginHandle handle);

/**
 * @brief Load a plugin from a shared library and resolve its interface.
 *
 * dlopen()s the library, verifies it exports the required entry symbols
 * (vcatPluginGetName, vcatPluginGetType), and binds every callback of the
 * interface matching its type. A video source plugin must export all of its
 * callbacks: if any is missing, the library is refused and NULL is returned.
 * Load errors are logged.
 *
 * @param[in] pluginPath path to the plugin library ("/path/to/mylib.so")
 * @return handle to pass to the other functions below, or NULL on failure
 */
VCatPluginHandle vcatPluginLoad(const char *pluginPath);

/**
 * @brief Close a plugin's shared library and free its handle.
 *
 * The handle is invalid after this call. Call vcatVideoSourceCleanup() first
 * for an initialized video source: this function does not run the plugin's
 * cleanup for you.
 *
 * @param[in] handle plugin handle to close (NULL is silently ignored)
 * @return RC_OK if successful
 */
Rc vcatPluginUnload(VCatPluginHandle handle);

/**
 * vcat plugin interface - video source
 *
 * A video source plugin must export its static header strings via the
 * VCAT_DEFINE_* macros in vcat_plugin_framework.h and implement the callbacks
 * below. All of them are resolved at load time by vcatPluginLoad(); a plugin
 * missing any of them fails to load.
 *
 * Expected call order:
 *   1. vcatVideoSourceProcessInputArgs() -- parse the command line (may exit
 *      early or fail; only on success does anything below run)
 *   2. vcatVideoSourceInitialize() -- allocate the source's resources
 *   3. vcatVideoSourceGetSourceBin() / vcatVideoSourceGetSourceElement() --
 *      build the pipeline
 *   4. vcatVideoSourceCleanup() -- release the source's resources at shutdown
 */

/**
 * @brief Get the source's URI help string.
 *
 * Shown in the main help under "Sources:". The host also derives the URI scheme
 * ("udp" from "udp://<host>:<port>") from it to match <source> tokens: a token
 * matches either as the bare scheme ("udp") or as a full URI on that scheme
 * ("udp://127.0.0.1:1234").
 *
 * @param[in] handle video source plugin handle
 * @return the URI help string, owned by the plugin ("UNKNOWN" if handle is
 *         NULL or not a video source)
 */
const char *vcatVideoSourceGetUriHelp(VCatPluginHandle handle);

/**
 * @brief Get the source's one-line description.
 *
 * Shown next to the URI help string in the main help under "Sources:".
 *
 * @param[in] handle video source plugin handle
 * @return the description string, owned by the plugin ("UNKNOWN" if handle is
 *         NULL or not a video source)
 */
const char *vcatVideoSourceGetDescription(VCatPluginHandle handle);

/**
 * @brief Process the source's slice of the command line.
 *
 * E.g. for "vcat udp://1.2.3.4:5 [OPTIONS]" the host hands the source the
 * "udp://1.2.3.4:5" token and everything after it. The source owns its whole
 * <SOURCE> [ARGS...] handling: it parses the source URI token (argv[0]),
 * consumes its own options with getopt_long (which starts scanning at index 1),
 * and prints its source-specific help on "-h"/"--help".
 *
 * @param[in] handle video source plugin handle
 * @param[in] argc argv count, including the <source> token in argv[0]
 * @param[in] argv the <source> token and its options
 * @return InputArgsResult:
 *         - ARG_PARSE_RESULT_SUCCESS: the source is ready for
 *           vcatVideoSourceInitialize()
 *         - ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY: the host should stop right
 *           away (usually help was printed)
 *         - ARG_PARSE_RESULT_ERROR: invalid args; the host should exit
 *           non-zero -- the source has already told the user what was wrong
 */
InputArgsResult vcatVideoSourceProcessInputArgs(VCatPluginHandle handle, int argc, char *argv[]);

/**
 * @brief Allocate the source's resources and prepare it for playback.
 *
 * Call only after vcatVideoSourceProcessInputArgs() returned
 * ARG_PARSE_RESULT_SUCCESS. Must be called before vcatVideoSourceGetSourceBin() and
 * vcatVideoSourceGetSourceElement() return usable GST objects.
 *
 * @param[in] handle video source plugin handle
 * @return RC_OK if successful
 */
Rc vcatVideoSourceInitialize(VCatPluginHandle handle);

/**
 * @brief Release the source's resources.
 *
 * The counterpart of vcatVideoSourceInitialize(); call once when the host
 * shuts down.
 *
 * @param[in] handle video source plugin handle
 * @return RC_OK if successful
 */
Rc vcatVideoSourceCleanup(VCatPluginHandle handle);

/**
 * @brief Get the GStreamer source element (the stream link point).
 *
 * The element the host links its appsink against. It lives inside the bin from
 * vcatVideoSourceGetSourceBin(); the host only links it, it never unrefs it.
 *
 * @param[in] handle video source plugin handle
 * @return the source element, or NULL if the source is not initialized (or
 *         handle is NULL / not a video source)
 */
GstElement *vcatVideoSourceGetSourceElement(VCatPluginHandle handle);

/**
 * @brief Get the GStreamer source bin -- the source's complete pipeline fragment.
 *
 * The self-contained bin (named VCAT_VIDEO_SOURCE_BIN_NAME) holding everything
 * the source contributes to the pipeline. The host adds it to its pipeline as
 * it is and links it via vcatVideoSourceGetSourceElement().
 *
 * Ownership: the host takes over the bin when it adds it to its pipeline; the
 * source must not use or unref it afterwards.
 *
 * @param[in] handle video source plugin handle
 * @return the source bin, or NULL if the source is not initialized (or handle
 *         is NULL / not a video source)
 */
GstElement *vcatVideoSourceGetSourceBin(VCatPluginHandle handle);

#endif // VCAT_PLUGIN_API_H
