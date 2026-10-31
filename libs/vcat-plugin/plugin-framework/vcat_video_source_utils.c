
#include "vcat_video_source_utils.h"

#include "crox_types.h"
#include "logging_facade.h"
#include "vcat_plugin_framework.h"

#include <gst/gstcaps.h>
#include <gst/gstelement.h>
#include <gst/gstpad.h>
#include <gst/gstutils.h>
#include <string.h>

// A known list of decoder candidates per codec, hardware first, CPU last.
// Every candidate resolves at runtime via GStreamer element factories: one
// binary serves all platforms, no vendor SDK is involved in the build.
//
// Entries whose decoder can only emit device-memory frames (e.g. Jetson's NVMM
// decoders) name their matching download converter; NULL means the CPU
// conversion tail converts whatever the decoder produces.
typedef struct
{
    VideoCodec codec;
    const char *decoder;
    const char *converter; // NULL: convert with the generic tail.
} DecodeCandidate;

static const DecodeCandidate kDecodeCandidates[] = {
    // NVIDIA Jetson's hardware decoders access hardware directly.
    {VIDEO_CODEC_H264, "nvv4l2decoder", "nvvideoconvert"},
    {VIDEO_CODEC_H265, "nvv4l2decoder", "nvvideoconvert"},
    {VIDEO_CODEC_H264, "nvv4l2decoder", "nvvidconv"     }, // JetPack 4 BSP name.
    {VIDEO_CODEC_H265, "nvv4l2decoder", "nvvidconv"     }, // JetPack 4 BSP name.

    // VA-API hardware decode: "va" plugin (>= 1.22) first, legacy "vaapi".
    {VIDEO_CODEC_H264, "vah264dec",     NULL            },
    {VIDEO_CODEC_H265, "vah265dec",     NULL            },
    {VIDEO_CODEC_H264, "vaapih264dec",  NULL            },
    {VIDEO_CODEC_H265, "vaapih265dec",  NULL            },

    // NVIDIA desktop GPUs via the gst "nvcodec" plugin.
    {VIDEO_CODEC_H264, "nvh264dec",     NULL            },
    {VIDEO_CODEC_H265, "nvh265dec",     NULL            },

    // FFmpeg software decode: present in any stock GStreamer install.
    {VIDEO_CODEC_H264, "avdec_h264",    NULL            },
    {VIDEO_CODEC_H265, "avdec_h265",    NULL            },
};
static const int kDecodeCandidatesLen = sizeof(kDecodeCandidates) / sizeof(kDecodeCandidates[0]);

// Converter candidates for the tail, vendor converters first. Each must be
// able to output plain system-memory RGBA.
static const char *const kConvertToRgbaConverters[] = {
    "nvvideoconvert", // NVIDIA Jetson (JetPack 5+): downloads NVMM and converts.
    "nvvidconv",      // NVIDIA Jetson (JetPack 4 BSP name).
    "v4l2convert",    // V4L2 stateless hardware converter.
    "videoconvert",   // CPU, never missing in an installed GStreamer.
};
static const int kConvertToRgbaConvertersLen = sizeof(kConvertToRgbaConverters) / sizeof(kConvertToRgbaConverters[0]);

/**
 * @brief bring an element's state that was added to a bin into the bin's state
 *
 * Chains built lazily (while a stream is already running) must be brought up
 * to the parent's state before data flows through them. Chains built during
 * plugin initialization sit in NULL state with their bin, which the pipeline's
 * first PLAYING transition then raises, so there is nothing to do.
 */
static void syncStateWithPlayingParent(GstElement *element)
{
    GstElement *parent = GST_ELEMENT_CAST(gst_element_get_parent(element));
    if (parent == NULL)
    {
        return;
    }

    GstState parentState = GST_STATE_NULL;
    gst_element_get_state(parent, &parentState, NULL, 0);
    gst_object_unref(parent);

    if (parentState >= GST_STATE_PAUSED)
    {
        gst_element_sync_state_with_parent(element);
    }
}

/**
 * @brief appends a converter -> capsfilter(`video/x-raw,format=RGBA`) tail
 * after or behind @p head
 *
 * @param[in,out] chain filled: @p head when given (must already be in @p bin),
 *                      otherwise the converter itself.
 * @param[in] preferredConverter when set, exactly this converter is required;
 *                               when NULL, the generic candidate list is
 *                               probed in order.
 *
 * @return RC_OK and fills @p chain, RC_ERROR when no converter is available
 */
