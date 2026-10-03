
#include "videosrc_udp.hpp"
#include "gst/gstbin.h"
#include "gst/gstelement.h"
#include "gst/gstpad.h"
#include "gst/gstutils.h"
#include "logging.hpp"
#include "video_config.hpp"
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace
{

enum VideoCodec : uint8_t
{
    NONE = 0,
    H264,
    H265
};

#define PTR(P) ((P) == nullptr ? "NULL" : STR((int)(P)))

void linkNewH26xPad(UdpVideoSrcContext *context, GstPad *newPad, VideoCodec codec)
{
    GstElement *h26xparse = codec == VideoCodec::H264 ? context->h264parse : context->h265parse;

    GstPadLinkReturn ret = GST_PAD_LINK_OK;
    GstPad *sinkPad = gst_element_get_static_pad(h26xparse, "sink");

    logging::info("Linking {} -> {}", GST_PAD_NAME(sinkPad), GST_PAD_NAME(newPad));

    if (gst_pad_is_linked(sinkPad))
    {
        logging::warn("Unable to link new pad");
        goto EXIT;
    }

    ret = gst_pad_link(newPad, sinkPad);
    if (GST_PAD_LINK_FAILED(ret))
    {
        logging::error("Failed to link demux -> parser: {}", gst_pad_link_get_name(ret));
        goto EXIT;
    }

    if (!gst_element_link(h26xparse, context->decoder))
    {
        logging::error("Failed to link h26x src");
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

    gchar *caps_str = gst_caps_to_string(caps);
    logging::info("decodebin pad: {}", caps_str);

    GstPadLinkReturn ret = gst_pad_link(newPad, sinkPad);

    if (ret != GST_PAD_LINK_OK)
    {
        logging::error("Failed to link decodebin -> sink: {}", gst_pad_link_get_name(ret));
    }
    else
    {
        logging::info("Linked decodebin -> sink");
    }

    g_free(caps_str);
    gst_caps_unref(caps);
    gst_object_unref(sinkPad);
}

void tsdemuxOnPadAdded(GstElement *_, GstPad *newPad, gpointer userData)
{
    UdpVideoSrcContext *context = (UdpVideoSrcContext *)userData;

    logging::info("tsdemux pad added: {}", GST_PAD_NAME(newPad));

    if (context->linked)
    {
        logging::warn("Unable to link new pad: already linked");
        return;
    }

    GstCaps *caps = gst_pad_get_current_caps(newPad);
    if (!caps)
    {
        caps = gst_pad_query_caps(newPad, NULL);
        if (!caps)
        {
            logging::error("Unable to read pad caps");
            return;
        }
    }

    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *name = gst_structure_get_name(structure);

    logging::info("New pad: {}", name);

    VideoCodec codec = VideoCodec::NONE;

    if (g_str_has_prefix(name, "video/x-h265"))
    {
        codec = VideoCodec::H265;
    }
    if (g_str_has_prefix(name, "video/x-h264"))
    {
        codec = VideoCodec::H264;
    }
    else
    {
        logging::error("Unsupported pad type {}", name);
    }

    if (codec != VideoCodec::NONE)
    {
        linkNewH26xPad(context, newPad, codec);
    }

    gst_caps_unref(caps);
}
} // namespace

UdpVideoSrc::UdpVideoSrc(NetworkSource videoSource)
{
    std::string failMessage = "Unable to initialize UDP video source";

    _srcBin = gst_bin_new("video_src_bin");
    verifyPtr(_srcBin, STR(_srcBin), failMessage);

    GstElement *source = gst_element_factory_make("udpsrc", nullptr);
    verifyPtr(source, STR(source), failMessage);

    GstElement *demux = gst_element_factory_make("tsdemux", nullptr);
    verifyPtr(demux, STR(demux), failMessage);

    GstElement *h265parse = gst_element_factory_make("h265parse", nullptr);
    verifyPtr(h265parse, STR(h265parse), failMessage);

    GstElement *h264parse = gst_element_factory_make("h264parse", nullptr);
    verifyPtr(h264parse, STR(h264parse), failMessage);

    GstElement *decoder = gst_element_factory_make("decodebin", nullptr);
    verifyPtr(decoder, STR(decoder), failMessage);

    GstElement *queue = gst_element_factory_make("queue", nullptr);
    verifyPtr(queue, STR(queue), failMessage);

    _srcElement = queue;

    _srcContext = new UdpVideoSrcContext{};

    _srcContext->decoder = decoder;
    _srcContext->h265parse = h265parse;
    _srcContext->h264parse = h264parse;

    logging::info("UDP video source ip={} port={}", videoSource.ip.toStr(), videoSource.port);

    g_object_set(source,                                    //
                 "port", videoSource.port,                  //
                 "address", videoSource.ip.toStr().c_str(), //
                 "auto-multicast", true,                    //
                 NULL);

    gst_bin_add_many(GST_BIN(_srcBin), //
                     source,           //
                     demux,            //
                     decoder,          //
                     h265parse,        //
                     h264parse,        //
                     queue,            //
                     NULL);

    if (!gst_element_link(source, demux))
    {
        throw std::runtime_error(std::format("{}: unable to link source -> demux", failMessage));
    }

    g_signal_connect(decoder, "pad-added", G_CALLBACK(decodeBinPadAdded), queue);
    g_signal_connect(demux, "pad-added", G_CALLBACK(tsdemuxOnPadAdded), _srcContext);
}
