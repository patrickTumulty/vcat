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

/**
 * @brief a way to capture a camera, described in platform terms.
 *
 * One row per source element, tried in priority order within the requested
 * camera type: vendor ISP sources first, generic fallbacks last. Rows with a
 * GStreamer device provider can enumerate their cameras for 'vcat camera
 * --list' and validate a user-selected device; the others (vendor sources
 * without a provider) can only report what kind of value --device expects.
 */
typedef struct CameraBackend
{
    CameraType type;
    const char *element;     // GStreamer source element (first available wins)
    const char *provider;    // device provider to enumerate with; NULL = cannot list
    const char *providerKey; // device property carrying the CLI token; NULL = display name
    const char *platform;    // human-readable label for the --list hint lines
    const char *tokenHint;   // what a --device value means here, as an example
} CameraBackend;

// clang-format off
static const CameraBackend gBackends[] = {
    // USB webcams: standard Linux first, then the native platform sources.
    {CAMERA_TYPE_USB, "v4l2src",     "v4l2deviceprovider", "device.path", "Linux V4L2",                  "<node>, e.g. /dev/video0"},
    {CAMERA_TYPE_USB, "mfvideosrc",  NULL,                 NULL,          "Windows (Media Foundation)",  "<path or index>"},
    {CAMERA_TYPE_USB, "ksvideosrc",  NULL,                 NULL,          "Windows (Kernel Streaming)",  "<path or index>"},
    {CAMERA_TYPE_USB, "avfvideosrc", NULL,                 NULL,          "macOS / iOS (AVFoundation)",  "<camera index>, e.g. 0"},

    // CSI / embedded ISP cameras: vendor ISP frameworks first, generic
    // fallbacks last.
    {CAMERA_TYPE_CSI, "nvarguscamerasrc", NULL,                 NULL,          "NVIDIA Jetson (Argus)",                   "<sensor index>, e.g. 0"},
    {CAMERA_TYPE_CSI, "nvv4l2camerasrc",  "v4l2deviceprovider", "device.path", "NVIDIA Jetson (V4L2/NVMM)",               "<node>, e.g. /dev/video0"},
    {CAMERA_TYPE_CSI, "qtiqmmfsrc",       NULL,                 NULL,          "Qualcomm (RB5, QCS6490, ...)",            "<camera index>, e.g. 0"},
    {CAMERA_TYPE_CSI, "mediasrcbin",      NULL,                 NULL,          "AMD / Xilinx (Kria KV260, ZynqMP)",       "<media controller node>"},
    {CAMERA_TYPE_CSI, "rkcamsrc",         NULL,                 NULL,          "Rockchip (older BSPs)",                   "<node>"},
    {CAMERA_TYPE_CSI, "imxv4l2videosrc",  NULL,                 NULL,          "NXP i.MX (older BSPs)",                   "<node>"},
    {CAMERA_TYPE_CSI, "libcamerasrc",     "libcameraprovider",  NULL,          "Raspberry Pi / modern Linux (libcamera)", "<camera name>, e.g. unicam"},
    {CAMERA_TYPE_CSI, "tcambin",          NULL,                 NULL,          "The Imaging Source",                      "<serial>"},
    {CAMERA_TYPE_CSI, "pylonsrc",         NULL,                 NULL,          "Basler",                                  "<serial>"},
    {CAMERA_TYPE_CSI, "rpicamsrc",        NULL,                 NULL,          "Raspberry Pi (legacy MMAL)",              "<camera index>, e.g. 0"},
    {CAMERA_TYPE_CSI, "v4l2src",          "v4l2deviceprovider", "device.path", "Generic Linux raw V4L2",                  "<node>, e.g. /dev/video11"},
    {CAMERA_TYPE_CSI, "pipewiresrc",      NULL,                 NULL,          "Linux desktop (PipeWire)",                "<node id>"},
};
// clang-format on
static const int gBackendsLen = sizeof(gBackends) / sizeof(gBackends[0]);

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;
static VcatRgbaChain gRgbaChain = {0};
static char gDeviceToken[PATH_MAX] = "/dev/video0"; // default USB node; --device overwrites it
static bool gDeviceGiven = false;                   // true once the user passed --device or a positional value
static int gCameraTypeRequested = -1;               // -1 = not passed; then USB is the default
static CameraType gCameraType = CAMERA_TYPE_USB;
static bool gListRequested = false;
static int gResolutionWidth = 0;  // 0 = not requested
static int gResolutionHeight = 0; // 0 = not requested
static int gFramerate = 0;        // 0 = not requested

