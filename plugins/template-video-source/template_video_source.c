/**
 * @file template_video_source.c
 *
 * Template for writing your own vcat video source plugin.
 *
 * Getting started:
 *
 *   1. Copy this directory into your own project and rename the plugin:
 *      the CMake project/target (see CMakeLists.txt), this file, the
 *      "template" source URI, and every "template" string below.
 *
 *   2. Implement the five callbacks below. The host resolves every symbol
 *      at load time; if any one of them is missing the plugin is refused:
 *
 *        - videoSourceProcessInputArgs()  parse "vcat <source> [OPTIONS]"
 *        - videoSourceInitialize()        build your GStreamer source bin
 *        - videoSourceCleanup()           release your resources at shutdown
 *        - videoSourceGetSourceElement()  element the host links its appsink to
 *        - videoSourceGetSourceBin()      the complete source bin
 *
 *      The required strings are exported for you by the VCAT_DEFINE_*
 *      macros further down (plugin name, plugin type, version, URI help,
 *      and description) -- no extra work needed there.
 *
 *   3. Build the plugin and drop the resulting .so into one of vcat's
 *      plugin directories (~/.config/vcat/plugins is the easy one).
 *      See "Writing a Plugin" in the README for the full walk-through.
 *
 * For a complete, working example -- a real GStreamer pipeline instead of
 * this file's stubs -- see plugins/test-video-source/test_video_source.c.
 */

#include "crox_types.h"
#include "vcat_plugin_framework.h"
#include <gst/gstelement.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* The plugin's own version, printed by "vcat template --version" and reported
 * through vcatPluginGetVersion(). Set it to whatever fits your project; it is
 * not queried by the host, it is only for your users.
 */
#define PLUGIN_VERSION "0.0.0"

/* Required: every plugin must export a display name (vcatPluginGetName()).
 * Used in log lines and to report load failures.
 */
VCAT_DEFINE_PLUGIN_NAME("Template")

/* Required: tells the host which interface this plugin implements so it can
 * resolve the right set of callbacks. Only PLUGIN_TYPE_VIDEO_SOURCE exists
 * today.
 */
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)

/* Optional: exported as vcatPluginGetVersion(). */
VCAT_DEFINE_PLUGIN_VERSION(PLUGIN_VERSION)

/* The "template" token is what appears on the vcat command line as the source
 * name, e.g. "vcat template [OPTIONS]". The host matches <source> tokens
 * against this string: everything up to the first "://" (or the whole string
 * for sources without a URI scheme) must uniquely identify your source. It is
 * also shown in the main help under "Sources:".
 */
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("template")

/* One-line description shown next to the URI help in the main help. */
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION("Template video source")

static void videoSourcePrintHelp()
{
    printf("template - Template video source\n"
           "\n"
           "Usage: vcat template [OPTIONS]\n"
           "\n"
           "Options:\n"
           "  -h, --help      Show this help\n"
           "  -v, --version   Show version information\n");
}

/**
 * @brief process the source's command line
 *
 * The host hands this source the <source> token as typed by the user
 * (argv[0], e.g. "template" or "udp://127.0.0.1:1234") and everything after
 * it. You own your own options; validate whatever you accept here.
 *
 * @param[in] argc count, including the source URI in argv[0]
 * @param[in] argv the source URI token and its options
 *
 * @return InputArgsResult telling the host whether to continue, exit early, or fail
 */
InputArgsResult videoSourceProcessInputArgs(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            videoSourcePrintHelp();
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
        {
            printf("template v%s\n", PLUGIN_VERSION);
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        }

        /* Invalid options are the plugin's problem: print your own message. */
        fprintf(stderr, "Unknown option '%s'. See 'vcat template --help'.\n", argv[i]);
        return ARG_PARSE_RESULT_ERROR;
    }

    /* Accept only our own source token. A URI source would parse the URI here
     * (e.g. extract host:port from "udp://host:port") and store it for
     * videoSourceInitialize(). See plugins/udp-video-source for an example.
     */
    if (strcmp(argv[0], "template") == 0)
    {
        return ARG_PARSE_RESULT_SUCCESS;
    }

    fprintf(stderr, "Invalid source '%s'. See 'vcat template --help'.\n", argv[0]);
    return ARG_PARSE_RESULT_ERROR;
}

/**
 * @brief Video Source Initialize
 *
 * Only called after videoSourceProcessInputArgs() returned
 * ARG_PARSE_RESULT_SUCCESS. This is where you parse the options your source
 * collected and build your GStreamer pipeline fragment:
 *
 *   1. Create the bin the host will pull: it must be named
 *      VCAT_VIDEO_SOURCE_BIN_NAME ("videosrcbin"), e.g.
 *      gst_bin_new(VCAT_VIDEO_SOURCE_BIN_NAME).
 *   2. Create your elements, add them to the bin, and link them.
 *   3. Remember the "source element": the last element in the chain, which
 *      the host links its sink against.
 *
 * @return RC_OK if successful
 */
rc_t videoSourceInitialize()
{
    return RC_OK;
}

/**
 * @brief Video Source Cleanup
 *
 * Release everything videoSourceInitialize() created. unlink the bin and
 * unref elements only if ownership was not already handed to the host; once
 * the host added your bin to its pipeline, it owns it.
 *
 * @return RC_OK if successful
 */
rc_t videoSourceCleanup()
{
    return RC_OK;
}

/**
 * @brief Get the GStreamer source element (the stream link point).
 *
 * The host links its sink element against this one. It lives inside the bin
 * returned by videoSourceGetSourceBin(); return NULL while uninitialized.
 *
 * @return the element, or NULL when not initialized
 */
GstElement *videoSourceGetSourceElement()
{
    return NULL;
}

/**
 * @brief Get the complete GStreamer source bin
 *
 * Your whole pipeline fragment, as one self-contained bin named
 * VCAT_VIDEO_SOURCE_BIN_NAME. The host adds it to its pipeline as-is and
 * owns it from then on; it never unrefs or guesses at your internals.
 *
 * @return the bin, or NULL when not initialized
 */
GstElement *videoSourceGetSourceBin()
{
    return NULL;
}
