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
#include <gst/gstelementfactory.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief how the camera is connected, which picks the GStreamer source element.
 *
 * The CLI talks about connection types (usb/csi), never about GStreamer
 * elements or V4L2/libcamera, to stay approachable for GStreamer beginners.
 */
typedef enum
{
    CAMERA_TYPE_UNKNOWN = 0,
    CAMERA_TYPE_USB = 1,
    CAMERA_TYPE_CSI = 2,
} CameraType;

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;
static VcatRgbaChain gRgbaChain = {0};
static char gDevicePath[PATH_MAX] = "/dev/video0";
// static char gCameraName[128] = "";    // CSI camera name, empty = libcamera's default
static int gCameraTypeRequested = -1; // -1 = infer the type from the device argument
static CameraType gCameraType = CAMERA_TYPE_USB;
static int gResolutionWidth = 0;  // 0 = not requested
static int gResolutionHeight = 0; // 0 = not requested
static int gFramerate = 0;        // 0 = not requested

VCAT_DEFINE_PLUGIN_NAME("Camera Source")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)
VCAT_DEFINE_PLUGIN_VERSION(VCAT_VERSION)
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("camera")
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION("Capture a USB or CSI camera (webcam / Raspberry Pi camera module)")

static const char *gUsbCameraSourceCandidates[] = {
    "v4l2src",     // Standard Linux V4L2 (x86/ARM webcams, Jetson, etc.)
    "mfvideosrc",  // Modern Windows (Media Foundation)
    "ksvideosrc",  // Legacy Windows (Kernel Streaming)
    "avfvideosrc", // macOS / iOS (AVFoundation)
};
static const int gUsbCameraSourceCandidatesLen = sizeof(gUsbCameraSourceCandidates) / //
                                                 sizeof(gUsbCameraSourceCandidates[0]);

static const char *gCsiCameraSourceCandidates[] = {
    /* 1. Vendor-specific ISP / Multimedia Framework sources (Highest Priority) */
    "nvarguscamerasrc", // NVIDIA Jetson (Orin, Xavier, Nano) - Hardware ISP via libargus
    "nvv4l2camerasrc",  // NVIDIA Jetson - V4L2 with zero-copy NVMM memory (YUV/Smart sensors)
    "qtiqmmfsrc",       // Qualcomm (RB5, QCS6490, Snapdragon) - Qualcomm Multimedia Framework
    "mediasrcbin",      // AMD / Xilinx (Kria KV260, ZynqMP) - Auto-configures media-controller ISP
    "rkcamsrc",         // Rockchip (older BSPs) - Rockchip ISP wrapper (newer BSPs use v4l2src)
    "imxv4l2videosrc",  // NXP i.MX (older BSPs) - Legacy i.MX V4L2 source (newer BSPs use v4l2src)

    /* 2. Modern Linux Camera Frameworks */
    "libcamerasrc", // Raspberry Pi (Pi 4/5, CM4 on Bullseye/Bookworm) & modern Linux V4L2/ISP

    /* 3. Third-Party Machine Vision CSI Wrappers (Optional - remove if not targeting industrial cams) */
    "tcambin",  // The Imaging Source - Cross-platform MIPI CSI / USB industrial cameras
    "pylonsrc", // Basler - Basler dart/pulse BCON for MIPI cameras

    /* 4. Legacy Platform Sources */
    "rpicamsrc", // Raspberry Pi (Legacy Buster OS / Broadcom MMAL stack)

    /* 5. Standard V4L2 / Generic Fallbacks (Lowest Priority) */
    "v4l2src",     // Generic Linux (NXP i.MX8/9, Rockchip RK3588, TI AM62A, MediaTek, raw V4L2)
    "pipewiresrc", // Modern Linux desktop/embedded (if camera is routed through PipeWire)
};
static const int gCsiCameraSourceCandidatesLen = sizeof(gCsiCameraSourceCandidates) / //
                                                 sizeof(gCsiCameraSourceCandidates[0]);