VCAT_DEFINE_PLUGIN_NAME("Camera Source")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)
VCAT_DEFINE_PLUGIN_VERSION(VCAT_VERSION)
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("camera")
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION("Capture a USB or CSI camera (webcam / Raspberry Pi camera module)")

// The provider knows one device per V4L2 node, including metadata-only nodes of
// the same physical camera. Align the names column so both list nicely.
static void printDevice(const char *token, const char *displayName)
{
    if (displayName != NULL && strcmp(displayName, token) != 0)
    {
        printf("  %-*s   %s\n", 18, token, displayName);
    }
    else
    {
        printf("  %s\n", token);
    }
}

/**
 * @brief Initialize GStreamer for CLI queries, keeping libcamera's startup
 *        chatter (camera enumeration logs) out of the output.
 *
 * The user's own LIBCAMERA_LOG_LEVELS setting wins when they export one.
 */
static void gstInitQuiet()
{
    // May run before the host initializes GStreamer; gst_init is idempotent.
    setenv("LIBCAMERA_LOG_LEVELS", "*:ERROR", 0);
    gst_init(NULL, NULL);
}

/**
 * @brief Derive the short camera model name from a libcamera camera id.
 *
 * e.g. "/base/soc/i2c0mux/i2c@1/ov5647@36" -> "ov5647"
 *
 * @return a newly allocated string, or NULL when nothing can be derived
 */
static gchar *cameraModelFromName(const char *name)
{
    if (name == NULL)
    {
        return NULL;
    }

    const char *base = strrchr(name, '/');
    base = base != NULL ? base + 1 : name;

    const char *at = strchr(base, '@');
    gsize len = at != NULL ? (gsize)(at - base) : strlen(base);
    if (len == 0)
    {
        return NULL;
    }

    return g_strndup(base, len);
}

/**
 * @brief Check whether a device matches a camera token, comparing its display
 *        name, its derived model name and every string property with the
 *        token.
 *
 * libcamera cameras can be named by their full id or their short model name,
 * V4L2 cameras by their device path; matching everything keeps this
 * independent of the provider's exact property layout.
 *
 * @return true when @p token matches the device
 */
static bool deviceMatchesToken(GstDevice *device, const char *token)
{
    gchar *displayName = gst_device_get_display_name(device);
    bool matched = displayName != NULL && strcmp(displayName, token) == 0;

    if (!matched)
    {
        gchar *model = cameraModelFromName(displayName);
        matched = model != NULL && strcmp(model, token) == 0;
        g_free(model);
    }

    if (!matched)
    {
        GstStructure *props = gst_device_get_properties(device);
        if (props != NULL)
        {
            for (guint i = 0; i < gst_structure_n_fields(props); i++)
            {
                const gchar *fieldName = gst_structure_nth_field_name(props, i);
                const GValue *value = gst_structure_get_value(props, fieldName);
                if (G_VALUE_TYPE(value) == G_TYPE_STRING && strcmp(g_value_get_string(value), token) == 0)
                {
                    matched = true;
                    break;
                }
            }
            gst_structure_free(props);
        }
    }

    g_free(displayName);

    return matched;
}

/**
 * @brief Find the first backend of the requested type whose GStreamer element
 *        is available on this machine.
 *
 * @param[in] type camera connection type to select
 *
 * @return the backend, or NULL when no element of that type is available
 */
static const CameraBackend *selectBackend(CameraType type)
{
    gstInitQuiet();

    for (int i = 0; i < gBackendsLen; i++)
    {
        if (gBackends[i].type != type)
        {
            continue;
        }
        if (gst_element_factory_find(gBackends[i].element))
        {
            return &gBackends[i];
        }
    }
    return NULL;
}

