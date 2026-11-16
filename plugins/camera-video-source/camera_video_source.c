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

static const char *cameraTypeLabel(CameraType type)
{
    return type == CAMERA_TYPE_USB ? "USB" : "CSI";
}

static const char *cameraTypeFlagName(CameraType type)
{
    return type == CAMERA_TYPE_USB ? "usb" : "csi";
}

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
    {CAMERA_TYPE_CSI, "nvarguscamerasrc", NULL,                NULL,          "NVIDIA Jetson (Argus)",                   "<sensor index>, e.g. 0"},
    {CAMERA_TYPE_CSI, "nvv4l2camerasrc",  "v4l2deviceprovider", "device.path", "NVIDIA Jetson (V4L2/NVMM)",               "<node>, e.g. /dev/video0"},
    {CAMERA_TYPE_CSI, "qtiqmmfsrc",       NULL,                NULL,          "Qualcomm (RB5, QCS6490, ...)",            "<camera index>, e.g. 0"},
    {CAMERA_TYPE_CSI, "mediasrcbin",      NULL,                NULL,          "AMD / Xilinx (Kria KV260, ZynqMP)",       "<media controller node>"},
    {CAMERA_TYPE_CSI, "rkcamsrc",         NULL,                NULL,          "Rockchip (older BSPs)",                   "<node>"},
    {CAMERA_TYPE_CSI, "imxv4l2videosrc",  NULL,                NULL,          "NXP i.MX (older BSPs)",                   "<node>"},
    {CAMERA_TYPE_CSI, "libcamerasrc",     "libcameraprovider", NULL,          "Raspberry Pi / modern Linux (libcamera)", "<camera name>, e.g. unicam"},
    {CAMERA_TYPE_CSI, "tcambin",          NULL,                NULL,          "The Imaging Source",                      "<serial>"},
    {CAMERA_TYPE_CSI, "pylonsrc",         NULL,                NULL,          "Basler",                                  "<serial>"},
    {CAMERA_TYPE_CSI, "rpicamsrc",        NULL,                NULL,          "Raspberry Pi (legacy MMAL)",              "<camera index>, e.g. 0"},
    {CAMERA_TYPE_CSI, "v4l2src",          "v4l2deviceprovider", "device.path", "Generic Linux raw V4L2",                 "<node>, e.g. /dev/video11"},
    {CAMERA_TYPE_CSI, "pipewiresrc",      NULL,                NULL,          "Linux desktop (PipeWire)",                "<node id>"},
};
// clang-format on
static const int gBackendsLen = sizeof(gBackends) / sizeof(gBackends[0]);

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
 * @brief everything the command line specifies about the camera to capture.
 *
 * Filled in by argument parsing, read by pipeline setup.
 */
typedef struct CameraOptions
{
    char deviceToken[PATH_MAX]; // camera to use; canonical form after lookup
    bool deviceGiven;           // false: the source picks its own default camera
    CameraType type;            // resolved connection type
    CameraType typeRequested;   // explicit --type; CAMERA_TYPE_UNKNOWN when unset
    bool printListRequested;    // --list
    int resolutionWidth;        // 0: not requested
    int resolutionHeight;       // 0: not requested
    int framerate;              // 0: not requested
} CameraOptions;

static CameraOptions gOptions = {
    .deviceToken = "/dev/video0",
    .type = CAMERA_TYPE_USB,
    .typeRequested = CAMERA_TYPE_UNKNOWN,
};

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;
static VcatRgbaChain gRgbaChain = {0};

VCAT_DEFINE_PLUGIN_NAME("Camera Source")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)
VCAT_DEFINE_PLUGIN_VERSION(VCAT_VERSION)
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("camera")
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION("Capture a USB or CSI camera (webcam / Raspberry Pi camera module)")

/**
 * @brief Print a device as "token  display name", omitting the second column
 *        when it adds nothing.
 */
