
#include "crox_types.h"
#include "vcat_plugin_framework.h"
#include <gst/gstelement.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

VCAT_DEFINE_PLUGIN_NAME("Template")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)
VCAT_DEFINE_PLUGIN_VERSION("0.0.0")
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("template")
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
            printf("template v%s\n", VCAT_VERSION);
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        }

        fprintf(stderr, "Unknown option '%s'. See 'vcat template --help'.\n", argv[i]);
        return ARG_PARSE_RESULT_ERROR;
    }

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
 * @return RC_OK if successful
 */
Rc videoSourceInitialize()
{
    return RC_OK;
}

/**
 * @brief Video Source Cleanup
 *
 * @return RC_OK if successful
 */
Rc videoSourceCleanup()
{
    return RC_OK;
}

/**
 * @brief Video Source Get GStreamer Source Element
 *
 * @return RC_OK if successful
 */
GstElement *videoSourceGetSourceElement()
{
    return NULL;
}

/**
 * @brief Video Source Get GStreamer Source Bin
 *
 * @return RC_OK if successful
 */
GstElement *videoSourceGetSourceBin()
{
    return NULL;
}
