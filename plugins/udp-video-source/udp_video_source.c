#include "common_types.h"
#include "gst/gstbin.h"
#include "gst/gstelement.h"
#include "gst/gstpad.h"
#include "gst/gstutils.h"
#include "logging_facade.h"
#include "vcat_plugin_types.h"
#include <gst/gstelement.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

typedef enum
{
    VIDEO_CODEC_NONE = 0,
    VIDEO_CODEC_H264,
    VIDEO_CODEC_H265
} VideoCodec;

typedef struct
{
    bool linked;
    GstElement *h264parse;
    GstElement *h265parse;
    GstElement *decoder;
} UdpVideoSrcContext;

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
} NetworkSource;

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;
static UdpVideoSrcContext *context = NULL;
static NetworkSource networkSource = {.ip = {127, 0, 0, 1}, .port = 1000};
static bool inputArgsPresent = false;

VCAT_DEFINE_PLUGIN_NAME("UDP Video Source")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)

void linkNewH26xPad(UdpVideoSrcContext *context, GstPad *newPad, VideoCodec codec)
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

void decodeBinPadAdded(GstElement *_, GstPad *newPad, gpointer userData)
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
    UdpVideoSrcContext *context = (UdpVideoSrcContext *)userData;

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
    if (g_str_has_prefix(name, "video/x-h264"))
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

/**
 * @brief process input args
 *
 * @param[in] argc argc
 * @param[in] argv argv
 *
 * @return RC_OK if successful
 */
Rc videoSourceProcessInputArgs(int argc, char *argv[])
{
    Ip *ip = &networkSource.ip;
    int *port = &networkSource.port;

    for (int i = 1; i < argc; i++)
    {
        if (sscanf(argv[i], "udp://%hhu.%hhu.%hhu.%hhu:%d", &ip->o3, &ip->o2, &ip->o1, &ip->o0, port))
        {
            inputArgsPresent = true;
        }
        else if (sscanf(argv[i], "udp://localhost:%d", port))
        {
            *ip = (Ip){127, 0, 0, 1};
            inputArgsPresent = true;
        }
    }

    return RC_OK;
}

/**
 * @brief Check if the current plugin can initialize. Usually this is
 * is dependent on whether the correct input args were provided.
 *
 * @return true if the plugin can initialize
 */
bool videoSourceCanInitialize()
{
    return inputArgsPresent;
}

/**
 * @brief Video Source Initialize
 *
 * @return RC_OK if successful
 */
Rc videoSourceInitialize()
{
    char *failMessage = "Unable to initialize UDP video source";

    gSourceBin = gst_bin_new("video_src_bin");
    VERIFY_ELEMENT(gSourceBin, failMessage);

    GstElement *source = gst_element_factory_make("udpsrc", NULL);
    VERIFY_ELEMENT(source, failMessage);

    GstElement *demux = gst_element_factory_make("tsdemux", NULL);
    VERIFY_ELEMENT(demux, failMessage);

    GstElement *h265parse = gst_element_factory_make("h265parse", NULL);
    VERIFY_ELEMENT(h265parse, failMessage);

    GstElement *h264parse = gst_element_factory_make("h264parse", NULL);
    VERIFY_ELEMENT(h264parse, failMessage);

    GstElement *decoder = gst_element_factory_make("decodebin", NULL);
    VERIFY_ELEMENT(decoder, failMessage);

    GstElement *queue = gst_element_factory_make("queue", NULL);
    VERIFY_ELEMENT(queue, failMessage);

    gSourceElement = queue;

    context = (UdpVideoSrcContext *)calloc(1, sizeof(UdpVideoSrcContext));

    context->decoder = decoder;
    context->h265parse = h265parse;
    context->h264parse = h264parse;

    char ipStr[17];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", networkSource.ip.o3, networkSource.ip.o2, networkSource.ip.o1,
             networkSource.ip.o0);

    lfLogInfo("UDP video source ip=%s port=%d", ipStr, networkSource.port);

    g_object_set(source,                     //
                 "port", networkSource.port, //
                 "address", ipStr,           //
                 "auto-multicast", true,     //
                 NULL);

    gst_bin_add_many(GST_BIN(gSourceBin), //
                     source,              //
                     demux,               //
                     decoder,             //
                     h265parse,           //
                     h264parse,           //
                     queue,               //
                     NULL);

    if (!gst_element_link(source, demux))
    {
        return RC_ERROR;
    }

    g_signal_connect(decoder, "pad-added", G_CALLBACK(decodeBinPadAdded), queue);
    g_signal_connect(demux, "pad-added", G_CALLBACK(tsdemuxOnPadAdded), context);
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
