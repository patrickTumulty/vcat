
#include "crox_types.h"
#include "file_utils.h"
#include "logging_facade.h"
#include "vcat_plugin_framework.h"
#include "vcat_video_source_utils.h"
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
static VcatRgbaChain gRgbaChain = {0};
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
    // Video pads link into the RGBA converter chain; the chain tail is already
    // connected to the exposed queue.
    GstElement *chainHead = GST_ELEMENT(userData);

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

    GstPad *sinkPad = gst_element_get_static_pad(chainHead, "sink");

    if (gst_pad_is_linked(sinkPad))
    {
        lfLogWarn("Unable to link new pad: already linked");
    }
    else if (gst_pad_link(newPad, sinkPad) != GST_PAD_LINK_OK)
    {
        lfLogError("Failed to link decodebin -> %s", GST_ELEMENT_NAME(chainHead));
    }
    else
    {
        lfLogInfo("Linked decodebin -> %s", GST_ELEMENT_NAME(chainHead));
    }

    gst_object_unref(sinkPad);

EXIT:

    gst_caps_unref(caps);
}

/**
 * @brief Video Source Initialize
 *
 * Builds: filesrc -> decodebin -> RGBA converter chain -> queue, with the
 * queue exposed to the main pipeline. Decoders that can only emit
 * device-memory frames are rejected during autoplugging
 * (vcatDecodebinAutoplugSelect), and the shared RGBA tail pins
 * `video/x-raw,format=RGBA`, so the host always receives system-memory RGBA
 * frames no matter which decoder is installed.
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

    // Reject decoders that can only emit device-memory frames; the RGBA tail
    // needs system memory, so decodebin falls back to its next candidate.
    g_signal_connect(decoder, "autoplug-select", G_CALLBACK(vcatDecodebinAutoplugSelect), NULL);

    GstElement *sync = gst_element_factory_make("clocksync", NULL);
    VERIFY_ELEMENT(sync, failMessage);

    GstElement *queue = gst_element_factory_make("queue", NULL);
    VERIFY_ELEMENT(queue, failMessage);

    gSourceElement = queue;

    if (vcatRgbaChainCreate(&gRgbaChain, GST_BIN(gSourceBin)) != RC_OK)
    {
        lfLogError("%s: no RGBA converter available", failMessage);
        return RC_ERROR;
    }

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

    // Link the tail into the queue explicitly: gst_element_link_many rejected
    // this pair, likely confusing pad filtering around the capsfilter.
    GstPad *tailSrc = gst_element_get_static_pad(gRgbaChain.tail, "src");
    GstPad *syncSink = gst_element_get_static_pad(sync, "sink");
    GstPadLinkReturn tailLink = gst_pad_link(tailSrc, syncSink);
    gst_object_unref(tailSrc);
    gst_object_unref(syncSink);

    if (tailLink != GST_PAD_LINK_OK || !gst_element_link(sync, queue))
    {
        lfLogError("%s: failed to link RGBA chain -> clocksync -> queue (%s)", failMessage,
                   gst_pad_link_get_name(tailLink));
        return RC_ERROR;
    }

    g_signal_connect(decoder, "pad-added", G_CALLBACK(decodeBinPadAdded), gRgbaChain.head);

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

