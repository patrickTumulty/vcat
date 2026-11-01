#include "crox_types.h"
#include "gst/gstcaps.h"
#include "gst/gstelement.h"
#include "gst/gstpad.h"
#include "gst/gststructure.h"
#include "gst/gstutils.h"
#include "logging_facade.h"
#include "vcat_plugin_framework.h"
#include "vcat_video_source_utils.h"

#include <ctype.h>
#include <getopt.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define RTSP_DEFAULT_PORT 554
#define RTSP_MAX_LATENCY_MS 60000

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;
static GstElement *gH264Depay = NULL;
static GstElement *gH265Depay = NULL;
static VcatH26xRouter gRouter = {0};
static bool gRouted = false;
static char gLocation[PATH_MAX] = "";
static unsigned int gLatencyMs = 200;
static bool gForceTcp = false;
static char gUserId[128] = "";
static char gPassword[128] = "";

VCAT_DEFINE_PLUGIN_NAME("RTSP")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)
VCAT_DEFINE_PLUGIN_VERSION(VCAT_VERSION)
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("rtsp://<host>:<port>/<path>")
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION("Receive an RTSP video stream from a camera or server")

static void videoSourcePrintHelp()
{
    printf("rtsp://<host>:<port>/<path> - Receive an RTSP video stream\n"
           "\n"
           "Usage: vcat rtsp://<host>:<port>/<path> [OPTIONS]\n"
           "\n"
           "The URI host accepts an IPv4 address (rtsp://192.168.1.50:8554/live),\n"
           "a hostname (rtsp://camera.local:8554/live) or 'localhost'\n"
           "(rtsp://localhost:8554/test). The port defaults to 554 and the path\n"
           "is optional, selecting the stream on the server.\n"
           "\n"
           "Credentials may be embedded in the URI (rtsp://user:pass@host/live)\n"
           "or provided with --user and --password.\n"
           "\n"
           "Options:\n"
           "  -h, --help            Show this help\n"
           "  -v, --version         Show version information\n"
           "  -l, --latency <ms>    Jitter buffer latency in milliseconds (default 200)\n"
           "  -t, --tcp             Force RTSP over TCP interleaved transport\n"
           "  -u, --user <name>     RTSP authentication user name\n"
           "  -w, --password <pw>   RTSP authentication password\n");
}

/**
 * @brief parse "rtsp://[user[:password]@]<host>[:port][/<path>]" into gLocation
 *
 * The location is rebuilt with an explicit port (default 554) for rtspsrc.
 * URI-embedded credentials fill in gUserId/gPassword only where --user and
 * --password left them unset, so explicit options win.
 *
 * @param[in] uri the source URI token, e.g. "rtsp://127.0.0.1:8554/test"
 *
 * @return true when parsed; gLocation is untouched otherwise
 */