// The provider knows one device per V4L2 node, including metadata-only nodes of
// the same physical camera. Align the names column so both list nicely.
static void printDevice(const char *path, const char *displayName)
{
    printf("  %-*s   %s\n", 18, path, displayName);
}

/**
 * @brief Check whether a device matches a camera name, comparing its display
 *        name and every string property with the name.
 *
 * libcamera cameras can be named by their id or their display name; matching
 * both keeps this independent of the provider's exact property layout.
 *
 * @return true when @p name matches the device
 */
static bool deviceMatchesName(GstDevice *device, const char *name)
{
    gchar *displayName = gst_device_get_display_name(device);
    bool matched = displayName != NULL && strcmp(displayName, name) == 0;
    g_free(displayName);
    if (matched)
    {
        return true;
    }

    GstStructure *props = gst_device_get_properties(device);
    if (props != NULL)
    {
        for (guint i = 0; i < gst_structure_n_fields(props); i++)
        {
            const gchar *fieldName = gst_structure_nth_field_name(props, i);
            const GValue *value = gst_structure_get_value(props, fieldName);
            if (G_VALUE_TYPE(value) == G_TYPE_STRING && strcmp(g_value_get_string(value), name) == 0)
            {
                matched = true;
                break;
            }
        }
        gst_structure_free(props);
    }

    return matched;
}

/**
 * @brief Print every device found by a GStreamer device provider.
 *
 * @param[in] providerFactoryName e.g. "v4l2deviceprovider"
 * @param[in] sectionHeader printed once, only when devices are found
 * @param[in] pathKey property naming the CLI-selectable device node, NULL when
 *                    the display name itself is the CLI argument (libcamera)
 *
 * @return the number of devices printed; 0 when the provider is unavailable
 *         (e.g. the libcamera plugin is not installed) or finds nothing
 */
static int listProviderDevices(const char *providerFactoryName, const char *sectionHeader, const char *pathKey)
{
    GstDeviceProvider *provider = gst_device_provider_factory_get_by_name(providerFactoryName);
    if (provider == NULL)
    {
        return 0;
    }

    if (!gst_device_provider_start(provider))
    {
        lfLogWarn("Unable to start device provider '%s'", providerFactoryName);
        gst_object_unref(provider);
        return 0;
    }

    GList *devices = gst_device_provider_get_devices(provider);
    int count = g_list_length(devices);

    if (count > 0)
    {
        printf("%s", sectionHeader);
    }

    for (GList *item = devices; item != NULL; item = item->next)
    {
        GstDevice *device = GST_DEVICE(item->data);

        gchar *displayName = gst_device_get_display_name(device);

        const gchar *path = NULL;
        if (pathKey != NULL)
        {
            GstStructure *props = gst_device_get_properties(device);
            path = props == NULL ? NULL : gst_structure_get_string(props, pathKey);
            if (props != NULL)
            {
                gst_structure_free(props);
            }
        }

        if (path != NULL)
        {
            printDevice(path, displayName);
        }
        else
        {
            // libcamera: the camera name is the CLI argument, not a device node
            printf("  %s\n", displayName);
        }

        g_free(displayName);
    }
    g_list_free_full(devices, gst_object_unref);

    gst_device_provider_stop(provider);
    gst_object_unref(provider);

    return count;
}

/**
 * @brief Print every camera found by GStreamer's V4L2 and libcamera device
 *        providers.
 *
 * @return true if the providers could be queried
 */
static bool listDevices()
{
    // The host initializes GStreamer later, with the video pipeline; --list may
    // run before that. gst_init() is idempotent.
    gst_init(NULL, NULL);

    int usbCount = listProviderDevices("v4l2deviceprovider", //
                                       "Detected USB cameras (V4L2 device nodes):\n", "device.path");
    int csiCount = listProviderDevices("libcameraprovider", //
                                       "Detected CSI cameras (libcamera names):\n", NULL);

    if (usbCount == 0 && csiCount == 0)
    {
        printf("No cameras detected.\n");
    }

    return true;
}

