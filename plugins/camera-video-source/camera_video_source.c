#include "crox_types.h"
#include "file_utils.h"
#include "gst/gst.h"
#include "gst/gstbin.h"
#include "gst/gstcaps.h"
#include "gst/gstdeviceprovider.h"
#include "gst/gstdeviceproviderfactory.h"
#include "gst/gstelement.h"
#include "gst/gststructure.h"
#include "gst/gstutils.h"
#include "gst/gstvalue.h"
#include "logging_facade.h"
#include "vcat_plugin_framework.h"
#include "vcat_video_source_utils.h"
#include <getopt.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;
static VcatRgbaChain gRgbaChain = {0};
static char gDevicePath[PATH_MAX] = "/dev/video0";
static int gResolutionWidth = 0;  // 0 = not requested
static int gResolutionHeight = 0; // 0 = not requested
static int gFramerate = 0;        // 0 = not requested

VCAT_DEFINE_PLUGIN_NAME("Camera Source")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)
VCAT_DEFINE_PLUGIN_VERSION(VCAT_VERSION)
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("camera")
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION("Capture a USB camera (webcam)")

// The provider knows one device per V4L2 node, including metadata-only nodes of
// the same physical camera. Align the names column so both list nicely.
static void printDevice(const char *path, const char *displayName)
{
    printf("  %-*s   %s\n", 18, path, displayName);
}

/**
 * @brief Print every device found by GStreamer's V4L2 device provider.
 *
 * @return true if the devices were enumerated
 */
static bool listDevices()
{
    // The host initializes GStreamer later, with the video pipeline; --list may
    // run before that. gst_init() is idempotent.
    gst_init(NULL, NULL);

    GstDeviceProvider *provider = gst_device_provider_factory_get_by_name("v4l2deviceprovider");
    if (provider == NULL)
    {
        fprintf(stderr, "Unable to create the V4L2 device provider\n");
        return false;
    }

    if (!gst_device_provider_start(provider))
    {
        fprintf(stderr, "Unable to start the V4L2 device provider\n");
        gst_object_unref(provider);
        return false;
    }

    printf("Detected cameras:\n");

    GList *devices = gst_device_provider_get_devices(provider);
    for (GList *item = devices; item != NULL; item = item->next)
    {
        GstDevice *device = GST_DEVICE(item->data);

        // The properties include "device.path", e.g. "/dev/video0".
        GstStructure *props = gst_device_get_properties(device);
        const gchar *path = props == NULL ? NULL : gst_structure_get_string(props, "device.path");

        gchar *displayName = gst_device_get_display_name(device);
        printDevice(path != NULL ? path : "unknown device", displayName);
        g_free(displayName);

        if (props != NULL)
        {
            gst_structure_free(props);
        }
    }
    g_list_free_full(devices, gst_object_unref);

    gst_device_provider_stop(provider);
    gst_object_unref(provider);

    return true;
}

static void videoSourcePrintHelp()
{
    printf("camera - Capture a USB camera (webcam)\n"
           "\n"
           "Usage: vcat camera [device] [OPTIONS]\n"
           "\n"
           "The device is an optional V4L2 device node, defaulting to\n"
           "/dev/video0. Run 'vcat camera --list' to see the detected cameras.\n"
           "\n"
           "Options:\n"
           "  -h, --help            Show this help\n"
           "  -v, --version         Show version information\n"
           "  -l, --list            List detected camera devices and exit\n"
           "  -d, --device <path>   Camera device node (default /dev/video0)\n"
           "  -r, --resolution WxH  Requested capture resolution, e.g. 1280x720\n"
           "  -f, --framerate fps   Requested capture framerate, e.g. 30\n");
}

/**
 * @brief process the source's command line, e.g. "vcat camera /dev/video0"
 *
 * @param[in] argc count, including the source URI in argv[0]
 * @param[in] argv the source URI token and its options
 *
 * @return InputArgsResult telling the host whether to continue, exit early, or fail
 */