static bool rtspParseSourceUri(const char *uri)
{
    static const char kScheme[] = "rtsp://";

    if (strncmp(uri, kScheme, sizeof(kScheme) - 1) != 0)
    {
        return false;
    }

    const char *authority = uri + sizeof(kScheme) - 1;

    // The path (if any) starts at the first '/': everything before it is the
    // authority, "[user[:password]@]<host>[:port]".
    const char *slash = strchr(authority, '/');
    size_t authorityLen = slash != NULL ? (size_t)(slash - authority) : strlen(authority);
    const char *path = slash != NULL ? slash : "";

    // Credentials end at the last '@' inside the authority.
    const char *at = NULL;
    for (const char *c = authority; c < authority + authorityLen; c++)
    {
        if (*c == '@')
        {
            at = c;
        }
    }

    const char *hostPort = authority;
    if (at != NULL)
    {
        size_t userInfoLen = (size_t)(at - authority);
        const char *userInfoColon = memchr(authority, ':', userInfoLen);
        size_t userLen = userInfoColon != NULL ? (size_t)(userInfoColon - authority) : userInfoLen;
        size_t passwordLen = userInfoColon != NULL ? userInfoLen - userLen - 1 : 0;

        if (userLen == 0 || userLen >= sizeof(gUserId) || passwordLen >= sizeof(gPassword))
        {
            return false;
        }

        if (gUserId[0] == '\0')
        {
            memcpy(gUserId, authority, userLen);
            gUserId[userLen] = '\0';
        }

        if (gPassword[0] == '\0' && passwordLen > 0)
        {
            memcpy(gPassword, userInfoColon + 1, passwordLen);
            gPassword[passwordLen] = '\0';
        }

        hostPort = at + 1;
    }

    size_t hostPortLen = (size_t)(authority + authorityLen - hostPort);

    // The host is an IPv4 address, a hostname, or 'localhost'; bracketed IPv6
    // is not supported, matching the other network sources.
    const char *portColon = memchr(hostPort, ':', hostPortLen);
    size_t hostLen = portColon != NULL ? (size_t)(portColon - hostPort) : hostPortLen;

    if (hostLen == 0)
    {
        return false;
    }

    for (size_t i = 0; i < hostLen; i++)
    {
        if (!isalnum((unsigned char)hostPort[i]) && hostPort[i] != '.' && hostPort[i] != '-')
        {
            return false;
        }
    }

    int port = RTSP_DEFAULT_PORT;
    if (portColon != NULL)
    {
        size_t portLen = hostPortLen - hostLen - 1;
        if (portLen == 0)
        {
            return false;
        }

        for (size_t i = 0; i < portLen; i++)
        {
            if (!isdigit((unsigned char)portColon[1 + i]))
            {
                return false;
            }
        }

        long parsedPort = strtol(portColon + 1, NULL, 10);
        if (parsedPort < 1 || parsedPort > 65535)
        {
            return false;
        }
        port = (int)parsedPort;
    }

    int written = snprintf(gLocation, sizeof(gLocation), "rtsp://%.*s:%d%s", (int)hostLen, hostPort, port, path);

    return written > 0 && (size_t)written < sizeof(gLocation);
}

/**
 * @brief process the source's command line, e.g. "vcat rtsp://1.2.3.4:554/live [OPTIONS]"
 *
 * Parses the source URI (argv[0]), prints the source-specific help on
 * "-h"/"--help", and consumes remaining options.
 *
 * @param[in] argc count, including the source URI in argv[0]
 * @param[in] argv the source URI token and its options
 *
 * @return InputArgsResult telling the host whether to continue, exit early, or fail
 */
InputArgsResult videoSourceProcessInputArgs(int argc, char *argv[])
{
    static const struct option cliOptions[] = {
        {"version",  no_argument,       NULL, 'v'},
        {"help",     no_argument,       NULL, 'h'},
        {"latency",  required_argument, NULL, 'l'},
        {"tcp",      no_argument,       NULL, 't'},
        {"user",     required_argument, NULL, 'u'},
        {"password", required_argument, NULL, 'w'},
        {NULL,       0,                 NULL, 0  }  //
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "+hvtl:u:w:", cliOptions, NULL)) != -1)
    {
        switch (opt)
        {
        case 'h':
            videoSourcePrintHelp();
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case 'v':
            printf("rtsp v%s\n", vcatPluginGetVersion());
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case 'l': {
            char *end = NULL;
            long latency = strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || latency <= 0 || latency > RTSP_MAX_LATENCY_MS)
            {
                fprintf(stderr, "Invalid latency '%s', expected milliseconds, e.g. 200.\n", optarg);
                return ARG_PARSE_RESULT_ERROR;
            }
            gLatencyMs = (unsigned int)latency;
            break;
        }
        case 't':
            gForceTcp = true;
            break;
        case 'u':
            if (strlen(optarg) >= sizeof(gUserId))
            {
                fprintf(stderr, "User name is too long (max %zu characters).\n", sizeof(gUserId) - 1);
                return ARG_PARSE_RESULT_ERROR;
            }
            snprintf(gUserId, sizeof(gUserId), "%s", optarg);
            break;
        case 'w':
            if (strlen(optarg) >= sizeof(gPassword))
            {
                fprintf(stderr, "Password is too long (max %zu characters).\n", sizeof(gPassword) - 1);
                return ARG_PARSE_RESULT_ERROR;
            }
            snprintf(gPassword, sizeof(gPassword), "%s", optarg);
            break;
        default:
            fprintf(stderr, "Unknown option. See 'vcat rtsp --help'.\n");
            return ARG_PARSE_RESULT_ERROR;
        }
    }

    // Parse the source URI, e.g. "rtsp://127.0.0.1:8554/test". URI-embedded
    // credentials fill in what --user/--password left unset.
    if (rtspParseSourceUri(argv[0]))
    {
        return ARG_PARSE_RESULT_SUCCESS;
    }

    fprintf(stderr, "Invalid source URI '%s'. See 'vcat rtsp --help'.\n", argv[0]);

    return ARG_PARSE_RESULT_ERROR;
}

