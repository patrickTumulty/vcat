#include "crox_types.h"
#include "gst/gstelement.h"
#include "gst/gstutils.h"
#include "ipv4_address.h"
#include "logging_facade.h"
#include "vcat_network_endpoint.h"
#include "vcat_plugin_framework.h"
#include "vcat_video_source_utils.h"

#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;
static VcatH26xRouter gRouter = {0};
static VcatNetworkEndpoint gNetworkSource = {
    .ip = ipv4_localhost,
    .port = 1000,
};

VCAT_DEFINE_PLUGIN_NAME("UDP MPEG transport stream")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)
VCAT_DEFINE_PLUGIN_VERSION(VCAT_VERSION)
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_URI_HELP("udp://<host>:<port>")
VCAT_DEFINE_VIDEO_SOURCE_PLUGIN_DESCRIPTION("Receive an MPEG-TS stream over UDP")

static void videoSourcePrintHelp()
{
    printf("udp://<host>:<port> - Receive an MPEG-TS stream over UDP\n"
           "\n"
           "Usage: vcat udp://<host>:<port> [OPTIONS]\n"
           "\n"
           "The URI host accepts an IPv4 address (udp://127.0.0.1:1234)\n"
           "or 'localhost' (udp://localhost:1234).\n"
           "\n"
           "Options:\n"
           "  -h, --help      Show this help\n"
           "  -v, --version   Show version information\n");
}

/**
 * @brief process the source's command line, e.g. "vcat udp://1.2.3.4:5 [OPTIONS]"
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
        {"version", no_argument, NULL, 'v'},
        {"help",    no_argument, NULL, 'h'},
        {NULL,      0,           NULL, 0  }  //
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "+hv", cliOptions, NULL)) != -1)
    {
        switch (opt)
        {
        case 'h':
            videoSourcePrintHelp();
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case 'v':
            printf("udp v%s\n", VCAT_VERSION);
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        default:
            fprintf(stderr, "Unknown option. See 'vcat udp --help'.\n");
            return ARG_PARSE_RESULT_ERROR;
        }
    }

    // Parse the source URI, e.g. "udp://127.0.0.1:1234" or "udp://localhost:1234".
    // On failure the endpoint is left untouched and keeps its defaults.
    if (vcatNetworkEndpointParseUri(&gNetworkSource, argv[0], "udp"))
    {
        return ARG_PARSE_RESULT_SUCCESS;
    }

    fprintf(stderr, "Invalid source URI '%s'. See 'vcat udp --help'.\n", argv[0]);
    return ARG_PARSE_RESULT_ERROR;
}

/**
 * @brief Video Source Initialize
 *
 * @return RC_OK if successful
 */
Rc videoSourceInitialize()
{
    char *failMessage = "Unable to initialize UDP video source";

    gSourceBin = gst_bin_new(VCAT_VIDEO_SOURCE_BIN_NAME);
    VERIFY_ELEMENT(gSourceBin, failMessage);

    GstElement *source = gst_element_factory_make("udpsrc", NULL);
    VERIFY_ELEMENT(source, failMessage);

    char ipStr[16];
    ipv4_address_to_string(gNetworkSource.ip, ipStr, sizeof(ipStr));
    lfLogInfo("UDP video source ip=%s port=%d", ipStr, gNetworkSource.port);

    g_object_set(source,                         //
                 "port", gNetworkSource.port,    //
                 "address", ipStr,               //
                 "auto-multicast", true,         //
                 "buffer-size", 2 * 1024 * 1024, // 2 MiB
                 NULL);

    GstElement *demux = gst_element_factory_make("tsdemux", NULL);
    VERIFY_ELEMENT(demux, failMessage);

    g_object_set(demux,          //
                 "latency", 100, //
                 NULL);

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
                     demux,               //
                     queue,               //
                     NULL);

    if (!gst_element_link(source, demux))
    {
        lfLogError("Failed to link udpsrc -> tsdemux");
        return RC_ERROR;
    }

    vcatH26xRouterRoutePads(&gRouter, demux, queue);

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