static Rc appendRgbaTail(VcatRgbaChain *chain, GstBin *bin, GstElement *head, const char *preferredConverter)
{
    char *failMessage = "Unable to build RGBA converter tail";

    GstElement *converter = NULL;
    const char *converterName = NULL;

    if (preferredConverter != NULL)
    {
        converter = gst_element_factory_make(preferredConverter, NULL);
        if (converter == NULL)
        {
            lfLogWarn("Paired converter '%s' is missing", preferredConverter);
            return RC_ERROR;
        }
        converterName = preferredConverter;
    }
    else
    {
        for (size_t i = 0; i < kConvertToRgbaConvertersLen; i++)
        {
            converter = gst_element_factory_make(kConvertToRgbaConverters[i], NULL);
            if (converter != NULL)
            {
                converterName = kConvertToRgbaConverters[i];
                break;
            }
        }
    }

    if (converter == NULL)
    {
        lfLogError("%s: no video converter element available", failMessage);
        return RC_ERROR;
    }

    GstElement *capsfilter = gst_element_factory_make("capsfilter", NULL);
    if (capsfilter == NULL)
    {
        lfLogError("%s: GST Element 'capsfilter' is NULL", failMessage);
        gst_object_unref(converter);
        return RC_ERROR;
    }

    GstCaps *caps = gst_caps_new_simple("video/x-raw",                   //
                                        "format", G_TYPE_STRING, "RGBA", //
                                        NULL);                           //
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_bin_add(bin, converter);
    gst_bin_add(bin, capsfilter);

    if (!gst_element_link(converter, capsfilter))
    {
        lfLogError("%s: failed to link '%s' -> capsfilter", failMessage, converterName);
        return RC_ERROR;
    }

    if (head != NULL)
    {
        if (!gst_element_link(head, converter))
        {
            lfLogError("%s: failed to link '%s' -> '%s'", failMessage, GST_ELEMENT_NAME(head), converterName);
            return RC_ERROR;
        }
        syncStateWithPlayingParent(head);
        chain->head = head;
    }
    else
    {
        chain->head = converter;
    }
    chain->tail = capsfilter;

    syncStateWithPlayingParent(converter);
    syncStateWithPlayingParent(capsfilter);

    lfLogInfo("RGBA tail: %s -> capsfilter(RGBA)%s", converterName, head != NULL ? " (appended)" : "");

    return RC_OK;
}

Rc vcatRgbaChainCreate(VcatRgbaChain *chain, GstBin *bin)
{
    if (chain == NULL || bin == NULL)
    {
        return RC_BAD_INPUT_ARG;
    }

    // The upstream video is already raw system memory: conversion is all that
    // is left, so probe the generic converter candidates directly.
    return appendRgbaTail(chain, bin, NULL, NULL);
}

Rc vcatRgbaChainCreateForCodec(VcatRgbaChain *chain, GstBin *bin, VideoCodec codec)
{
    if (chain == NULL || bin == NULL)
    {
        return RC_BAD_INPUT_ARG;
    }

    for (size_t i = 0; i < kDecodeCandidatesLen; i++)
    {
        const DecodeCandidate *candidate = &kDecodeCandidates[i];
        if (candidate->codec != codec)
        {
            continue;
        }

        chain->head = gst_element_factory_make(candidate->decoder, NULL);
        if (chain->head == NULL)
        {
            continue;
        }

        gst_bin_add(bin, chain->head);
        if (appendRgbaTail(chain, bin, chain->head, candidate->converter) != RC_OK)
        {
            lfLogWarn("Skipping decoder candidate '%s': no compatible converter", candidate->decoder);
            gst_bin_remove(GST_BIN(bin), chain->head);
            continue;
        }

        lfLogInfo("Pinned decode chain for %s: %s -> %s -> capsfilter(RGBA)", videoCodecToString(codec),
                  candidate->decoder, candidate->converter != NULL ? candidate->converter : "<generic converter>");

        return RC_OK;
    }

    lfLogError("No decoder for %s found; install a GStreamer decoder for it", videoCodecToString(codec));
    return RC_ERROR;
}

VcatAutoplugSelectResult vcatDecodebinAutoplugSelect(GstElement *_,
                                                     GstPad *pad,
                                                     GstCaps *caps,
                                                     GstElementFactory *factory)
{
    // Raw video which is only emitted in device memory can never reach the
    // system-memory RGBA capsfilter closing every source bin: skip the
    // element so decodebin tries its next candidate.
    gboolean emitsRawVideo = FALSE;
    gboolean emitsSystemMemoryRawVideo = FALSE;

    GstCaps *systemRawVideo = gst_caps_from_string("video/x-raw");
    if (systemRawVideo == NULL)
    {
        return VCAT_AUTOPLUG_SELECT_TRY; // Not able to reason; let decodebin decide.
    }

    const GList *templates = gst_element_factory_get_static_pad_templates(factory);
    for (const GList *item = templates; item != NULL && !emitsSystemMemoryRawVideo; item = item->next)
    {
        GstStaticPadTemplate *template = item->data;
        if (template->direction != GST_PAD_SRC)
        {
            continue;
        }

        GstCaps *templateCaps = gst_static_caps_get(&template->static_caps);
        GstCaps *systemMemoryRawVideo = gst_caps_intersect_full(templateCaps, systemRawVideo, GST_CAPS_INTERSECT_FIRST);

        if (gst_caps_is_empty(systemMemoryRawVideo) == FALSE)
        {
            for (guint i = 0; i < gst_caps_get_size(systemMemoryRawVideo); ++i)
            {
                // Caps without features default to system memory, so every
                // member of the intersection is system-memory raw video.
                if (g_strcmp0(gst_structure_get_name(gst_caps_get_structure(systemMemoryRawVideo, i)), "video/x-raw") ==
                    0)
                {
                    emitsRawVideo = TRUE;
                    emitsSystemMemoryRawVideo = TRUE;
                }
            }
        }

        gst_caps_unref(systemMemoryRawVideo);
        gst_caps_unref(templateCaps);
    }

    gst_caps_unref(systemRawVideo);

    if (emitsRawVideo && !emitsSystemMemoryRawVideo)
    {
        lfLogInfo("Decodebin skipping '%s': raw video only in device memory",
                  gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory)));
        return VCAT_AUTOPLUG_SELECT_SKIP;
    }

    return VCAT_AUTOPLUG_SELECT_TRY;
}