// V4L2 infrastructure drivers: their nodes are capture-capable on paper, but
// they only produce frames when a pipeline drives the media controller / ISP,
// which libcamerasrc does. A standalone v4l2src on them just stalls, so their
// devices are hidden from --list to keep it to usable cameras.
static const char *const gHiddenV4l2Drivers[] = {
    "bcm2835-isp", // Raspberry Pi ISP nodes
    "unicam",      // Raspberry Pi CSI node (raw Bayer; needs the Pi's ISP)
};
static const int gHiddenV4l2DriversLen = sizeof(gHiddenV4l2Drivers) / sizeof(gHiddenV4l2Drivers[0]);

/**
 * @brief Print every device found by a GStreamer device provider.
 *
 * @param[in] providerFactoryName e.g. "v4l2deviceprovider"
 * @param[in] sectionHeader printed once, only when devices are found
 * @param[in] tokenKey device property naming the CLI-selectable device; NULL
 *                     when the display name itself is the CLI argument
 *
 * @return the number of devices printed; 0 when the provider is unavailable
 *         (e.g. the libcamera plugin is not installed) or finds nothing
 */
static int listProviderDevices(const char *providerFactoryName, const char *sectionHeader, const char *tokenKey)
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

        // Some V4L2 drivers expose infrastructure nodes (Pi ISP output nodes,
        // CSI metadata nodes): capture-capable on paper, but they only produce
        // frames when a pipeline drives the media controller, which is what
        // libcamerasrc does. A standalone v4l2src on them just stalls, so hide
        // them to keep the listing to usable cameras.
        GstStructure *props = gst_device_get_properties(device);
        const gchar *driver = props == NULL ? NULL : gst_structure_get_string(props, "v4l2.device.driver");
        bool hidden = false;
        for (int i = 0; i < gHiddenV4l2DriversLen && !hidden; i++)
        {
            hidden = driver != NULL && strcmp(driver, gHiddenV4l2Drivers[i]) == 0;
        }

        // The token points into the props structure: copy it before the
        // structure is freed.
        gchar *token = NULL;
        if (!hidden)
        {
            if (tokenKey != NULL)
            {
                const gchar *path = props == NULL ? NULL : gst_structure_get_string(props, tokenKey);
                token = path != NULL ? g_strdup(path) : NULL;
            }
            else
            {
                // e.g. libcamera: the full camera id is unwieldy, so show the
                // short model name as the value to pass.
                token = cameraModelFromName(displayName);
                if (token == NULL)
                {
                    token = g_strdup(displayName);
                }
            }
        }

        if (props != NULL)
        {
            gst_structure_free(props);
        }

        if (hidden)
        {
            g_free(displayName);
            continue;
        }

        if (token != NULL)
        {
            printDevice(token, displayName);
            g_free(token);
        }
        else
        {
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
 * @brief Print the listing section for one camera type.
 *
 * Only the backend that 'vcat camera --type <kind>' would actually select gets
 * its devices enumerated; alternates of the same type appear as hint lines
 * only. That keeps vendor infrastructure nodes (e.g. the Pi's ISP and CSI
 * V4L2 nodes) out of the sections they do not belong to.
 *
 * @param[in] type camera type whose section is printed
 * @param[in] selected backend that '--type <kind>' would select, NULL when
 *                    none is available
 * @param[in] providerToSkip provider whose devices were already listed under
 *                          another section; its backend then shows as a hint
 * @param[out] printed set to true when anything was printed (devices or hints)
 */
static void listBackendSection(CameraType type,
                               const CameraBackend *selected,
                               const char *providerToSkip,
                               bool *printed)
{
    const char *typeFlag = type == CAMERA_TYPE_USB ? "usb" : "csi";
    const char *typeLabel = type == CAMERA_TYPE_USB ? "USB" : "CSI";
    const char *sectionHeader = type == CAMERA_TYPE_USB //
                                    ?
                                    "\nUSB cameras (pass with --type usb --device <node>):\n" :
                                    "\nCSI cameras (pass with --type csi --device <name>):\n";

    bool selectedEnumerated = selected != NULL && selected->provider != NULL && //
                              (providerToSkip == NULL || strcmp(selected->provider, providerToSkip) != 0);

    if (selectedEnumerated)
    {
        if (listProviderDevices(selected->provider, sectionHeader, selected->providerKey) > 0)
        {
            *printed = true;
        }
    }

    // Hint lines for the installed backends that cannot be enumerated (this
    // includes the selected backend when it has no usable provider).
    bool hintHeaderPrinted = false;
    for (int i = 0; i < gBackendsLen; i++)
    {
        const CameraBackend *backend = &gBackends[i];
        if (backend->type != type || !gst_element_factory_find(backend->element))
        {
            continue;
        }

        // Skip the selected backend when its devices were enumerated, and
        // provider-capable alternates: the former was already listed, the
        // latter would be noise (e.g. the raw-V4L2 fallback next to the
        // generic USB section).
        bool isEnumerated = backend == selected && selectedEnumerated;
        if (isEnumerated || (backend->provider != NULL && backend != selected))
        {
            continue;
        }

        if (!hintHeaderPrinted)
        {
            printf("\n%s cameras that cannot be listed automatically (pass with --type %s):\n", typeLabel, typeFlag);
            hintHeaderPrinted = true;
            *printed = true;
        }
        printf("  %-36s --device %s\n", backend->platform, backend->tokenHint);
    }
}

/**
 * @brief Print every camera the installed backends can see, grouped by type.
 *
 * @param[in] filter camera type to restrict the listing to, or
 *                   CAMERA_TYPE_UNKNOWN for all types
 *
 * @return true, so '--list' always exits successfully
 */
static bool listDevices(CameraType filter)
{
    gstInitQuiet();

    // The generic raw-V4L2 fallback is shared with the USB section: when it is
    // the selected CSI backend, its nodes were already listed as USB cameras,
    // so it shows as a hint instead of a repeated (misleading) section.
    const char *usbProviderUsed = NULL;
    bool printed = false;

    if (filter == CAMERA_TYPE_UNKNOWN || filter == CAMERA_TYPE_USB)
    {
        const CameraBackend *selected = selectBackend(CAMERA_TYPE_USB);
        listBackendSection(CAMERA_TYPE_USB, selected, NULL, &printed);
        if (selected != NULL && selected->provider != NULL)
        {
            usbProviderUsed = selected->provider;
        }
    }
    if (filter == CAMERA_TYPE_UNKNOWN || filter == CAMERA_TYPE_CSI)
    {
        const CameraBackend *selected = selectBackend(CAMERA_TYPE_CSI);
        listBackendSection(CAMERA_TYPE_CSI, selected, usbProviderUsed, &printed);
    }

    if (!printed)
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
           "  CSI cameras take a camera name or index, depending on the platform;\n"
           "  'vcat camera --list' shows the valid values.\n"
           "\n"
           "Options:\n"
           "  -h, --help            Show this help\n"
           "  -v, --version         Show version information\n"
           "  -l, --list            List detected cameras and exit; combine with\n"
           "                        --type usb or --type csi to filter the output\n"
           "  -d, --device <dev>    Camera to use (see --list for the valid values)\n"
           "  -t, --type <kind>     Camera connection type: 'usb' (default) or 'csi'\n"
           "  -r, --resolution WxH  Requested capture resolution, e.g. 1280x720\n"
           "  -f, --framerate fps   Requested capture framerate, e.g. 30\n"
           "\n"
           "Examples:\n"
           "  vcat camera                              Use /dev/video0 (USB)\n"
           "  vcat camera /dev/video2                  Use a specific USB camera\n"
           "  vcat camera --list                       Show all detected cameras\n"
           "  vcat camera --list --type csi            Show only CSI cameras\n"
           "  vcat camera --type csi --device unicam   Raspberry Pi camera, by name\n"
           "  vcat camera --type csi --device 0        Jetson camera, by sensor index\n");
}