static void printDevice(FILE *stream, const char *token, const char *displayName)
{
    if (token == NULL)
    {
        token = "unknown device";
    }

    if (displayName != NULL && strcmp(displayName, token) != 0)
    {
        fprintf(stream, "  %-*s   %s\n", 18, token, displayName);
    }
    else
    {
        fprintf(stream, "  %s\n", token);
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
 * @brief Read a string device property as an owned copy.
 *
 * @return a newly allocated string, or NULL when the property is absent
 */
static gchar *devicePropertyString(GstDevice *device, const char *key)
{
    GstStructure *props = gst_device_get_properties(device);
    const gchar *value = props == NULL ? NULL : gst_structure_get_string(props, key);
    gchar *copy = value != NULL ? g_strdup(value) : NULL;
    if (props != NULL)
    {
        gst_structure_free(props);
    }
    return copy;
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
        for (guint i = 0; props != NULL && i < gst_structure_n_fields(props); i++)
        {
            const gchar *fieldName = gst_structure_nth_field_name(props, i);
            const GValue *value = gst_structure_get_value(props, fieldName);
            if (G_VALUE_TYPE(value) == G_TYPE_STRING && strcmp(g_value_get_string(value), token) == 0)
            {
                matched = true;
                break;
            }
        }
        if (props != NULL)
        {
            gst_structure_free(props);
        }
    }

    g_free(displayName);

    return matched;
}

/**
 * @brief Resolve a device token against enumerated devices to its canonical
 *        CLI form: the provider's tokenKey property when it carries one, else
 *        the display name (the full libcamera camera id).
 *
 * @return the canonical token (owned), or NULL when no device matched
 */
static gchar *resolveDeviceToken(GList *devices, const char *tokenKey, const char *token)
{
    for (GList *item = devices; item != NULL; item = item->next)
    {
        GstDevice *device = GST_DEVICE(item->data);
        if (!deviceMatchesToken(device, token))
        {
            continue;
        }

        gchar *canonical = tokenKey != NULL ? devicePropertyString(device, tokenKey) : NULL;
        if (canonical == NULL)
        {
            canonical = gst_device_get_display_name(device);
        }
        return canonical;
    }
    return NULL;
}

/**
 * @brief Check whether a device belongs to one of the hidden V4L2
 *        infrastructure drivers.
 */
static bool isHiddenV4l2Device(GstDevice *device)
{
    gchar *driver = devicePropertyString(device, "v4l2.device.driver");

    bool hidden = false;
    for (int i = 0; i < gHiddenV4l2DriversLen && !hidden; i++)
    {
        hidden = driver != NULL && strcmp(driver, gHiddenV4l2Drivers[i]) == 0;
    }

    g_free(driver);
    return hidden;
}

/**
 * @brief The CLI token shown for a device: the provider's tokenKey property
 *        when it carries one, else the short model name derived from the
 *        display name.
 *
 * @return an owned string, or NULL when nothing can be derived
 */
static gchar *listingTokenForDevice(GstDevice *device, const char *tokenKey)
{
    if (tokenKey != NULL)
    {
        return devicePropertyString(device, tokenKey);
    }

    gchar *displayName = gst_device_get_display_name(device);
    gchar *model = cameraModelFromName(displayName);
    if (model == NULL)
    {
        model = g_strdup(displayName);
    }
    g_free(displayName);
    return model;
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

/**
 * @brief a provider enumeration in progress, with its resources.
 */
typedef struct DeviceEnumeration
{
    GList *devices;              // NULL when nothing was found
    GstDeviceProvider *provider; // stop + unref via releaseDeviceEnumeration()
    bool enumerated;             // false when the provider was unavailable
} DeviceEnumeration;

/**
 * @brief Enumerate the devices of a GStreamer device provider.
 *
 * @param[out] enumeration filled with the device list on success; release it
 *                        with releaseDeviceEnumeration() when done
 */
static void enumerateDevices(const char *providerFactoryName, DeviceEnumeration *enumeration)
{
    enumeration->devices = NULL;
    enumeration->provider = NULL;
    enumeration->enumerated = false;

    GstDeviceProvider *provider = gst_device_provider_factory_get_by_name(providerFactoryName);
    if (provider == NULL)
    {
        return;
    }

    if (!gst_device_provider_start(provider))
    {
        lfLogWarn("Unable to start device provider '%s'", providerFactoryName);
        gst_object_unref(provider);
        return;
    }

    enumeration->provider = provider;
    enumeration->devices = gst_device_provider_get_devices(provider);
    enumeration->enumerated = true;
}

static void releaseDeviceEnumeration(DeviceEnumeration *enumeration)
{
    if (enumeration->provider == NULL)
    {
        return;
    }

    g_list_free_full(enumeration->devices, gst_object_unref);
    gst_device_provider_stop(enumeration->provider);
    gst_object_unref(enumeration->provider);

    enumeration->devices = NULL;
    enumeration->provider = NULL;
}

/**
 * @brief Print the devices of one provider as "token  display name" rows.
 *
 * @return the number of devices printed; 0 when the provider is unavailable
 *         (e.g. the libcamera plugin is not installed)
 */
static int listProviderDevices(const char *providerFactoryName, const char *sectionHeader, const char *tokenKey)
{
    DeviceEnumeration enumeration;
    enumerateDevices(providerFactoryName, &enumeration);
    if (!enumeration.enumerated)
    {
        return 0;
    }

    int count = g_list_length(enumeration.devices);
    if (count > 0)
    {
        printf("%s", sectionHeader);
    }

    for (GList *item = enumeration.devices; item != NULL; item = item->next)
    {
        GstDevice *device = GST_DEVICE(item->data);
        if (isHiddenV4l2Device(device))
        {
            continue;
        }

        gchar *displayName = gst_device_get_display_name(device);
        gchar *token = listingTokenForDevice(device, tokenKey);
        printDevice(stdout, token, displayName);
        g_free(token);
        g_free(displayName);
    }

    releaseDeviceEnumeration(&enumeration);
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
 *
 * @return true when anything was printed (devices or hints)
 */
static bool listBackendSection(CameraType type, const CameraBackend *selected, const char *providerToSkip)
{
    const char *typeFlag = cameraTypeFlagName(type);
    const char *typeLabel = cameraTypeLabel(type);
    const char *sectionHeader = type == CAMERA_TYPE_USB //
                                    ?
                                    "\nUSB cameras (pass with --type usb --device <node>):\n" :
                                    "\nCSI cameras (pass with --type csi --device <name>):\n";

    bool selectedEnumerated = selected != NULL &&           //
                              selected->provider != NULL && //
                              (providerToSkip == NULL || strcmp(selected->provider, providerToSkip) != 0);

    bool printed = false;
    if (selectedEnumerated && listProviderDevices(selected->provider, sectionHeader, selected->providerKey) > 0)
    {
        printed = true;
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
            printed = true;
        }
        printf("  %-36s --device %s\n", backend->platform, backend->tokenHint);
    }

    return printed;
}

/**
 * @brief Print every camera the installed backends can see, grouped by type.
 *
 * @param[in] filter camera type to restrict the listing to, or
 *                   CAMERA_TYPE_UNKNOWN for all types
 */
static void listDevices(CameraType filter)
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
        printed |= listBackendSection(CAMERA_TYPE_USB, selected, NULL);
        usbProviderUsed = selected != NULL ? selected->provider : NULL;
    }

    if (filter == CAMERA_TYPE_UNKNOWN || filter == CAMERA_TYPE_CSI)
    {
        const CameraBackend *selected = selectBackend(CAMERA_TYPE_CSI);
        printed |= listBackendSection(CAMERA_TYPE_CSI, selected, usbProviderUsed);
    }

    if (!printed)
    {
        printf("No cameras detected.\n");
    }
}

