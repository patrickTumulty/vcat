
#include "vcat_video_source_utils.h"

#include "logging_facade.h"
#include "vcat_plugin_framework.h"

#include <gst/gstcaps.h>
#include <gst/gstpad.h>
#include <gst/gstutils.h>
#include <string.h>

GstElement *vcatCreateLeakyQueue(void)
{
    GstElement *queue = gst_element_factory_make("queue", NULL);
    if (!queue)
    {
        lfLogError("GST Element queue is NULL");
        return NULL;
    }

    g_object_set(queue,                 //
                 "max-size-buffers", 1, //
                 "max-size-bytes", 0,   //
                 "max-size-time", 0,    //
                 "leaky", 2,            // Downstream Leaky
                 NULL);

    return queue;
}

static void decodebinPadAdded(GstElement *_, GstPad *newPad, gpointer userData)
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

void vcatRouteDecodebinPads(GstElement *decodebin, GstElement *sink)
{
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(decodebinPadAdded), sink);
}

static void linkPadToH26xParser(VcatH26xRouter *router, GstPad *newPad, bool forH264)
{
    GstElement *h26xparse = forH264 ? router->h264parse : router->h265parse;

    GstPad *sinkPad = gst_element_get_static_pad(h26xparse, "sink");

    lfLogInfo("Linking %s -> %s", GST_PAD_NAME(sinkPad), GST_PAD_NAME(newPad));

    if (gst_pad_is_linked(sinkPad))
    {
        lfLogWarn("Unable to link new pad");
        goto EXIT;
    }

    GstPadLinkReturn ret = gst_pad_link(newPad, sinkPad);
    if (GST_PAD_LINK_FAILED(ret))
    {
        lfLogError("Failed to link demux -> parser: %s", gst_pad_link_get_name(ret));
        goto EXIT;
    }

    if (vcatH26xRouterLinkParser(router, forH264) != RC_OK)
    {
        goto EXIT;
    }

    router->linked = true;

EXIT:

    gst_object_unref(sinkPad);
}

static void h26xRouterPadAdded(GstElement *_, GstPad *newPad, gpointer userData)
{
    VcatH26xRouter *router = (VcatH26xRouter *)userData;

    lfLogInfo("Demuxer pad added: %s", GST_PAD_NAME(newPad));

    if (router->linked)
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

    if (g_str_has_prefix(name, "video/x-h265"))
    {
        linkPadToH26xParser(router, newPad, false);
    }
    else if (g_str_has_prefix(name, "video/x-h264"))
    {
        linkPadToH26xParser(router, newPad, true);
    }
    else
    {
        lfLogError("Unsupported pad type %s", name);
    }

    gst_caps_unref(caps);
}

void vcatH26xRouterRoutePads(VcatH26xRouter *router, GstElement *demuxer)
{
    g_signal_connect(demuxer, "pad-added", G_CALLBACK(h26xRouterPadAdded), router);
}

Rc vcatH26xRouterCreate(VcatH26xRouter *router, GstBin *bin)
{
    char *failMessage = "Unable to configure H.264/H.265 parser router";

    memset(router, 0, sizeof(*router));

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

    router->h264parse = h264parse;
    router->h265parse = h265parse;
    router->decoder = decoder;

    gst_bin_add_many(bin, h265parse, h264parse, decoder, NULL);

    return RC_OK;
}

Rc vcatH26xRouterLinkParser(VcatH26xRouter *router, bool forH264)
{
    if (router->linked)
    {
        return RC_OK; // The decoder already has its parser.
    }

    GstElement *h26xparse = forH264 ? router->h264parse : router->h265parse;

    if (!gst_element_link(h26xparse, router->decoder))
    {
        lfLogError("Failed to link %s parser -> decoder", forH264 ? "H264" : "H265");
        return RC_ERROR;
    }

    return RC_OK;
}