static void videoSourcePrintHelp()
{
    printf("camera - Capture a USB or CSI camera (webcam / Raspberry Pi camera module)\n"
           "\n"
           "Usage: vcat camera [device] [OPTIONS]\n"
           "\n"
           "The device depends on the camera type:\n"
           "  USB cameras take a V4L2 device node, defaulting to /dev/video0.\n"
           "  CSI cameras take a camera name, e.g. 'unicam'.\n"
           "\n"
           "Run 'vcat camera --list' to see the detected cameras.\n"
           "\n"
           "Options:\n"
           "  -h, --help            Show this help\n"
           "  -v, --version         Show version information\n"
           "  -l, --list            List detected cameras and exit\n"
           "  -d, --device <dev>    Camera to use: a /dev/videoN node (USB) or a\n"
           "                        camera name (CSI)\n"
           "  -t, --type <kind>     Camera connection type: 'usb' or 'csi'. Inferred\n"
           "                        from the device when omitted\n"
           "  -r, --resolution WxH  Requested capture resolution, e.g. 1280x720\n"
           "  -f, --framerate fps   Requested capture framerate, e.g. 30\n"
           "\n"
           "Examples:\n"
           "  vcat camera                      Use /dev/video0\n"
           "  vcat camera /dev/video2          Use a specific USB camera\n"
           "  vcat camera unicam               Use the CSI camera named 'unicam'\n"
           "  vcat camera --type csi           Use libcamera's default camera\n");
}

/**
 * @brief Check that the requested CSI (libcamera) camera can actually be used,
 *        listing the available camera names when it cannot.
 *
 * @return true when the camera can be used
 */
// static bool validateCsiCamera()
// {
//     // The host initializes GStreamer later, with the video pipeline; validation
//     // may run before that. gst_init() is idempotent.
//     gst_init(NULL, NULL);
//
//     if (!gst_element_factory_find("libcamerasrc"))
//     {
//         fprintf(stderr, "CSI camera support requires the GStreamer libcamera plugin,\n"
//                         "e.g. 'sudo apt install gstreamer1.0-libcamera' on Raspberry Pi OS.\n"
//                         "Run 'vcat camera --list' to see the USB cameras.\n");
//         return false;
//     }
//
//     GstDeviceProvider *provider = gst_device_provider_factory_get_by_name("libcameraprovider");
//     if (provider == NULL)
//     {
//         fprintf(stderr, "Unable to enumerate CSI (libcamera) cameras\n");
//         return false;
//     }
//
//     if (!gst_device_provider_start(provider))
//     {
//         fprintf(stderr, "Unable to enumerate CSI (libcamera) cameras\n");
//         gst_object_unref(provider);
//         return false;
//     }
//
//     GList *devices = gst_device_provider_get_devices(provider);
//     bool usable = true;
//
//     if (gCameraName[0] != '\0')
//     {
//         bool found = false;
//         for (GList *item = devices; item != NULL; item = item->next)
//         {
//             if (deviceMatchesName(GST_DEVICE(item->data), gCameraName))
//             {
//                 found = true;
//                 break;
//             }
//         }
//
//         if (!found)
//         {
//             fprintf(stderr, "CSI camera '%s' not found. Available cameras:\n", gCameraName);
//             for (GList *item = devices; item != NULL; item = item->next)
//             {
//                 gchar *displayName = gst_device_get_display_name(GST_DEVICE(item->data));
//                 fprintf(stderr, "  %s\n", displayName != NULL ? displayName : "unknown camera");
//                 g_free(displayName);
//             }
//             usable = false;
//         }
//     }
//     else if (devices == NULL)
//     {
//         fprintf(stderr, "No CSI cameras detected. Check the ribbon cable and camera\n"
//                         "detection in the boot configuration, or run 'vcat camera --list'.\n");
//         usable = false;
//     }
//
//     g_list_free_full(devices, gst_object_unref);
//
//     gst_device_provider_stop(provider);
//     gst_object_unref(provider);
//
//     return usable;
// }

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
        {"type",       required_argument, NULL, 't'},
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
    while ((opt = getopt_long(remaining + 1, args, "+d:r:f:t:hvl", cliOptions, NULL)) != -1)
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
        case 't':
            if (strcmp(optarg, "usb") == 0)
            {
                gCameraTypeRequested = CAMERA_TYPE_USB;
            }
            else if (strcmp(optarg, "csi") == 0)
            {
                gCameraTypeRequested = CAMERA_TYPE_CSI;
            }
            else
            {
                fprintf(stderr, "Invalid camera type '%s', expected 'usb' or 'csi'.\n", optarg);
                return ARG_PARSE_RESULT_ERROR;
            }
            break;
        case 'r':
            if (sscanf(optarg, "%dx%d", &gResolutionWidth, &gResolutionHeight) != 2 || //
                gResolutionWidth <= 0 ||                                               //
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

    // Classify the camera argument: a "/dev/..." string is a V4L2 device node
    // (USB); anything else is a libcamera camera name (CSI). Without an
    // argument, an explicit --type wins and otherwise USB stays the default.
    if (device != NULL)
    {
        if (strncmp(device, "/dev/", 5) == 0)
        {
            snprintf(gDevicePath, sizeof(gDevicePath), "%s", device);
        }
    }

    if (gCameraTypeRequested >= 1)
    {
        gCameraType = (CameraType)gCameraTypeRequested;
    }

    if (gCameraType == CAMERA_TYPE_USB)
    {
        lfLogInfo("USB camera device: '%s'", gDevicePath);

        if (!fileExists(gDevicePath))
        {
            fprintf(stderr, "Camera device does not exist: '%s'. Run 'vcat camera --list'.\n", gDevicePath);
            return ARG_PARSE_RESULT_ERROR;
        }
    }
    else
    {
        // lfLogInfo("CSI camera: '%s'", gCameraName[0] != '\0' ? gCameraName : "(default)");

        // if (!validateCsiCamera())
        // {
        //     return ARG_PARSE_RESULT_ERROR;
        // }
    }

    return ARG_PARSE_RESULT_SUCCESS;
}

