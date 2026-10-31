#include "crox_types.h"
#include "gst/gstbin.h"
#include "gst/gstelement.h"
#include "gst/gstutils.h"
#include "gst/gstvalue.h"
#include "logging_facade.h"
#include "vcat_plugin_framework.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;

VCAT_DEFINE_PLUGIN_NAME("Test Video Source")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)
VCAT_DEFINE_PLUGIN_VERSION(VCAT_VERSION)
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("test")
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION("Generate a test video pattern")

static void videoSourcePrintHelp()
{
    printf("test - Generate a test video pattern\n"
           "\n"
           "Usage: vcat test [OPTIONS]\n"
           "\n"
           "Options:\n"
           "  -h, --help      Show this help\n"
           "  -v, --version   Show version information\n");
}

/**
 * @brief process the source's command line, e.g. "vcat test [OPTIONS]"
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
            printf("test v%s\n", VCAT_VERSION);
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        }

        fprintf(stderr, "Unknown option '%s'. See 'vcat test --help'.\n", argv[i]);
        return ARG_PARSE_RESULT_ERROR;
    }

    if (strcmp(argv[0], "test") == 0)
    {
        return ARG_PARSE_RESULT_SUCCESS;
    }

    fprintf(stderr, "Invalid source '%s'. See 'vcat test --help'.\n", argv[0]);
    return ARG_PARSE_RESULT_ERROR;
}

/**
 * @brief Video Source Initialize
 *
 * @return RC_OK if successful
 */
Rc videoSourceInitialize()
{
    char *failMessage = "Unable to initialize test video source";

    gSourceBin = gst_bin_new("video_src_bin");
    VERIFY_ELEMENT(gSourceBin, failMessage);

    GstElement *source = gst_element_factory_make("videotestsrc", NULL);
    VERIFY_ELEMENT(source, failMessage);

    GstElement *videoconvert = gst_element_factory_make("videoconvert", NULL);
    VERIFY_ELEMENT(videoconvert, failMessage);

    GstElement *capsfilter = gst_element_factory_make("capsfilter", NULL);
    VERIFY_ELEMENT(capsfilter, failMessage);

    g_object_set(G_OBJECT(source), //
                 "is-live", true,  //
                 "pattern", 18,    // ball=18, snow=1
                 NULL);

    GstCaps *caps = gst_caps_new_simple("video/x-raw",                         //
                                        "format", G_TYPE_STRING, "RGBA",       //
                                        "width", G_TYPE_INT, 1280,             //
                                        "height", G_TYPE_INT, 720,             //
                                        "framerate", GST_TYPE_FRACTION, 25, 1, //
                                        NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(gSourceBin), //
                     source,              //
                     videoconvert,        //
                     capsfilter,          //
                     NULL);

    if (!gst_element_link_many(source,       //
                               videoconvert, //
                               capsfilter,   //
                               NULL))
    {
        lfLogError("%s: failed to link source -> videoconvert -> capsfilter", failMessage);
        return RC_ERROR;
    }

    gSourceElement = capsfilter;

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
    return gSourceElement;
}

/**
 * @brief Video Source Get GStreamer Source Bin
 *
 * @return RC_OK if successful
 */
GstElement *videoSourceGetSourceBin()
{
    return gSourceBin;
}