/**
 * @brief result of looking a device token up in the enumerated camera lists.
 */
typedef enum
{
    TOKEN_MATCH_NONE = 0,
    TOKEN_MATCH_USB,
    TOKEN_MATCH_CSI,
    TOKEN_MATCH_BOTH, // the token matched devices of both types; ask for --type
} TokenMatch;

/**
 * @brief Look a token up in one provider's devices, resolving it to the
 *        device's canonical CLI form.
 *
 * @param[out] resolvedOut set to the canonical token when a device matched
 *
 * @return true when the provider could enumerate; the match itself is
 *         reflected in @p resolvedOut
 */
static bool matchTokenInProvider(const char *providerFactoryName,
                                 const char *tokenKey,
                                 const char *token,
                                 gchar **resolvedOut)
{
    DeviceEnumeration enumeration;
    enumerateDevices(providerFactoryName, &enumeration);
    if (enumeration.enumerated)
    {
        *resolvedOut = resolveDeviceToken(enumeration.devices, tokenKey, token);
    }
    releaseDeviceEnumeration(&enumeration);
    return enumeration.enumerated;
}

/**
 * @brief Classify a device token by looking it up in the cameras the installed
 *        backends can enumerate.
 *
 * The generic raw-V4L2 CSI fallback lists the same nodes as the USB side and
 * is skipped, so CSI cannot spuriously match a USB webcam's name.
 *
 * @param[in] token the user-provided device token
 * @param[out] usbResolvedOut canonical token of the matched USB device
 * @param[out] csiResolvedOut canonical token of the matched CSI camera
 * @param[out] anyListedOut true when at least one provider could enumerate
 *
 * @return which camera types list a device matching the token
 */