static void decodeBinPadAdded(GstElement *_, GstPad *newPad, gpointer userData)
{
    GstElement *rgbaChainHead = GST_ELEMENT(userData);

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

    GstPad *sinkPad = gst_element_get_static_pad(rgbaChainHead, "sink");

    if (gst_pad_is_linked(sinkPad))
    {
        lfLogWarn("Unable to link new pad: already linked");
    }
    else if (gst_pad_link(newPad, sinkPad) != GST_PAD_LINK_OK)
    {
        lfLogError("Failed to link decodebin -> %s", GST_ELEMENT_NAME(rgbaChainHead));
    }
    else
    {
        lfLogInfo("Linked decodebin -> %s", GST_ELEMENT_NAME(rgbaChainHead));
    }

    gst_object_unref(sinkPad);

EXIT:

    gst_caps_unref(caps);
}

static void setCameraDeviceProperty(GstElement *source, const char *cameraId)
{
    if (source == NULL || cameraId == NULL || cameraId[0] == '\0')
    {
        return;
    }

    GObjectClass *klass = G_OBJECT_GET_CLASS(source);

    if (g_object_class_find_property(klass, "camera-name") != NULL)
    {
        // libcamerasrc
        g_object_set(G_OBJECT(source), "camera-name", cameraId, NULL);
    }
    else if (g_object_class_find_property(klass, "device") != NULL)
    {
        // v4l2src, nvv4l2camerasrc, imxv4l2videosrc
        g_object_set(G_OBJECT(source), "device", cameraId, NULL);
    }
    else if (g_object_class_find_property(klass, "sensor-id") != NULL)
    {
        // nvarguscamerasrc (expects integer index like "0", "1")
        g_object_set(G_OBJECT(source), "sensor-id", atoi(cameraId), NULL);
    }
    else if (g_object_class_find_property(klass, "camera") != NULL)
    {
        // qtiqmmfsrc (expects unsigned integer index like "0", "1")
        g_object_set(G_OBJECT(source), "camera", (guint)atoi(cameraId), NULL);
    }
    else if (g_object_class_find_property(klass, "camera-number") != NULL)
    {
        // rpicamsrc (expects integer index)
        g_object_set(G_OBJECT(source), "camera-number", atoi(cameraId), NULL);
    }
    else if (g_object_class_find_property(klass, "serial") != NULL)
    {
        // tcambin, pylonsrc
        g_object_set(G_OBJECT(source), "serial", cameraId, NULL);
    }
}