/**
 * @brief map an RTSP RTP encoding name onto the router's supported codecs
 *
 * @param[in] encodingName caps "encoding-name" field, e.g. "H264", "H265"
 *
 * @return the matching codec, VIDEO_CODEC_UNKNOWN when unsupported
 */
static VideoCodec rtspCodecFromEncodingName(const char *encodingName)
{
    if (encodingName == NULL)
    {
        return VIDEO_CODEC_UNKNOWN;
    }

    if (strcasecmp(encodingName, "H264") == 0)
    {
        return VIDEO_CODEC_H264;
    }

    if (strcasecmp(encodingName, "H265") == 0 || strcasecmp(encodingName, "HEVC") == 0)
    {
        return VIDEO_CODEC_H265;
    }

    return VIDEO_CODEC_UNKNOWN;
}

/**
 * @brief rtspsrc "select-stream" handler: only set up supported video streams
 *
 * Audio and metadata tracks are declined so the server never allocates
 * transport for them, and unsupported video encodings fail here instead of
 * producing a pad that can never be routed. On GStreamer releases without
 * this signal the connection is a no-op and the "pad-added" handler does the
 * same filtering instead.
 *
 * @return TRUE to set up the stream, FALSE to decline it
 */
static gboolean rtspSrcSelectStream(GstElement *src, guint streamId, GstCaps *caps, gpointer userData)
{
    (void)src;
    (void)streamId;
    (void)userData;

    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *media = gst_structure_get_string(structure, "media");
    bool isVideo = media != NULL && strcasecmp(media, "video") == 0;

    if (!isVideo)
    {
        lfLogInfo("Declining non-video RTSP stream (media=%s)", media != NULL ? media : "unknown");
        return FALSE;
    }

    const gchar *encodingName = gst_structure_get_string(structure, "encoding-name");
    if (rtspCodecFromEncodingName(encodingName) == VIDEO_CODEC_UNKNOWN)
    {
        lfLogError("Declining RTSP video stream with unsupported encoding '%s' (supported: H264, H265)",
                   encodingName != NULL ? encodingName : "unknown");
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief rtspsrc "pad-added" handler: wire the first supported video stream
 *
 * Links the RTP pad into its depayloader, the depayloader into the router's
 * bitstream parser, and the router's pinned decode chain tail into the
 * exposed queue. Later pads are skipped, matching the other network sources.
 *
 * @param[in] newPad the rtspsrc stream pad, caps "application/x-rtp"
 */
static void rtspSrcPadAdded(GstElement *src, GstPad *newPad, gpointer userData)
{
    (void)src;
    (void)userData;

    lfLogInfo("RTSP source pad added: %s", GST_PAD_NAME(newPad));

    if (gRouted)
    {
        lfLogWarn("Unable to link new pad: already linked");
        return;
    }

    GstCaps *caps = gst_pad_get_current_caps(newPad);
    if (!caps)
    {
        caps = gst_pad_query_caps(newPad, NULL);
        if (!caps)
        {
            lfLogError("Unable to read pad caps");
            return;
        }
    }

    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *media = gst_structure_get_string(structure, "media");
    const gchar *encodingName = gst_structure_get_string(structure, "encoding-name");
    VideoCodec codec = rtspCodecFromEncodingName(encodingName);

    gchar *capsStr = gst_caps_to_string(caps);
    lfLogInfo("RTSP stream: %s", capsStr);
    g_free(capsStr);

    if (media == NULL || strcasecmp(media, "video") != 0 || codec == VIDEO_CODEC_UNKNOWN)
    {
        lfLogInfo("Skipping non-video or unsupported RTSP stream (media=%s encoding=%s)",
                  media != NULL ? media : "unknown", encodingName != NULL ? encodingName : "unknown");
        gst_caps_unref(caps);
        return;
    }

    gst_caps_unref(caps);

    GstElement *depay = codec == VIDEO_CODEC_H264 ? gH264Depay : gH265Depay;
    GstElement *h26xparse = codec == VIDEO_CODEC_H264 ? gRouter.h264parse : gRouter.h265parse;

    // Wire the depayloader into its pinned decode chain: decoder and RGBA
    // tail are built here on first use, the parser was linked by the router.
    GstElement *tail = NULL;
    if (vcatH26xRouterPrepareCodecChain(&gRouter, codec, &tail) != RC_OK)
    {
        return;
    }

    GstPad *depaySink = gst_element_get_static_pad(depay, "sink");
    if (depaySink == NULL)
    {
        lfLogError("Unable to find depayloader sink pad");
        return;
    }

    if (gst_pad_is_linked(depaySink))
    {
        lfLogWarn("Unable to link new pad: depayloader already linked");
        gst_object_unref(depaySink);
        return;
    }

    GstPadLinkReturn padLink = gst_pad_link(newPad, depaySink);
    gst_object_unref(depaySink);

    if (GST_PAD_LINK_FAILED(padLink))
    {
        lfLogError("Failed to link rtspsrc -> depayloader: %s", gst_pad_link_get_name(padLink));
        return;
    }

    if (!gst_element_link(depay, h26xparse))
    {
        lfLogError("Failed to link depayloader -> parser");
        return;
    }

    if (!gst_element_link(tail, gSourceElement))
    {
        lfLogError("Failed to link decode chain tail -> queue");
        return;
    }

    gRouted = true;

    lfLogInfo("Routed %s RTSP video stream", videoCodecToString(codec));
}

/**
 * @brief Video Source Initialize
 *
 * Builds: rtspsrc -> [pad-added: depayloader -> parser -> pinned decode chain]
 * -> leaky queue, with the queue exposed to the main pipeline. The SDP
 * negotiated with the server decides which depayloader receives data; the
 * shared router guarantees the host receives system-memory RGBA frames.
 *
 * @return RC_OK if successful
 */
rc_t videoSourceInitialize()
{
    char *failMessage = "Unable to initialize RTSP video source";

    gSourceBin = gst_bin_new(VCAT_VIDEO_SOURCE_BIN_NAME);
    VERIFY_ELEMENT(gSourceBin, failMessage);

    GstElement *source = gst_element_factory_make("rtspsrc", NULL);
    VERIFY_ELEMENT(source, failMessage);

    lfLogInfo("RTSP video source location=%s latency=%u transport=%s auth=%s", gLocation, gLatencyMs,
              gForceTcp ? "tcp" : "auto", gUserId[0] != '\0' ? "user/password" : "none");

    g_object_set(source,                //
                 "location", gLocation, //
                 "latency", gLatencyMs, //
                 NULL);

    if (gForceTcp)
    {
        // A flags property set through its string form, keeping the GstRTSP
        // type headers out of the plugin.
        gst_util_set_object_arg(G_OBJECT(source), "protocols", "tcp");
    }

    if (gUserId[0] != '\0')
    {
        g_object_set(source,               //
                     "user-id", gUserId,   //
                     "user-pw", gPassword, //
                     NULL);
    }

    GstElement *h264Depay = gst_element_factory_make("rtph264depay", NULL);
    VERIFY_ELEMENT(h264Depay, failMessage);

    GstElement *h265Depay = gst_element_factory_make("rtph265depay", NULL);
    VERIFY_ELEMENT(h265Depay, failMessage);

    gH264Depay = h264Depay;
    gH265Depay = h265Depay;

    if (vcatH26xRouterCreate(&gRouter, GST_BIN(gSourceBin)) != RC_OK)
    {
        return RC_ERROR;
    }

    GstElement *queue = gst_element_factory_make("queue", NULL);
    VERIFY_ELEMENT(queue, failMessage);

    g_object_set(queue,                 //
                 "max-size-buffers", 1, //
                 "max-size-bytes", 0,   //
                 "max-size-time", 0,    //
                 "leaky", 2,            // Downstream Leaky
                 NULL);

    gSourceElement = queue;

    gst_bin_add_many(GST_BIN(gSourceBin), //
                     source,              //
                     h264Depay,           //
                     h265Depay,           //
                     queue,               //
                     NULL);

    // Both depayloaders wait unlinked until the SDP describes a stream; the
    // handlers above wire the matching one in, the other stays idle.
    g_signal_connect(source, "pad-added", G_CALLBACK(rtspSrcPadAdded), NULL);
    g_signal_connect(source, "select-stream", G_CALLBACK(rtspSrcSelectStream), NULL);

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
 * @return the leaky queue element the host links its render path into
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