static TokenMatch lookupCameraToken(const char *token,
                                    gchar **usbResolvedOut,
                                    gchar **csiResolvedOut,
                                    bool *anyListedOut)
{
    bool usbListed = false;
    bool csiListed = false;

    const CameraBackend *usbBackend = selectBackend(CAMERA_TYPE_USB);
    if (usbBackend != NULL && usbBackend->provider != NULL)
    {
        usbListed = matchTokenInProvider(usbBackend->provider, usbBackend->providerKey, token, usbResolvedOut);
    }

    const CameraBackend *csiBackend = selectBackend(CAMERA_TYPE_CSI);
    if (csiBackend != NULL && csiBackend->provider != NULL && //
        (usbBackend == NULL || usbBackend->provider == NULL || strcmp(csiBackend->provider, usbBackend->provider) != 0))
    {
        csiListed = matchTokenInProvider(csiBackend->provider, csiBackend->providerKey, token, csiResolvedOut);
    }

    *anyListedOut = usbListed || csiListed;

    if (*usbResolvedOut != NULL)
    {
        return *csiResolvedOut != NULL ? TOKEN_MATCH_BOTH : TOKEN_MATCH_USB;
    }
    return *csiResolvedOut != NULL ? TOKEN_MATCH_CSI : TOKEN_MATCH_NONE;
}

static void printAmbiguousTokenError(const char *token, const char *usbMatch, const char *csiMatch)
{
    fprintf(stderr, "Camera '%s' matches more than one camera type:\n", token);
    if (usbMatch != NULL)
    {
        fprintf(stderr, "  USB: %s\n", usbMatch);
    }
    if (csiMatch != NULL)
    {
        fprintf(stderr, "  CSI: %s\n", csiMatch);
    }
    fprintf(stderr, "Pass --type usb or --type csi to pick one.\n");
}

static void printUnknownTokenError(const char *token, bool anyListed)
{
    if (anyListed)
    {
        fprintf(stderr, "Unknown camera '%s'.\nRun 'vcat camera --list' to see the detected cameras.\n", token);
        return;
    }

    // Nothing could be enumerated (e.g. Windows/macOS): the token cannot be
    // classified, so an explicit type is required.
    fprintf(stderr,
            "Unable to determine the camera type of '%s'.\n"
            "Pass --type usb or --type csi, or run 'vcat camera --list'.\n",
            token);
}

/**
 * @brief Apply the user's device token: record it and resolve its camera type
 *        and canonical form.
 *
 * @return false when the token cannot be classified (error already printed)
 */
