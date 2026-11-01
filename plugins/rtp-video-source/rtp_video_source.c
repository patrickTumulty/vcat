#include "crox_types.h"
#include "gst/gstcaps.h"
#include "gst/gstelement.h"
#include "gst/gstutils.h"
#include "ipv4_address.h"
#include "logging_facade.h"
#include "vcat_network_endpoint.h"
#include "vcat_plugin_framework.h"
#include "vcat_video_source_utils.h"

#include <getopt.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;
static VcatH26xRouter gRouter = {0};
static bool gMpegTs = false;
static VcatNetworkEndpoint gNetworkSource = {
    .ip = ipv4_localhost,
    .port = 5004,
};
static VideoCodec gCodec = VIDEO_CODEC_H265;

VCAT_DEFINE_PLUGIN_NAME("RTP")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)
VCAT_DEFINE_PLUGIN_VERSION(VCAT_VERSION)
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("rtp://<host>:<port>")
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION("Receive an RTP video stream over UDP")

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
           "  -c, --codec <type>    RTP payload encoding: h265 (default), h264\n"
           "      --mpegts          Specify RTP MPEGTS stream (codec determined from ts stream)\n");
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
    int mpegtsFlag = 0;
    const struct option cliOptions[] = {
        {"version", no_argument,       NULL,        'v'},
        {"help",    no_argument,       NULL,        'h'},
        {"codec",   required_argument, NULL,        'c'},
        {"mpegts",  no_argument,       &mpegtsFlag, 1  },
        {NULL,      0,                 NULL,        0  }  //
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
            printf("rtp v%s\n", vcatPluginGetVersion());
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case 'c': {
            const char *codecArg = optarg;
            if (strcasecmp(codecArg, "h264") == 0)
            {
                gCodec = VIDEO_CODEC_H264;
            }
            else if (strcasecmp(codecArg, "h265") == 0)
            {
                gCodec = VIDEO_CODEC_H265;
            }
            else
            {
                fprintf(stderr, "Unknown codec '%s'. See 'vcat rtp --help'.\n", codecArg);
                return ARG_PARSE_RESULT_ERROR;
            }
            break;
        }
        case 0: // A long option with its own flag pointer (e.g. --mpegts) was set.
            break;
        default:
            fprintf(stderr, "Unknown option. See 'vcat rtp --help'.\n");
            return ARG_PARSE_RESULT_ERROR;
        }
    }

    gMpegTs = mpegtsFlag != 0;

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

    gSourceBin = gst_bin_new(VCAT_VIDEO_SOURCE_BIN_NAME);
    VERIFY_ELEMENT(gSourceBin, failMessage);

    GstElement *source = gst_element_factory_make("udpsrc", NULL);
    VERIFY_ELEMENT(source, failMessage);

    char ipStr[16];
    ipv4_address_to_string(gNetworkSource.ip, ipStr, sizeof(ipStr));

    lfLogInfo("RTP video source ip=%s port=%d codec=%s", ipStr, gNetworkSource.port, videoCodecToString(gCodec));

    g_object_set(source,                         //
                 "port", gNetworkSource.port,    //
                 "address", ipStr,               //
                 "auto-multicast", true,         //
                 "buffer-size", 2 * 1024 * 1024, // 2 MiB
                 NULL);

    const char *capsStr;
    const char *depayName;
    if (gMpegTs)
    {
        depayName = "rtpmp2tdepay";
        capsStr = "application/x-rtp, media=(string)video, payload=(int)33, clock-rate=(int)90000, "
                  "encoding-name=(string)MP2T";
    }
    else
    {
        switch (gCodec)
        {
        case VIDEO_CODEC_H264:
            depayName = "rtph264depay";
            capsStr = "application/x-rtp, media=(string)video, payload=(int)96, clock-rate=(int)90000, "
                      "encoding-name=(string)H264";
            break;
        case VIDEO_CODEC_H265:
        default:
            capsStr = "application/x-rtp, media=(string)video, payload=(int)96, clock-rate=(int)90000, "
                      "encoding-name=(string)H265";
            depayName = "rtph265depay";
            break;
        }
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

    GstElement *depay = gst_element_factory_make(depayName, NULL);
    VERIFY_ELEMENT(depay, failMessage);

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

    if (gMpegTs)
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

        vcatH26xRouterRoutePads(&gRouter, demux, queue);
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

        GstElement *tail = NULL;
        if (vcatH26xRouterPrepareCodecChain(&gRouter, gCodec, &tail) != RC_OK)
        {
            return RC_ERROR;
        }

        if (!gst_element_link(tail, queue))
        {
            lfLogError("Failed to link decode chain tail -> queue");
            return RC_ERROR;
        }
    }

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
