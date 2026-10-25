
#include "crox_types.h"
#include "file_utils.h"
#include "logging_facade.h"
#include "vcat_plugin_framework.h"
#include <getopt.h>
#include <gst/gstbin.h>
#include <gst/gstelement.h>
#include <gst/gstpad.h>
#include <gst/gstutils.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;
static char gFilePath[PATH_MAX] = "";

VCAT_DEFINE_PLUGIN_NAME("File Source")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)
VCAT_DEFINE_PLUGIN_VERSION(VCAT_VERSION)
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("file")
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION("Play a video file")

static void videoSourcePrintHelp()
{
    printf("file - Play a video file\n"
           "\n"
           "Usage: vcat file <path> [OPTIONS]\n"
           "\n"
           "The path may be relative to the working directory\n"
           "(vcat file ./clip.mp4).\n"
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
    static const struct option cliOptions[] = {
        {"version", no_argument, NULL, 'v'},
        {"help",    no_argument, NULL, 'h'},
        {NULL,      0,           NULL, 0  }  //
    };

    if (argc <= 1)
    {
        return ARG_PARSE_RESULT_ERROR;
    }

    int opt;
    while ((opt = getopt_long(argc, argv, "+hv", cliOptions, NULL)) != -1)
    {
        switch (opt)
        {
        case 'h':
            videoSourcePrintHelp();
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case 'v':
            printf("file v%s\n", VCAT_VERSION);
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        default:
            fprintf(stderr, "Unknown option. See 'vcat file --help'.\n");
            return ARG_PARSE_RESULT_ERROR;
        }
    }

    snprintf(gFilePath, sizeof(gFilePath), "%s", argv[1]);

    lfLogInfo("File Source: '%s'", gFilePath);

    if (!fileExists(gFilePath))
    {
        fprintf(stderr, "File does not exist: '%s'\n", gFilePath);
        return ARG_PARSE_RESULT_ERROR;
    }

    return ARG_PARSE_RESULT_SUCCESS;
}

static void decodeBinPadAdded(GstElement *_, GstPad *newPad, gpointer userData)
{
    GstElement *sink = GST_ELEMENT(userData);

    GstCaps *caps = gst_pad_get_current_caps(newPad);
    if (!caps)
    {
        caps = gst_pad_query_caps(newPad, NULL);
        if (!caps)
        {
            return;
        }
    }

    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *name = gst_structure_get_name(structure);

    gchar *capsStr = gst_caps_to_string(caps);
    lfLogInfo("decodebin pad: %s", capsStr);
    g_free(capsStr);

    if (!g_str_has_prefix(name, "video/"))
    {
        lfLogInfo("Skipping non-video pad '%s'", name);
        goto EXIT;
    }

    GstPad *sinkPad = gst_element_get_static_pad(sink, "sink");

    if (gst_pad_is_linked(sinkPad))
    {
        lfLogWarn("Unable to link new pad: already linked");
    }
    else if (gst_pad_link(newPad, sinkPad) != GST_PAD_LINK_OK)
    {
        lfLogError("Failed to link decodebin -> %s", GST_ELEMENT_NAME(sink));
    }
    else
    {
        lfLogInfo("Linked decodebin -> %s", GST_ELEMENT_NAME(sink));
    }

    gst_object_unref(sinkPad);

EXIT:

    gst_caps_unref(caps);
}

/**
 * @brief Video Source Initialize
 *
 * Builds: filesrc -> decodebin, with decodebin's pads linked to the queue
 * that is exposed to the main pipeline.
 *
 * @return RC_OK if successful
 */
Rc videoSourceInitialize()
{
    char *failMessage = "Unable to initialize file video source";

    gSourceBin = gst_bin_new("video_src_bin");
    VERIFY_ELEMENT(gSourceBin, failMessage);

    GstElement *source = gst_element_factory_make("filesrc", NULL);
    VERIFY_ELEMENT(source, failMessage);

    g_object_set(source,                //
                 "location", gFilePath, //
                 NULL);

    GstElement *decoder = gst_element_factory_make("decodebin", NULL);
    VERIFY_ELEMENT(decoder, failMessage);

    GstElement *sync = gst_element_factory_make("clocksync", NULL);
    VERIFY_ELEMENT(sync, failMessage);

    GstElement *queue = gst_element_factory_make("queue", NULL);
    VERIFY_ELEMENT(queue, failMessage);

    gSourceElement = queue;

    gst_bin_add_many(GST_BIN(gSourceBin), //
                     source,              //
                     decoder,             //
                     sync,                //
                     queue,               //
                     NULL);

    if (!gst_element_link(source, decoder))
    {
        lfLogError("%s: failed to link filesrc -> decodebin", failMessage);
        return RC_ERROR;
    }

    if (!gst_element_link(sync, queue))
    {
        lfLogError("%s: failed to link videorate -> queue", failMessage);
        return RC_ERROR;
    }

    g_signal_connect(decoder, "pad-added", G_CALLBACK(decodeBinPadAdded), sync);

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
