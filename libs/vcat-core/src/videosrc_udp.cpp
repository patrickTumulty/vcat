
#include "videosrc_udp.hpp"
#include "gst/gstbin.h"
#include "gst/gstelement.h"
#include "gst/gstpad.h"
#include "gst/gstutils.h"
#include "spdlog/spdlog.h"
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

void linkNewH26xPad(UdpVideoSrcContext *context, GstPad *newPad, VideoCodec codec)
{
    GstElement *h26xparse = codec == VideoCodec::H264 ? context->h264parse : context->h265parse;

    GstPad *sinkPad = gst_element_get_static_pad(h26xparse, "sink");
    if (gst_pad_is_linked(sinkPad))
    {
        spdlog::warn("Unable to link new pad");
        gst_object_unref(sinkPad);
        return;
    }

    GstPadLinkReturn ret = gst_pad_link(newPad, sinkPad);
    if (GST_PAD_LINK_FAILED(ret))
    {
        spdlog::error("Failed to link demux -> parser: {}", gst_pad_link_get_name(ret));
    }

    if (!gst_element_link(h26xparse, context->decoder))
    {
        spdlog::error("Failed to link h26x src");
    }

    context->linked = true;

    gst_object_unref(sinkPad);
}

void decodeBinPadAdded(GstElement *_, GstPad *newPad, gpointer userData)
{
    GstElement *sink = GST_ELEMENT(userData);

    GstPad *sink_pad = gst_element_get_static_pad(sink, "sink");

    if (gst_pad_is_linked(sink_pad))
    {
        gst_object_unref(sink_pad);
        return;
    }

    GstCaps *caps = gst_pad_get_current_caps(newPad);
    if (!caps)
        caps = gst_pad_query_caps(newPad, NULL);

    gchar *caps_str = gst_caps_to_string(caps);
    spdlog::info("decodebin pad: {}", caps_str);

    GstPadLinkReturn ret = gst_pad_link(newPad, sink_pad);

    if (ret != GST_PAD_LINK_OK)
    {
        spdlog::error("Failed to link decodebin -> sink: {}", gst_pad_link_get_name(ret));
    }
    else
    {
        spdlog::info("Linked decodebin -> sink");
    }

    g_free(caps_str);
    gst_caps_unref(caps);
    gst_object_unref(sink_pad);
}

void tsdemuxOnPadAdded(GstElement *_, GstPad *newPad, gpointer userData)
{
    UdpVideoSrcContext *context = (UdpVideoSrcContext *)userData;

    if (context->linked)
    {
        spdlog::warn("Unable to link new pad: already linked");
        return;
    }

    GstCaps *caps = gst_pad_get_current_caps(newPad);

    if (!caps)
    {
        caps = gst_pad_query_caps(newPad, NULL);
    }

    if (!caps)
    {
        spdlog::error("Unable to read pad caps");
        return;
    }

    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *name = gst_structure_get_name(structure);

    spdlog::info("New pad: {}", name);

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
        spdlog::error("Unsupported pad type {}", name);
    }

    if (codec != VideoCodec::NONE)
    {
        linkNewH26xPad(context, newPad, codec);
    }

    gst_caps_unref(caps);
}
} // namespace

UdpVideoSrc::UdpVideoSrc(Ip ip, int port)
{
    std::string failMessage = "Unable to initialize UDP video source";

    _srcBin = gst_bin_new("video_src_bin");
    verifyElement(_srcBin, STR(_srcBin), failMessage);

    GstElement *source = gst_element_factory_make("udpsrc", nullptr);
    verifyElement(source, STR(source), failMessage);

    GstElement *demux = gst_element_factory_make("tsdemux", nullptr);
    verifyElement(demux, STR(demux), failMessage);

    GstElement *h265parse = gst_element_factory_make("h265parse", nullptr);
    verifyElement(h265parse, STR(h265parse), failMessage);

    GstElement *h264parse = gst_element_factory_make("h264parse", nullptr);
    verifyElement(h264parse, STR(h265parse), failMessage);

    GstElement *decoder = gst_element_factory_make("decodebin", nullptr);
    verifyElement(decoder, STR(decoder), failMessage);

    GstElement *videoconvert = gst_element_factory_make("videoconvert", nullptr);
    verifyElement(videoconvert, STR(videoconvert), failMessage);

    GstElement *capsfilter = gst_element_factory_make("capsfilter", nullptr);
    verifyElement(capsfilter, STR(capsfilter), failMessage);

    _srcElement = capsfilter;

    _srcContext.decoder = decoder;
    _srcContext.h265parse = h265parse;
    _srcContext.h264parse = h264parse;

    g_object_set(source,                        //
                 "port", port,                  //
                 "address", ip.toStr().c_str(), //
                 "auto-multicast", true,        //
                 NULL);

    GstCaps *caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGB", NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(_srcBin), //
                     source,           //
                     demux,            //
                     decoder,          //
                     h265parse,        //
                     h264parse,        //
                     videoconvert,     //
                     capsfilter,       //
                     NULL);

    if (!gst_element_link(source, demux))
    {
        throw std::runtime_error(std::format("{}: unable to link source -> demux", failMessage));
    }

    if (!gst_element_link_many(videoconvert, //
                               capsfilter,   //
                               NULL))
    {
        throw std::runtime_error(std::format("{}: Failed to link decoder -> converter -> sink", failMessage));
    }

    g_signal_connect(decoder, "pad-added", G_CALLBACK(decodeBinPadAdded), videoconvert);
    g_signal_connect(demux, "pad-added", G_CALLBACK(tsdemuxOnPadAdded), &_srcContext);
}