static void setUsbCameraDeviceProperty(GstElement *source, const char *deviceId)
{
    if (source == NULL || deviceId == NULL || deviceId[0] == '\0')
    {
        return;
    }

    GObjectClass *klass = G_OBJECT_GET_CLASS(source);

    if (g_object_class_find_property(klass, "device") != NULL)
    {
        // v4l2src (Linux: e.g., "/dev/video0")
        g_object_set(G_OBJECT(source), "device", deviceId, NULL);
    }
    else if (g_object_class_find_property(klass, "device-path") != NULL)
    {
        // mfvideosrc & ksvideosrc (Windows: DirectShow/MF symbolic link path)
        g_object_set(G_OBJECT(source), "device-path", deviceId, NULL);
    }
    else if (g_object_class_find_property(klass, "device-index") != NULL)
    {
        // avfvideosrc (macOS/iOS: expects integer index like "0", "1")
        g_object_set(G_OBJECT(source), "device-index", atoi(deviceId), NULL);
    }
}

/**
 * @brief Video Source Initialize
 *
 * Builds: source -> [capsfilter] -> decodebin -> RGBA converter chain ->
 * queue, with the queue exposed to the main pipeline. The source element
 * depends on the camera type: v4l2src for USB device nodes, libcamerasrc for
 * CSI cameras (the Pi's ISP processing is only reachable through libcamera).
 * decodebin hands raw cameras through as-is and decodes Motion-JPEG-only
 * cameras to raw video, and the RGBA chain guarantees the host receives
 * system-memory RGBA frames.
 *
 * @return RC_OK if successful
 */
rc_t videoSourceInitialize()
{
    char *failMessage = "Unable to initialize camera video source";

    gSourceBin = gst_bin_new(VCAT_VIDEO_SOURCE_BIN_NAME);
    VERIFY_ELEMENT(gSourceBin, failMessage);

    GstElement *source = NULL;
    if (gCameraType == CAMERA_TYPE_CSI)
    {
        source = vcatInitElementFromCandidatesList(gCsiCameraSourceCandidates, //
                                                   gCsiCameraSourceCandidatesLen);
        VERIFY_ELEMENT(source, failMessage);

        setCameraDeviceProperty(source, gDevicePath);
    }
    else
    {
        source = vcatInitElementFromCandidatesList(gUsbCameraSourceCandidates, //
                                                   gUsbCameraSourceCandidatesLen);
        VERIFY_ELEMENT(source, failMessage);

        setUsbCameraDeviceProperty(source, gDevicePath);
    }

    GstElement *decoder = gst_element_factory_make("decodebin", NULL);
    VERIFY_ELEMENT(decoder, failMessage);

    g_signal_connect(decoder, "autoplug-select", G_CALLBACK(vcatDecodebinSkipDeviceMemoryDecoders), NULL);

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
            lfLogError("%s: failed to link source -> capsfilter", failMessage);
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
        lfLogError("%s: failed to link source -> decodebin", failMessage);
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
rc_t videoSourceCleanup()
{
    return RC_OK;
}

/**
 * @brief Get the GStreamer source element (the stream link point)
 *
 * @return the element, or NULL when not initialized
 */
GstElement *videoSourceGetSourceElement()
{
    return gSourceElement;
}

/**
 * @brief Get the complete GStreamer source bin
 *
 * @return the bin, or NULL when not initialized
 */
GstElement *videoSourceGetSourceBin()
{
    return gSourceBin;
}