InputArgsResult videoSourceProcessInputArgs(int argc, char *argv[])
{
    static const struct option cliOptions[] = {
        {"version",    no_argument,       NULL, 'v'},
        {"help",       no_argument,       NULL, 'h'},
        {"list",       no_argument,       NULL, 'l'},
        {"device",     required_argument, NULL, 'd'},
        {"resolution", required_argument, NULL, 'r'},
        {"framerate",  required_argument, NULL, 'f'},
        {NULL,         0,                 NULL, 0  }  //
    };

    int optStart = 1;
    const char *device = NULL;

    // The selected token must be the bare scheme: local sources take their
    // resource as a positional argument, not a URI. The host still routes
    // "://" forms here on the shared scheme, so reject those explicitly.
    if (strcmp(argv[0], "camera") != 0)
    {
        fprintf(stderr, "Invalid source '%s'. Use 'vcat camera <device>', not a URI.\n", argv[0]);
        return ARG_PARSE_RESULT_ERROR;
    }

    // An optional positional device argument, e.g. "vcat camera /dev/video2"
    if (argc > 1 && argv[1][0] != '-')
    {
        device = argv[1];
        optStart = 2;
    }

    // getopt's first array element is skipped, so keep a placeholder for it
    // rather than passing shifted pointers (which would hide the first token).
    // optind is also global: the host already ran getopt before handing the
    // source token over, so reset it for a clean scan.
    int remaining = argc - optStart;
    char *args[remaining + 1];
    args[0] = argv[0];
    for (int i = 0; i < remaining; i++)
    {
        args[i + 1] = argv[optStart + i];
    }

    optind = 0;

    int opt;
    while ((opt = getopt_long(remaining + 1, args, "+d:r:f:hvl", cliOptions, NULL)) != -1)
    {
        switch (opt)
        {
        case 'h':
            videoSourcePrintHelp();
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case 'v':
            printf("camera v%s\n", VCAT_VERSION);
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case 'l':
            return listDevices() ? ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY : ARG_PARSE_RESULT_ERROR;
        case 'd':
            device = optarg;
            break;
        case 'r':
            if (sscanf(optarg, "%dx%d", &gResolutionWidth, &gResolutionHeight) != 2 || gResolutionWidth <= 0 ||
                gResolutionHeight <= 0)
            {
                fprintf(stderr, "Invalid resolution '%s', expected WxH, e.g. 1280x720.\n", optarg);
                return ARG_PARSE_RESULT_ERROR;
            }
            break;
        case 'f':
            gFramerate = atoi(optarg);
            if (gFramerate <= 0)
            {
                fprintf(stderr, "Invalid framerate '%s', expected a positive number.\n", optarg);
                return ARG_PARSE_RESULT_ERROR;
            }
            break;
        default:
            fprintf(stderr, "Unknown option. See 'vcat camera --help'.\n");
            return ARG_PARSE_RESULT_ERROR;
        }
    }

    if (optind < remaining + 1)
    {
        fprintf(stderr, "Unknown argument '%s'. See 'vcat camera --help'.\n", args[optind]);
        return ARG_PARSE_RESULT_ERROR;
    }

    if (device != NULL)
    {
        snprintf(gDevicePath, sizeof(gDevicePath), "%s", device);
    }

    lfLogInfo("Camera device: '%s'", gDevicePath);

    if (!fileExists(gDevicePath))
    {
        fprintf(stderr, "Camera device does not exist: '%s'. Run 'vcat camera --list'.\n", gDevicePath);
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
 * Builds: v4l2src -> [capsfilter] -> decodebin -> RGBA converter chain ->
 * queue, with the queue exposed to the main pipeline. decodebin hands raw
 * cameras through as-is and decodes Motion-JPEG-only cameras to raw video;
 * decoders that can only emit device-memory frames are rejected during
 * autoplugging (vcatDecodebinAutoplugSelect) and the shared RGBA tail pins
 * `video/x-raw,format=RGBA`, so the host always receives system-memory RGBA
 * frames.
 *
 * @return RC_OK if successful
 */
Rc videoSourceInitialize()
{
    char *failMessage = "Unable to initialize camera video source";

    gSourceBin = gst_bin_new("video_src_bin");
    VERIFY_ELEMENT(gSourceBin, failMessage);

    GstElement *source = gst_element_factory_make("v4l2src", NULL);
    VERIFY_ELEMENT(source, failMessage);

    g_object_set(G_OBJECT(source), //
                 "device", gDevicePath, NULL);

    GstElement *decoder = gst_element_factory_make("decodebin", NULL);
    VERIFY_ELEMENT(decoder, failMessage);

    // Reject decoders that can only emit device-memory frames; the RGBA tail
    // needs system memory, so decodebin falls back to its next candidate.
    g_signal_connect(decoder, "autoplug-select", G_CALLBACK(vcatDecodebinAutoplugSelect), NULL);

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
                     queue,               //
                     NULL);

    // Optional capture constraint, e.g. from -r/--resolution and -f/--framerate.
    // Unconstrained caps let the camera pick its best supported mode.
    GstElement *capsfilter = NULL;
    int requestedWidthHeight = gResolutionWidth > 0;
    int requestedFramerate = gFramerate > 0;
    if (requestedWidthHeight || requestedFramerate)
    {
        capsfilter = gst_element_factory_make("capsfilter", NULL);
        VERIFY_ELEMENT(capsfilter, failMessage);

        GstCaps *caps = gst_caps_new_empty();
        GstStructure *structure = gst_structure_new_empty("video/x-raw");

        if (requestedWidthHeight)
        {
            gst_structure_set(structure,                               //
                              "width", G_TYPE_INT, gResolutionWidth,   //
                              "height", G_TYPE_INT, gResolutionHeight, //
                              NULL);
        }

        if (requestedFramerate)
        {
            gst_structure_set(structure,                                     //
                              "framerate", GST_TYPE_FRACTION, gFramerate, 1, //
                              NULL);
        }

        gst_caps_append_structure(caps, structure);

        g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
        gst_caps_unref(caps);

        gst_bin_add(GST_BIN(gSourceBin), capsfilter);

        if (!gst_element_link(source, capsfilter))
        {
            lfLogError("%s: failed to link v4l2src -> capsfilter", failMessage);
            return RC_ERROR;
        }
    }

    if (capsfilter != NULL)
    {
        if (!gst_element_link(capsfilter, decoder))
        {
            lfLogError("%s: failed to link capsfilter -> decodebin", failMessage);
            return RC_ERROR;
        }
    }
    else if (!gst_element_link(source, decoder))
    {
        lfLogError("%s: failed to link v4l2src -> decodebin", failMessage);
        return RC_ERROR;
    }

    if (!gst_element_link(gRgbaChain.tail, queue))
    {
        lfLogError("%s: failed to link RGBA chain -> queue", failMessage);
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