static bool applyDeviceToken(const char *token, CameraOptions *options)
{
    options->deviceGiven = true;
    snprintf(options->deviceToken, sizeof(options->deviceToken), "%s", token);

    if (options->typeRequested != CAMERA_TYPE_UNKNOWN)
    {
        return true; // explicit --type already resolved the camera type
    }

    if (strncmp(token, "/dev/", 5) == 0)
    {
        // Unambiguous by form; this fast path also keeps the common USB case
        // free of provider startup cost. Existence is checked in validation.
        options->type = CAMERA_TYPE_USB;
        return true;
    }

    gchar *usbResolved = NULL;
    gchar *csiResolved = NULL;
    bool anyListed = false;
    TokenMatch match = lookupCameraToken(token, &usbResolved, &csiResolved, &anyListed);

    bool classified = match == TOKEN_MATCH_USB || match == TOKEN_MATCH_CSI;
    if (classified)
    {
        options->type = match == TOKEN_MATCH_USB ? CAMERA_TYPE_USB : CAMERA_TYPE_CSI;
        const char *resolved = match == TOKEN_MATCH_USB ? usbResolved : csiResolved;
        snprintf(options->deviceToken, sizeof(options->deviceToken), "%s", resolved);
    }
    else if (match == TOKEN_MATCH_BOTH)
    {
        printAmbiguousTokenError(token, usbResolved, csiResolved);
    }
    else
    {
        printUnknownTokenError(token, anyListed);
    }

    g_free(usbResolved);
    g_free(csiResolved);
    return classified;
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
           "  -t, --type <kind>     Camera connection type: 'usb' or 'csi'. Inferred by\n"
           "                        matching the device against the detected cameras;\n"
           "                        only needed to override\n"
           "  -r, --resolution WxH  Requested capture resolution, e.g. 1280x720\n"
           "  -f, --framerate fps   Requested capture framerate, e.g. 30\n"
           "\n"
           "Examples:\n"
           "  vcat camera                              Use /dev/video0 (USB)\n"
           "  vcat camera /dev/video2                  Use a specific USB camera\n"
           "  vcat camera ov5647                       CSI camera by model name (type inferred)\n"
           "  vcat camera --list                       Show all detected cameras\n"
           "  vcat camera --list --type csi            Show only CSI cameras\n"
           "  vcat camera --type csi --device 0        Jetson camera, by sensor index\n");
}

static bool parseCameraType(const char *value, CameraOptions *options)
{
    if (strcmp(value, "usb") == 0)
    {
        options->type = options->typeRequested = CAMERA_TYPE_USB;
    }
    else if (strcmp(value, "csi") == 0)
    {
        options->type = options->typeRequested = CAMERA_TYPE_CSI;
    }
    else
    {
        fprintf(stderr, "Invalid camera type '%s', expected 'usb' or 'csi'.\n", value);
        return false;
    }
    return true;
}

static bool parseResolution(const char *value, CameraOptions *options)
{
    if (sscanf(value, "%dx%d", &options->resolutionWidth, &options->resolutionHeight) != 2 || //
        options->resolutionWidth <= 0 ||                                                      //
        options->resolutionHeight <= 0)
    {
        fprintf(stderr, "Invalid resolution '%s', expected WxH, e.g. 1280x720.\n", value);
        return false;
    }
    return true;
}

static bool parseFramerate(const char *value, CameraOptions *options)
{
    options->framerate = atoi(value);
    if (options->framerate <= 0)
    {
        fprintf(stderr, "Invalid framerate '%s', expected a positive number.\n", value);
        return false;
    }
    return true;
}

/**
 * @brief Print the enumerated cameras as "token (full name)" rows.
 */
static void printCameraList(GList *devices)
{
    for (GList *item = devices; item != NULL; item = item->next)
    {
        GstDevice *device = GST_DEVICE(item->data);
        gchar *displayName = gst_device_get_display_name(device);
        gchar *token = listingTokenForDevice(device, NULL);
        printDevice(stderr, token, displayName);
        g_free(token);
        g_free(displayName);
    }
}

/**
 * @brief Check the requested USB camera against the file system.
 *
 * Only a Linux V4L2 node can be verified on the file system; the other token
 * forms (Windows/macOS paths and indexes) cannot.
 *
 * @return false when the camera cannot be used (error already printed)
 */
static bool validateUsbCamera(const CameraOptions *options)
{
    lfLogInfo("USB camera device: '%s'", options->deviceToken);

    if (strncmp(options->deviceToken, "/dev/", 5) == 0 && !fileExists(options->deviceToken))
    {
        fprintf(stderr, "Camera device does not exist: '%s'. Run 'vcat camera --list'.\n", options->deviceToken);
        return false;
    }
    return true;
}

