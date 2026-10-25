#include "crox_types.h"
#include "gst/gstbin.h"
#include "gst/gstcaps.h"
#include "gst/gstelement.h"

#include "gst/gstpad.h"
#include "gst/gstutils.h"
#include "logging_facade.h"
#include "vcat_plugin_framework.h"
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

typedef enum
{
    VIDEO_CODEC_NONE = 0,
    VIDEO_CODEC_H264,
    VIDEO_CODEC_H265,
    VIDEO_CODEC_TS
} VideoCodec;

typedef struct
{
    bool linked;
    GstElement *h264parse;
    GstElement *h265parse;
    GstElement *decoder;
} RtpVideoSrcContext;

typedef struct
{
    union {
        struct
        {
            uint8_t o3;
            uint8_t o2;
            uint8_t o1;
            uint8_t o0;
        };
        uint32_t ip;
    };
} Ip;

typedef struct
{
    Ip ip;
    int port;
    VideoCodec codec;
} NetworkSource;

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;
static RtpVideoSrcContext *context = NULL;
static NetworkSource networkSource = {
    .ip = {127, 0, 0, 1},   //
    .port = 5004,           //
    .codec = VIDEO_CODEC_H264
};

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

static void linkNewH26xPad(RtpVideoSrcContext *context, GstPad *newPad, VideoCodec codec)
{
    GstElement *h26xparse = codec == VIDEO_CODEC_H264 ? context->h264parse : context->h265parse;

    GstPadLinkReturn ret = GST_PAD_LINK_OK;
    GstPad *sinkPad = gst_element_get_static_pad(h26xparse, "sink");

    lfLogInfo("Linking %s -> %s", GST_PAD_NAME(sinkPad), GST_PAD_NAME(newPad));

    if (gst_pad_is_linked(sinkPad))
    {
        lfLogWarn("Unable to link new pad");
        goto EXIT;
    }

    ret = gst_pad_link(newPad, sinkPad);
    if (GST_PAD_LINK_FAILED(ret))
    {
        lfLogError("Failed to link demux -> parser: %s", gst_pad_link_get_name(ret));
        goto EXIT;
    }

    if (!gst_element_link(h26xparse, context->decoder))
    {
        lfLogError("Failed to link h26x src");
        goto EXIT;
    }

    context->linked = true;

EXIT:

    gst_object_unref(sinkPad);
}

static void decodeBinPadAdded(GstElement *_, GstPad *newPad, gpointer userData)
{
    GstElement *sink = GST_ELEMENT(userData);

    GstPad *sinkPad = gst_element_get_static_pad(sink, "sink");

    if (gst_pad_is_linked(sinkPad))
    {
        gst_object_unref(sinkPad);
        return;
    }

    GstCaps *caps = gst_pad_get_current_caps(newPad);
    if (!caps)
        caps = gst_pad_query_caps(newPad, NULL);

    gchar *capsStr = gst_caps_to_string(caps);
    lfLogInfo("decodebin pad: %s", capsStr);

    GstPadLinkReturn ret = gst_pad_link(newPad, sinkPad);

    if (ret != GST_PAD_LINK_OK)
    {
        lfLogError("Failed to link decodebin -> sink: %s", gst_pad_link_get_name(ret));
    }
    else
    {
        lfLogInfo("Linked decodebin -> sink");
    }

    g_free(capsStr);
    gst_caps_unref(caps);
    gst_object_unref(sinkPad);
}

void tsdemuxOnPadAdded(GstElement *_, GstPad *newPad, gpointer userData)
{
    RtpVideoSrcContext *context = (RtpVideoSrcContext *)userData;

    lfLogInfo("tsdemux pad added: %s", GST_PAD_NAME(newPad));

    if (context->linked)
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
    const gchar *name = gst_structure_get_name(structure);

    lfLogInfo("New pad: %s", name);

    VideoCodec codec = VIDEO_CODEC_NONE;

    if (g_str_has_prefix(name, "video/x-h265"))
    {
        codec = VIDEO_CODEC_H265;
    }
    else if (g_str_has_prefix(name, "video/x-h264"))
    {
        codec = VIDEO_CODEC_H264;
    }
    else
    {
        lfLogError("Unsupported pad type %s", name);
    }

    if (codec != VIDEO_CODEC_NONE)
    {
        linkNewH26xPad(context, newPad, codec);
    }

    gst_caps_unref(caps);
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
        {NULL,      0,                 NULL, 0  }  //
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
                networkSource.codec = VIDEO_CODEC_H264;
            }
            else if (strcasecmp(codecArg, "h265") == 0)
            {
                networkSource.codec = VIDEO_CODEC_H265;
            }
            else if (strcasecmp(codecArg, "ts") == 0)
            {
                networkSource.codec = VIDEO_CODEC_TS;
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

    // Parse the source URI, e.g. "rtp://127.0.0.1:5004" or "rtp://localhost:5004"
    Ip *ip = &networkSource.ip;
    int *port = &networkSource.port;
    uint32_t initialIp = ip->ip;
    int initialPort = *port;

    if (sscanf(argv[0], "rtp://%hhu.%hhu.%hhu.%hhu:%d", &ip->o3, &ip->o2, &ip->o1, &ip->o0, port) == 5 && *port > 0)
    {
        return ARG_PARSE_RESULT_SUCCESS;
    }

    if (sscanf(argv[0], "rtp://localhost:%d", port) == 1 && *port > 0)
    {
        *ip = (Ip){127, 0, 0, 1};
        return ARG_PARSE_RESULT_SUCCESS;
    }

    // Undo any partially parsed values before failing.
    ip->ip = initialIp;
    *port = initialPort;
    fprintf(stderr, "Invalid source URI '%s'. See 'vcat rtp --help'.\n", argv[0]);
    return ARG_PARSE_RESULT_ERROR;
}