/**
 * @brief Check that the selected CSI backend and camera can actually be used,
 *        listing the available camera names when they cannot.
 *
 * @param[in] backend the selected backend, or NULL when none is available
 *
 * @return true when the camera can be used
 */
static bool validateCsiCamera(const CameraBackend *backend)
{
    gstInitQuiet();

    if (backend == NULL)
    {
        fprintf(stderr, "No CSI camera source is available.\n"
                        "On Raspberry Pi OS, install the GStreamer libcamera plugin:\n"
                        "  sudo apt install gstreamer1.0-libcamera\n"
                        "Run 'vcat camera --list' to see the USB cameras.\n");
        return false;
    }

    if (backend->provider == NULL)
    {
        // Vendor backends (Jetson, Qualcomm, ...) have no GStreamer device
        // provider to enumerate; the element's existence was already verified
        // during selection, so there is nothing more to check here.
        return true;
    }

    GstDeviceProvider *provider = gst_device_provider_factory_get_by_name(backend->provider);
    if (provider == NULL)
    {
        fprintf(stderr, "Unable to enumerate CSI cameras via '%s'\n", backend->provider);
        return false;
    }

    if (!gst_device_provider_start(provider))
    {
        fprintf(stderr, "Unable to enumerate CSI cameras via '%s'\n", backend->provider);
        gst_object_unref(provider);
        return false;
    }

    GList *devices = gst_device_provider_get_devices(provider);
    bool usable = true;

    if (gDeviceGiven)
    {
        bool found = false;
        for (GList *item = devices; item != NULL; item = item->next)
        {
            GstDevice *device = GST_DEVICE(item->data);
            if (!deviceMatchesToken(device, gDeviceToken))
            {
                continue;
            }
            found = true;

            if (backend->providerKey == NULL)
            {
                // libcamerasrc matches the full camera id; expand a short
                // model name so the source finds the camera.
                gchar *displayName = gst_device_get_display_name(device);
                if (displayName != NULL)
                {
                    snprintf(gDeviceToken, sizeof(gDeviceToken), "%s", displayName);
                }
                g_free(displayName);
            }
            break;
        }

        if (!found)
        {
            fprintf(stderr, "CSI camera '%s' not found. Available cameras:\n", gDeviceToken);
            for (GList *item = devices; item != NULL; item = item->next)
            {
                gchar *displayName = gst_device_get_display_name(GST_DEVICE(item->data));
                gchar *model = cameraModelFromName(displayName);
                if (model != NULL && strcmp(model, displayName) != 0)
                {
                    fprintf(stderr, "  %s   (%s)\n", model, displayName);
                }
                else
                {
                    fprintf(stderr, "  %s\n", displayName != NULL ? displayName : "unknown camera");
                }
                g_free(model);
                g_free(displayName);
            }
            usable = false;
        }
    }
    else if (devices == NULL)
    {
        fprintf(stderr, "No CSI cameras detected. Check the ribbon cable and camera\n"
                        "detection in the boot configuration, or run 'vcat camera --list'.\n");
        usable = false;
    }

    g_list_free_full(devices, gst_object_unref);

    gst_device_provider_stop(provider);
    gst_object_unref(provider);

    return usable;
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
            // Record and keep parsing: --list may be combined with --type.
            gListRequested = true;
            break;
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

    if (gListRequested)
    {
        CameraType filter = gCameraTypeRequested >= 1 ? (CameraType)gCameraTypeRequested : CAMERA_TYPE_UNKNOWN;
        return listDevices(filter) ? ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY : ARG_PARSE_RESULT_ERROR;
    }

    // Record the camera type: an explicit --type wins, otherwise USB is the
    // default. There is no inference from the device token; a non-"dev" token
    // without --type is rejected below so nothing is ever guessed.
    gCameraType = gCameraTypeRequested >= 1 ? (CameraType)gCameraTypeRequested : CAMERA_TYPE_USB;

    if (device != NULL)
    {
        // The token form must match the requested type; without --type only a
        // V4L2 node is unambiguous, anything else would be a guess.
        if (strncmp(device, "/dev/", 5) != 0 && gCameraTypeRequested < 0)
        {
            fprintf(stderr,
                    "'%s' is not a V4L2 device node. Pass --type csi to select a\n"
                    "camera by name or index, or run 'vcat camera --list' to see the\n"
                    "valid values.\n",
                    device);
            return ARG_PARSE_RESULT_ERROR;
        }

        gDeviceGiven = true;
        snprintf(gDeviceToken, sizeof(gDeviceToken), "%s", device);
    }

    // Validate the requested camera before building the pipeline.
    if (gCameraType == CAMERA_TYPE_USB)
    {
        lfLogInfo("USB camera device: '%s'", gDeviceToken);

        // Only a Linux V4L2 node can be verified on the file system; the other
        // token forms (Windows/macOS paths and indexes) cannot.
        if (strncmp(gDeviceToken, "/dev/", 5) == 0 && !fileExists(gDeviceToken))
        {
            fprintf(stderr, "Camera device does not exist: '%s'. Run 'vcat camera --list'.\n", gDeviceToken);
            return ARG_PARSE_RESULT_ERROR;
        }
    }
    else
    {
        if (!validateCsiCamera(selectBackend(CAMERA_TYPE_CSI)))
        {
            return ARG_PARSE_RESULT_ERROR;
        }
        // Logged after validation: a short model name is expanded to the full
        // camera id in there, which is what the source element receives.
        lfLogInfo("CSI camera: '%s'", gDeviceGiven ? gDeviceToken : "(default)");
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

/**
 * @brief Set the source element's camera selection property.
 *
 * Each source names its camera differently, so probe the element's properties
 * and set the first one that matches. One binary therefore works on every
 * platform and vendor source.
 *
 * @param[in] source the source element
 * @param[in] deviceId the user-provided camera token
 */
static void setDeviceProperty(GstElement *source, const char *deviceId)
{
    if (source == NULL || deviceId == NULL || deviceId[0] == '\0')
    {
        return;
    }

    GObjectClass *klass = G_OBJECT_GET_CLASS(source);

    if (g_object_class_find_property(klass, "camera-name") != NULL)
    {
        // libcamerasrc (camera name or id)
        g_object_set(G_OBJECT(source), "camera-name", deviceId, NULL);
    }
    else if (g_object_class_find_property(klass, "device") != NULL)
    {
        // v4l2src, nvv4l2camerasrc, imxv4l2videosrc (device node)
        g_object_set(G_OBJECT(source), "device", deviceId, NULL);
    }
    else if (g_object_class_find_property(klass, "sensor-id") != NULL)
    {
        // nvarguscamerasrc (expects integer index like "0", "1")
        g_object_set(G_OBJECT(source), "sensor-id", atoi(deviceId), NULL);
    }
    else if (g_object_class_find_property(klass, "camera") != NULL)
    {
        // qtiqmmfsrc (expects unsigned integer index like "0", "1")
        g_object_set(G_OBJECT(source), "camera", (guint)atoi(deviceId), NULL);
    }
    else if (g_object_class_find_property(klass, "camera-number") != NULL)
    {
        // rpicamsrc (expects integer index)
        g_object_set(G_OBJECT(source), "camera-number", atoi(deviceId), NULL);
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
    else if (g_object_class_find_property(klass, "serial") != NULL)
    {
        // tcambin, pylonsrc
        g_object_set(G_OBJECT(source), "serial", deviceId, NULL);
    }
}

/**
 * @brief Video Source Initialize
 *
 * Builds: source -> [capsfilter] -> decodebin -> RGBA converter chain ->
 * queue, with the queue exposed to the main pipeline. The source element comes
 * from the backend table: the first available candidate of the requested
 * camera type. decodebin hands raw cameras through as-is and decodes
 * Motion-JPEG-only cameras to raw video, and the RGBA chain guarantees the
 * host receives system-memory RGBA frames.
 *
 * @return RC_OK if successful
 */
rc_t videoSourceInitialize()
{
    char *failMessage = "Unable to initialize camera video source";

    gSourceBin = gst_bin_new(VCAT_VIDEO_SOURCE_BIN_NAME);
    VERIFY_ELEMENT(gSourceBin, failMessage);

    const CameraBackend *backend = selectBackend(gCameraType);
    GstElement *source = backend == NULL ? NULL : gst_element_factory_make(backend->element, NULL);
    if (source == NULL)
    {
        lfLogError("%s: no %s camera source is available", failMessage, gCameraType == CAMERA_TYPE_CSI ? "CSI" : "USB");
        return RC_ERROR;
    }
    lfLogInfo("Camera source element: '%s' (%s)", backend->element, backend->platform);

    if (gDeviceGiven)
    {
        // Only a user-provided token is forwarded; without one the source
        // picks its own default camera.
        setDeviceProperty(source, gDeviceToken);
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