/**
 * @brief Check that the selected CSI backend and camera can actually be used,
 *        listing the available camera names when they cannot. A short camera
 *        model name is expanded to the full camera id in here, which is what
 *        the source element receives.
 *
 * @param[in] backend the selected backend, or NULL when none is available
 * @param[in,out] options the token is replaced by its canonical form
 *
 * @return false when the camera cannot be used (error already printed)
 */
static bool validateCsiCamera(const CameraBackend *backend, CameraOptions *options)
{
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
        // Vendor backends (Jetson, Qualcomm, ...) have no device provider to
        // enumerate; the element's existence was verified during selection.
        return true;
    }

    DeviceEnumeration enumeration;
    enumerateDevices(backend->provider, &enumeration);
    if (!enumeration.enumerated)
    {
        fprintf(stderr, "Unable to enumerate CSI cameras via '%s'\n", backend->provider);
        return false;
    }

    bool usable = true;
    if (options->deviceGiven)
    {
        gchar *resolved = resolveDeviceToken(enumeration.devices, backend->providerKey, options->deviceToken);
        if (resolved != NULL)
        {
            snprintf(options->deviceToken, sizeof(options->deviceToken), "%s", resolved);
            g_free(resolved);
        }
        else
        {
            fprintf(stderr, "CSI camera '%s' not found. Available cameras:\n", options->deviceToken);
            printCameraList(enumeration.devices);
            usable = false;
        }
    }
    else if (enumeration.devices == NULL)
    {
        fprintf(stderr, "No CSI cameras detected. Check the ribbon cable and camera\n"
                        "detection in the boot configuration, or run 'vcat camera --list'.\n");
        usable = false;
    }

    releaseDeviceEnumeration(&enumeration);

    lfLogInfo("CSI camera: '%s'", options->deviceGiven ? options->deviceToken : "(default)");
    return usable;
}

/**
 * @brief Check the requested camera before the pipeline is built.
 *
 * @return false when the camera cannot be used (error already printed)
 */
static bool validateCamera(CameraOptions *options)
{
    if (options->type == CAMERA_TYPE_USB)
    {
        return validateUsbCamera(options);
    }
    return validateCsiCamera(selectBackend(CAMERA_TYPE_CSI), options);
}

/**
 * @brief Parse the camera source's command line, e.g. "vcat camera /dev/video0",
 *        and resolve the camera to capture.
 *
 * @param[in] argc count, including the source URI in argv[0]
 * @param[in] argv the source URI token and its options
 * @param[out] options filled in with the requested camera
 *
 * @return InputArgsResult telling the host whether to continue, exit early, or fail
 */
static InputArgsResult parseCameraArgs(int argc, char *argv[], CameraOptions *options)
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

    // The selected token must be the bare scheme: local sources take their
    // resource as a positional argument, not a URI. The host still routes
    // "://" forms here on the shared scheme, so reject those explicitly.
    if (strcmp(argv[0], "camera") != 0)
    {
        fprintf(stderr, "Invalid source '%s'. Use 'vcat camera <device>', not a URI.\n", argv[0]);
        return ARG_PARSE_RESULT_ERROR;
    }

    // An optional positional device argument, e.g. "vcat camera /dev/video2"
    const char *device = NULL;
    int optStart = 1;
    if (argc > 1 && argv[1][0] != '-')
    {
        device = argv[1];
        optStart = 2;
    }

    // getopt skips the first array element and keeps optind global across
    // calls, so shift the pointers into a fresh array and reset optind for a
    // clean scan.
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
            options->printListRequested = true; // --list may combine with --type
            break;
        case 'd':
            device = optarg;
            break;
        case 't':
            if (!parseCameraType(optarg, options))
            {
                return ARG_PARSE_RESULT_ERROR;
            }
            break;
        case 'r':
            if (!parseResolution(optarg, options))
            {
                return ARG_PARSE_RESULT_ERROR;
            }
            break;
        case 'f':
            if (!parseFramerate(optarg, options))
            {
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

    if (options->printListRequested)
    {
        listDevices(options->typeRequested);
        return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
    }

    if (device != NULL && !applyDeviceToken(device, options))
    {
        return ARG_PARSE_RESULT_ERROR;
    }

    return validateCamera(options) ? ARG_PARSE_RESULT_SUCCESS : ARG_PARSE_RESULT_ERROR;
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
    return parseCameraArgs(argc, argv, &gOptions);
}