/**
 * @brief Video Source Initialize
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

    char ipStr[17];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", networkSource.ip.o3, networkSource.ip.o2, networkSource.ip.o1,
             networkSource.ip.o0);

    lfLogInfo("RTP video source ip=%s port=%d codec=%s", ipStr, networkSource.port,
              videoCodecToString(networkSource.codec));

    g_object_set(source,                         //
                 "port", networkSource.port,     //
                 "address", ipStr,               //
                 "auto-multicast", true,         //
                 "buffer-size", 2 * 1024 * 1024, // 2 MiB
                 NULL);

    // The RTP payload caps tell the jitterbuffer the stream's encoding and
    // clock rate; the depayloader downstream is chosen to match.
    const char *capsStr;
    switch (networkSource.codec)
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
    switch (networkSource.codec)
    {
    case VIDEO_CODEC_H265: depayName = "rtph265depay"; break;
    case VIDEO_CODEC_TS: depayName = "rtpmp2tdepay"; break;
    case VIDEO_CODEC_H264:
    default: depayName = "rtph264depay"; break;
    }

    GstElement *depay = gst_element_factory_make(depayName, NULL);
    VERIFY_ELEMENT(depay, failMessage);

    GstElement *demux = NULL;

    if (networkSource.codec == VIDEO_CODEC_TS)
    {
        demux = gst_element_factory_make("tsdemux", NULL);
        VERIFY_ELEMENT(demux, failMessage);

        g_object_set(demux,          //
                     "latency", 100, //
                     NULL);
    }

    GstElement *h265parse = gst_element_factory_make("h265parse", NULL);
    VERIFY_ELEMENT(h265parse, failMessage);

    g_object_set(h265parse,            //
                 "config-interval", 1, //
                 NULL);

    GstElement *h264parse = gst_element_factory_make("h264parse", NULL);
    VERIFY_ELEMENT(h264parse, failMessage);

    g_object_set(h264parse,            //
                 "config-interval", 1, //
                 NULL);

    GstElement *decoder = gst_element_factory_make("decodebin", NULL);
    VERIFY_ELEMENT(decoder, failMessage);

    GstElement *queue = gst_element_factory_make("queue", NULL);
    VERIFY_ELEMENT(queue, failMessage);

    g_object_set(queue,                 //
                 "max-size-buffers", 1, //
                 "max-size-bytes", 0,   //
                 "max-size-time", 0,    //
                 "leaky", 2,            // Downstream Leaky
                 NULL);

    gSourceElement = queue;

    context = (RtpVideoSrcContext *)calloc(1, sizeof(RtpVideoSrcContext));

    context->decoder = decoder;
    context->h265parse = h265parse;
    context->h264parse = h264parse;

    gst_bin_add_many(GST_BIN(gSourceBin), //
                     source,              //
                     jitterbuffer,        //
                     depay,               //
                     h265parse,           //
                     h264parse,           //
                     decoder,             //
                     queue,               //
                     NULL);

    if (demux)
    {
        // The MPEG-TS payload carries its own elementary streams; the demuxer
        // produces dynamic H264/H265 pads which hook into the parsers.
        gst_bin_add(GST_BIN(gSourceBin), demux);

        if (!gst_element_link(source, jitterbuffer) ||
            !gst_element_link(jitterbuffer, depay) ||
            !gst_element_link(depay, demux))
        {
            return RC_ERROR;
        }

        g_signal_connect(demux, "pad-added", G_CALLBACK(tsdemuxOnPadAdded), context);
    }
    else
    {
        GstElement *h26xparse =
            networkSource.codec == VIDEO_CODEC_H264 ? context->h264parse : context->h265parse;

        if (!gst_element_link(source, jitterbuffer))
        {
            lfLogError("Failed to link udpsrc -> jitterbuffer");
            return RC_ERROR;
        }
        if (!gst_element_link(jitterbuffer, depay))
        {
            lfLogError("Failed to link jitterbuffer -> depay");
            return RC_ERROR;
        }
        if (!gst_element_link(depay, h26xparse))
        {
            lfLogError("Failed to link depay -> h26xparse");
            return RC_ERROR;
        }
        if (!gst_element_link(h26xparse, decoder))
        {
            lfLogError("Failed to link h26xparse -> decoder");
            return RC_ERROR;
        }
    }

    g_signal_connect(decoder, "pad-added", G_CALLBACK(decodeBinPadAdded), queue);
    return RC_OK;
}

/**
 * @brief Video Source Cleanup
 *
 * @return RC_OK if successful
 */
Rc videoSourceCleanup()
{
    free(context);
    context = NULL;
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
