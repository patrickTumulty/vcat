#include "crox_types.h"
#include "gst/gstcaps.h"
#include "gst/gstelement.h"
#include "gst/gstpad.h"
#include "gst/gstutils.h"
#include "logging_facade.h"
#include "vcat_network_endpoint.h"
#include "vcat_plugin_framework.h"
#include "vcat_video_source_utils.h"

#include <getopt.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

typedef enum
{
    VIDEO_CODEC_H264 = 0,
    VIDEO_CODEC_H265,
    VIDEO_CODEC_TS
} VideoCodec;

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;
static VcatH26xRouter gRouter = {0};
static VcatNetworkEndpoint gNetworkSource = {
    .address = {.octets = {127, 0, 0, 1}},
    .port = 5004,
};
static VideoCodec gCodec = VIDEO_CODEC_H264;

VCAT_DEFINE_PLUGIN_NAME("RTP")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)
VCAT_DEFINE_PLUGIN_VERSION(VCAT_VERSION)
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("rtp://<host>:<port>")
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION("Receive an RTP video stream over UDP")

static const char *videoCodecToString(VideoCodec codec)
{
    switch (codec)
    {
    case VIDEO_CODEC_H264: return "h264";
    case VIDEO_CODEC_H265: return "h265";
    case VIDEO_CODEC_TS: return "ts";
    default: return "none";
    }
}

static void videoSourcePrintHelp()
{
    printf("rtp://<host>:<port> - Receive RTP video over UDP\n"
           "\n"
           "Usage: vcat rtp://<host>:<port> [OPTIONS]\n"
           "\n"
           "The URI host accepts an IPv4 address (rtp://127.0.0.1:5004)\n"
           "or 'localhost' (rtp://localhost:5004).\n"
           "\n"
           "Options:\n"
           "  -h, --help            Show this help\n"
           "  -v, --version         Show version information\n"
           "  -c, --codec <type>    RTP payload encoding: h264 (default), h265, ts\n");
}

/**
 * @brief process the source's command line, e.g. "vcat rtp://1.2.3.4:5 [OPTIONS]"
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
        {"version", no_argument,      NULL, 'v'},
        {"help",    no_argument,      NULL, 'h'},
        {"codec",   required_argument, NULL, 'c'},
        {NULL,      0,                NULL, 0  }  //
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "+hvc:", cliOptions, NULL)) != -1)
    {
        switch (opt)
        {
        case 'h':
            videoSourcePrintHelp();
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case 'v':
            printf("rtp v%s\n", VCAT_VERSION);
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case 'c':
        {
            const char *codecArg = optarg;

            if (strcasecmp(codecArg, "h264") == 0)
            {
                gCodec = VIDEO_CODEC_H264;
            }
            else if (strcasecmp(codecArg, "h265") == 0)
            {
                gCodec = VIDEO_CODEC_H265;
            }
            else if (strcasecmp(codecArg, "ts") == 0)
            {
                gCodec = VIDEO_CODEC_TS;
            }
            else
            {
                fprintf(stderr, "Unknown codec '%s'. See 'vcat rtp --help'.\n", codecArg);
                return ARG_PARSE_RESULT_ERROR;
            }
            break;
        }
        default:
            fprintf(stderr, "Unknown option. See 'vcat rtp --help'.\n");
            return ARG_PARSE_RESULT_ERROR;
        }
    }

    // Parse the source URI, e.g. "rtp://127.0.0.1:5004" or "rtp://localhost:5004".
    // On failure the endpoint is left untouched and keeps its defaults.
    if (vcatNetworkEndpointParseUri(&gNetworkSource, argv[0], "rtp"))
    {
        return ARG_PARSE_RESULT_SUCCESS;
    }

    fprintf(stderr, "Invalid source URI '%s'. See 'vcat rtp --help'.\n", argv[0]);
    return ARG_PARSE_RESULT_ERROR;
}

/**
 * @brief Video Source Initialize
 *
 * The RTP payload caps tell the jitterbuffer the stream's encoding and clock
 * rate; the depayloader downstream is chosen to match. The MPEG-TS payload
 * carries elementary streams of its own, so its dynamic pads are routed
 * through the shared H.264/H.265 parser router.
 *
 * @return RC_OK if successful
 */