/**
 * @brief Link a decodebin pad into the RGBA chain head when it is still free.
 */
static void linkPadToRgbaChain(GstPad *newPad, GstElement *rgbaChainHead)
{
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
}

/**
 * @brief decodebin pad-added handler: route video pads into the RGBA chain.
 */
static void decodeBinPadAdded(GstElement *_, GstPad *newPad, gpointer userData)
{
    GstElement *rgbaChainHead = GST_ELEMENT(userData);

    GstCaps *caps = gst_pad_get_current_caps(newPad);
    if (caps == NULL)
    {
        caps = gst_pad_query_caps(newPad, NULL);
        if (caps == NULL)
        {
            return;
        }
    }

    const gchar *name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    gchar *capsStr = gst_caps_to_string(caps);
    lfLogInfo("decodebin pad: %s", capsStr);
    g_free(capsStr);

    if (g_str_has_prefix(name, "video/"))
    {
        linkPadToRgbaChain(newPad, rgbaChainHead);
    }
    else
    {
        lfLogInfo("Skipping non-video pad '%s'", name);
    }

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
 * @param[in] deviceId the camera token in its canonical form
 */
static void setDeviceProperty(GstElement *source, const char *deviceId)
{
    if (source == NULL || deviceId == NULL || deviceId[0] == '\0')
    {
        return;
    }

    GObjectClass *klass = G_OBJECT_GET_CLASS(source);
    const char *property = NULL;

    if (g_object_class_find_property(klass, "camera-name") != NULL)
    {
        // libcamerasrc (camera name or id)
        property = "camera-name";
        g_object_set(G_OBJECT(source), "camera-name", deviceId, NULL);
    }
    else if (g_object_class_find_property(klass, "device") != NULL)
    {
        // v4l2src, nvv4l2camerasrc, imxv4l2videosrc (device node)
        property = "device";
        g_object_set(G_OBJECT(source), "device", deviceId, NULL);
    }
    else if (g_object_class_find_property(klass, "sensor-id") != NULL)
    {
        // nvarguscamerasrc (expects integer index like "0", "1")
        property = "sensor-id";
        g_object_set(G_OBJECT(source), "sensor-id", atoi(deviceId), NULL);
    }
    else if (g_object_class_find_property(klass, "camera") != NULL)
    {
        // qtiqmmfsrc (expects unsigned integer index like "0", "1")
        property = "camera";
        g_object_set(G_OBJECT(source), "camera", (guint)atoi(deviceId), NULL);
    }
    else if (g_object_class_find_property(klass, "camera-number") != NULL)
    {
        // rpicamsrc (expects integer index)
        property = "camera-number";
        g_object_set(G_OBJECT(source), "camera-number", atoi(deviceId), NULL);
    }
    else if (g_object_class_find_property(klass, "device-path") != NULL)
    {
        // mfvideosrc & ksvideosrc (Windows: DirectShow/MF symbolic link path)
        property = "device-path";
        g_object_set(G_OBJECT(source), "device-path", deviceId, NULL);
    }
    else if (g_object_class_find_property(klass, "device-index") != NULL)
    {
        // avfvideosrc (macOS/iOS: expects integer index like "0", "1")
        property = "device-index";
        g_object_set(G_OBJECT(source), "device-index", atoi(deviceId), NULL);
    }
    else if (g_object_class_find_property(klass, "serial") != NULL)
    {
        // tcambin, pylonsrc
        property = "serial";
        g_object_set(G_OBJECT(source), "serial", deviceId, NULL);
    }

    if (property != NULL)
    {
        lfLogInfo("Source '%s' camera property '%s' = '%s'", GST_ELEMENT_NAME(source), property, deviceId);
    }
    else
    {
        lfLogWarn("Source '%s' has no camera selection property; using its default camera", GST_ELEMENT_NAME(source));
    }
}

/**
 * @brief Create the source element for the resolved camera type.
 *
 * @return the element, or NULL when no candidate is available (logged)
 */
static GstElement *createSourceElement(const CameraOptions *options)
{
    const CameraBackend *backend = selectBackend(options->type);
    if (backend == NULL)
    {
        lfLogError("Unable to initialize camera video source: no %s camera source is available",
                   cameraTypeLabel(options->type));
        return NULL;
    }

    GstElement *source = gst_element_factory_make(backend->element, NULL);
    if (source == NULL)
    {
        lfLogError("Unable to initialize camera video source: failed to create '%s'", backend->element);
        return NULL;
    }

    lfLogInfo("Selected %s camera backend '%s' (%s)", cameraTypeLabel(options->type), backend->element,
              backend->platform);
    return source;
}

/**
 * @brief Caps constraining the capture resolution and framerate.
 *
 * @return owned caps, or NULL when no constraint was requested
 */
static GstCaps *captureConstraintCaps(int width, int height, int framerate)
{
    if (width <= 0 && framerate <= 0)
    {
        return NULL;
    }

    GstCaps *caps = gst_caps_new_empty();
    GstStructure *structure = gst_structure_new_empty("video/x-raw");

    if (width > 0)
    {
        gst_structure_set(structure, "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);
    }

    if (framerate > 0)
    {
        gst_structure_set(structure, "framerate", GST_TYPE_FRACTION, framerate, 1, NULL);
    }

    gst_caps_append_structure(caps, structure);
    return caps;
}

/**
 * @brief Log the requested capture constraint, when one was given.
 */
static void logCaptureConstraint(int width, int height, int framerate)
{
    GString *constraint = g_string_new(NULL);
    if (width > 0)
    {
        g_string_append_printf(constraint, "%dx%d", width, height);
    }
    if (framerate > 0)
    {
        g_string_append_printf(constraint, "%s%d fps", constraint->len > 0 ? " @ " : "", framerate);
    }
    if (constraint->len > 0)
    {
        lfLogInfo("Capture constraint: %s", constraint->str);
    }
    g_string_free(constraint, TRUE);
}

/**
 * @brief Log every element of the source bin, for support dumps.
 */
static void logSourceBinElements()
{
    GString *elements = g_string_new(NULL);
    for (GList *link = GST_BIN_CHILDREN(gSourceBin); link != NULL; link = link->next)
    {
        g_string_append_printf(elements, "%s%s", elements->len > 0 ? ", " : "", GST_ELEMENT_NAME(link->data));
    }
    lfLogInfo("Camera source bin elements: %s", elements->str);
    g_string_free(elements, TRUE);
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

    GstElement *source = createSourceElement(&gOptions);
    if (source == NULL)
    {
        return RC_ERROR;
    }

    if (gOptions.deviceGiven)
    {
        setDeviceProperty(source, gOptions.deviceToken);
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

    gst_bin_add_many(GST_BIN(gSourceBin), source, decoder, queue, NULL);

    GstElement *capsfilter = NULL;
    GstCaps *constraint = captureConstraintCaps(gOptions.resolutionWidth,  //
                                                gOptions.resolutionHeight, //
                                                gOptions.framerate);
    if (constraint != NULL)
    {
        capsfilter = gst_element_factory_make("capsfilter", NULL);
        VERIFY_ELEMENT(capsfilter, failMessage);

        g_object_set(G_OBJECT(capsfilter), "caps", constraint, NULL);
        gst_caps_unref(constraint);

        gst_bin_add(GST_BIN(gSourceBin), capsfilter);

        logCaptureConstraint(gOptions.resolutionWidth, gOptions.resolutionHeight, gOptions.framerate);

        if (!gst_element_link(source, capsfilter))
        {
            lfLogError("%s: failed to link source -> capsfilter", failMessage);
            return RC_ERROR;
        }
    }

    GstElement *sourceTail = capsfilter != NULL ? capsfilter : source;
    if (!gst_element_link(sourceTail, decoder))
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

    logSourceBinElements();

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