static void linkPadToH26xParser(GstPad *newPad, GstElement *h26xparse)
{
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

    VideoCodec codec = VIDEO_CODEC_UNKNOWN;
    GstElement *h26xparse = NULL;
    if (g_str_has_prefix(name, "video/x-h265"))
    {
        codec = VIDEO_CODEC_H265;
        h26xparse = router->h265parse;
    }
    else if (g_str_has_prefix(name, "video/x-h264"))
    {
        codec = VIDEO_CODEC_H264;
        h26xparse = router->h264parse;
    }
    else
    {
        lfLogError("Unsupported pad type %s", name);
        gst_caps_unref(caps);
        return;
    }

    gst_caps_unref(caps);

    GstElement *tail = NULL;
    if (vcatH26xRouterPrepareCodecChain(router, codec, &tail) != RC_OK)
    {
        return;
    }

    linkPadToH26xParser(newPad, h26xparse);

    if (!gst_element_link(tail, router->sink))
    {
        lfLogError("Failed to link decode chain tail -> sink");
        return;
    }

    router->linked = true;
}

void vcatH26xRouterRoutePads(VcatH26xRouter *router, GstElement *demuxer, GstElement *sink)
{
    router->sink = sink;
    g_signal_connect(demuxer, "pad-added", G_CALLBACK(h26xRouterPadAdded), router);
}

Rc vcatH26xRouterCreate(VcatH26xRouter *router, GstBin *bin)
{
    if (router == NULL || bin == NULL)
    {
        return RC_BAD_INPUT_ARG;
    }

    char *failMessage = "Unable to configure H.264/H.265 parser router";

    memset(router, 0, sizeof(*router));

    router->bin = bin;

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

    router->h264parse = h264parse;
    router->h265parse = h265parse;

    gst_bin_add_many(bin, h265parse, h264parse, NULL);

    return RC_OK;
}

Rc vcatH26xRouterPrepareCodecChain(VcatH26xRouter *router, VideoCodec codec, GstElement **tail)
{
    if (router == NULL || tail == NULL)
    {
        return RC_BAD_INPUT_ARG;
    }

    *tail = NULL;

    VcatRgbaChain *chain = NULL;
    GstElement *h26xparse = NULL;
    switch (codec)
    {
    case VIDEO_CODEC_H264:
        chain = &router->h264Chain;
        h26xparse = router->h264parse;
        break;
    case VIDEO_CODEC_H265:
        chain = &router->h265Chain;
        h26xparse = router->h265parse;
        break;
    default:
        break;
    }

    if (chain == NULL)
    {
        lfLogError("No pinned decode route for codec %s", videoCodecToString(codec));
        return RC_ERROR;
    }

    if (chain->head == NULL)
    {
        if (vcatRgbaChainCreateForCodec(chain, router->bin, codec) != RC_OK)
        {
            return RC_ERROR;
        }

        if (!gst_element_link(h26xparse, chain->head))
        {
            lfLogError("Failed to link parser -> decode chain");
            return RC_ERROR;
        }
    }

    *tail = chain->tail;

    return RC_OK;
}

const char *videoCodecToString(VideoCodec codec)
{
    switch (codec)
    {
    case VIDEO_CODEC_H264:
        return "H264";
    case VIDEO_CODEC_H265:
        return "H265";
    case VIDEO_CODEC_AV1:
        return "AV1";
    case VIDEO_CODEC_VP8:
        return "VP8";
    case VIDEO_CODEC_VP9:
        return "VP9";
    case VIDEO_CODEC_MPEG2:
        return "MPEG2";
    case VIDEO_CODEC_MJPEG:
        return "MJPEG";
    case VIDEO_CODEC_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

const char *videoContainerToString(VideoContainer container)
{
    switch (container)
    {
    case VIDEO_CONTAINER_MPEG_TS:
        return "MPEG TS";
    case VIDEO_CONTAINER_MP4:
        return "MP4";
    case VIDEO_CONTAINER_MKV:
        return "MKV";
    case VIDEO_CONTAINER_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}