Rc videoSourceInitialize()
{
    char *failMessage = "Unable to initialize RTP video source";

    gSourceBin = gst_bin_new("video_src_bin");
    VERIFY_ELEMENT(gSourceBin, failMessage);

    GstElement *source = gst_element_factory_make("udpsrc", NULL);
    VERIFY_ELEMENT(source, failMessage);

    char ipStr[16];
    vcatIpv4AddressFormat(&gNetworkSource.address, ipStr, sizeof(ipStr));

    lfLogInfo("RTP video source ip=%s port=%d codec=%s", ipStr, gNetworkSource.port, videoCodecToString(gCodec));

    g_object_set(source,                         //
                 "port", gNetworkSource.port,    //
                 "address", ipStr,               //
                 "auto-multicast", true,         //
                 "buffer-size", 2 * 1024 * 1024, // 2 MiB
                 NULL);

    const char *capsStr;
    switch (gCodec)
    {
    case VIDEO_CODEC_H265:
        capsStr = "application/x-rtp, media=(string)video, payload=(int)96, clock-rate=(int)90000, "
                  "encoding-name=(string)H265";
        break;
    case VIDEO_CODEC_TS:
        // MP2T is a static RTP payload type (33); no clock rate needed.
        capsStr = "application/x-rtp, media=(string)video, payload=(int)33, encoding-name=(string)MP2T";
        break;
    case VIDEO_CODEC_H264:
    default:
        capsStr = "application/x-rtp, media=(string)video, payload=(int)96, clock-rate=(int)90000, "
                  "encoding-name=(string)H264";
        break;
    }

    GstCaps *caps = gst_caps_from_string(capsStr);
    VERIFY_ELEMENT(caps, failMessage);
    g_object_set(source, "caps", caps, NULL);
    gst_caps_unref(caps);

    GstElement *jitterbuffer = gst_element_factory_make("rtpjitterbuffer", NULL);
    VERIFY_ELEMENT(jitterbuffer, failMessage);

    g_object_set(jitterbuffer,   //
                 "latency", 100, //
                 NULL);

    const char *depayName;
    switch (gCodec)
    {
    case VIDEO_CODEC_H265: depayName = "rtph265depay"; break;
    case VIDEO_CODEC_TS: depayName = "rtpmp2tdepay"; break;
    case VIDEO_CODEC_H264:
    default: depayName = "rtph264depay"; break;
    }

    GstElement *depay = gst_element_factory_make(depayName, NULL);
    VERIFY_ELEMENT(depay, failMessage);

    if (vcatH26xRouterCreate(&gRouter, GST_BIN(gSourceBin)) != RC_OK)
    {
        return RC_ERROR;
    }

    GstElement *queue = vcatCreateLeakyQueue();
    VERIFY_ELEMENT(queue, failMessage);

    gSourceElement = queue;

    gst_bin_add_many(GST_BIN(gSourceBin), //
                     source,              //
                     jitterbuffer,        //
                     depay,               //
                     queue,               //
                     NULL);

    if (!gst_element_link(source, jitterbuffer))
    {
        lfLogError("Failed to link udpsrc -> jitterbuffer");
        return RC_ERROR;
    }

    if (!gst_element_link(jitterbuffer, depay))
    {
        lfLogError("Failed to link jitterbuffer -> depayloader");
        return RC_ERROR;
    }

    if (gCodec == VIDEO_CODEC_TS)
    {
        // The MPEG-TS payload carries its own elementary streams; the demuxer
        // produces dynamic H264/H265 pads which hook into the parsers.
        GstElement *demux = gst_element_factory_make("tsdemux", NULL);
        VERIFY_ELEMENT(demux, failMessage);

        g_object_set(demux,          //
                     "latency", 100, //
                     NULL);

        gst_bin_add(GST_BIN(gSourceBin), demux);

        if (!gst_element_link(depay, demux))
        {
            lfLogError("Failed to link depayloader -> tsdemux");
            return RC_ERROR;
        }

        vcatH26xRouterRoutePads(&gRouter, demux);
    }
    else
    {
        bool forH264 = gCodec == VIDEO_CODEC_H264;
        GstElement *h26xparse = forH264 ? gRouter.h264parse : gRouter.h265parse;

        if (!gst_element_link(depay, h26xparse))
        {
            lfLogError("Failed to link depayloader -> parser");
            return RC_ERROR;
        }

        if (vcatH26xRouterLinkParser(&gRouter, forH264) != RC_OK)
        {
            return RC_ERROR;
        }
    }

    vcatRouteDecodebinPads(gRouter.decoder, queue);
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
 * @return the leaky queue element the host links its render path into
 */
GstElement *videoSourceGetSourceElement()
{
    return gSourceElement;
}

/**
 * @brief Video Source Get GStreamer Source Bin
 *
 * @return the complete source bin
 */
GstElement *videoSourceGetSourceBin()
{
    return gSourceBin;
}
