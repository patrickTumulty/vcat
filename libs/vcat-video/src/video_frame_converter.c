#include "video_frame_converter.h"
#include "logging_facade.h"
#include <string.h>

typedef struct FrameConverter
{
    GstElement *bin;
    GstElement *input;
    GstElement *downloadFilter;
    GstElement *bridge;
    gboolean linked;
} FrameConverter;

// These formats can be handed to the CPU converter. In particular, do not ask
// hardware to produce packed RGB/BGR: some advertised conversions (e.g. VIC)
// cannot actually perform that operation. Plain video/x-raw means system memory.
#define DOWNLOAD_CAPS "video/x-raw,format=(string){ BGRx, RGBA, NV12, I420 }"

static gboolean isMemoryBridge(GstPluginFeature *feature, gpointer userData)
{
    if (!GST_IS_ELEMENT_FACTORY(feature))
        return FALSE;

    GstElementFactory *factory = GST_ELEMENT_FACTORY(feature);
    const gchar *klass = gst_element_factory_get_metadata(factory, GST_ELEMENT_METADATA_KLASS);
    if (klass == NULL || strstr(klass, "Filter") == NULL || strstr(klass, "Video") == NULL)
        return FALSE;

    // A bridge must be a one-input, one-output raw-video transform. This also
    // finds download elements that aren't classified as colorspace converters.
    const GList *pads = gst_element_factory_get_static_pad_templates(factory);
    if (g_list_length((GList *)pads) != 2)
        return FALSE;
    gboolean acceptsMemory = FALSE;
    gboolean outputsSystemMemory = FALSE;
    for (const GList *item = pads; item != NULL; item = item->next)
    {
        GstStaticPadTemplate *pad = item->data;
        if (pad->presence != GST_PAD_ALWAYS ||
            (strcmp(pad->name_template, "sink") != 0 && strcmp(pad->name_template, "src") != 0))
            return FALSE;
        GstCaps *padCaps = gst_static_caps_get(&pad->static_caps);
        GstCaps *explicitCaps = gst_caps_copy(padCaps);
        gst_caps_unref(padCaps);
        // ANY features often mean passthrough, not the ability to download
        // arbitrary device memory. Require explicit memory-type support.
        for (guint i = gst_caps_get_size(explicitCaps); i > 0; --i)
        {
            if (gst_caps_features_is_any(gst_caps_get_features(explicitCaps, i - 1)))
                gst_caps_remove_structure(explicitCaps, i - 1);
        }
        if (pad->direction == GST_PAD_SINK)
        {
            acceptsMemory = !gst_caps_is_any(explicitCaps) && gst_caps_is_subset(userData, explicitCaps);
        }
        else
        {
            GstCaps *outputCaps = gst_caps_from_string(DOWNLOAD_CAPS);
            outputsSystemMemory = !gst_caps_is_any(explicitCaps) && gst_caps_can_intersect(explicitCaps, outputCaps);
            gst_caps_unref(outputCaps);
        }
        gst_caps_unref(explicitCaps);
    }

    return acceptsMemory && outputsSystemMemory;
}

static gboolean linkMemoryBridge(FrameConverter *converter, GstCaps *caps)
{
    GList *factories = gst_registry_feature_filter(gst_registry_get(), isMemoryBridge, FALSE, caps);
    factories = g_list_sort(factories, gst_plugin_feature_rank_compare_func);

    for (GList *item = factories; item != NULL; item = item->next)
    {
        GstElementFactory *factory = item->data;
        GstElement *bridge = gst_element_factory_create(factory, NULL);
        if (bridge == NULL)
            continue;

        gst_bin_add(GST_BIN(converter->bin), bridge);
        GstPad *sink = gst_element_get_static_pad(bridge, "sink");
        gboolean accepts = sink != NULL && gst_pad_query_accept_caps(sink, caps);
        if (sink != NULL)
            gst_object_unref(sink);

        if (accepts && gst_element_link(bridge, converter->downloadFilter) &&
            gst_element_link(converter->input, bridge) && gst_element_sync_state_with_parent(bridge))
        {
            converter->bridge = bridge;
            lfLogInfo("Video memory bridge: %s -> system memory -> videoconvert",
                      gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory)));
            break;
        }

        gst_element_set_state(bridge, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(converter->bin), bridge);
    }

    gst_plugin_feature_list_free(factories);
    return converter->bridge != NULL;
}

static GstPadProbeReturn onInputCaps(GstPad *pad, GstPadProbeInfo *info, gpointer userData)
{
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
    if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
        return GST_PAD_PROBE_OK;

    FrameConverter *converter = userData;
    GstCaps *caps = NULL;
    gst_event_parse_caps(event, &caps);

    GstCaps *systemCaps = gst_caps_from_string("video/x-raw");
    gboolean systemMemory = gst_caps_is_subset(caps, systemCaps);

    if (converter->linked)
    {
        if (converter->bridge == NULL && systemMemory)
        {
            gst_caps_unref(systemCaps);
            return GST_PAD_PROBE_OK;
        }
        if (converter->bridge != NULL && !systemMemory)
        {
            GstPad *sink = gst_element_get_static_pad(converter->bridge, "sink");
            gboolean accepts = gst_pad_query_accept_caps(sink, caps);
            gst_object_unref(sink);
            if (accepts)
            {
                gst_caps_unref(systemCaps);
                return GST_PAD_PROBE_OK;
            }
        }

        if (converter->bridge != NULL)
        {
            gst_element_set_state(converter->bridge, GST_STATE_NULL);
            gst_bin_remove(GST_BIN(converter->bin), converter->bridge);
            converter->bridge = NULL;
        }
        else
        {
            gst_element_unlink(converter->input, converter->downloadFilter);
        }
        converter->linked = FALSE;
    }

    if (systemMemory)
    {
        g_object_set(converter->downloadFilter, "caps", systemCaps, NULL);
        converter->linked = gst_element_link(converter->input, converter->downloadFilter);
        if (converter->linked)
            lfLogInfo("Video conversion: system memory -> videoconvert");
    }
    else
    {
        GstCaps *downloadCaps = gst_caps_from_string(DOWNLOAD_CAPS);
        g_object_set(converter->downloadFilter, "caps", downloadCaps, NULL);
        gst_caps_unref(downloadCaps);
        converter->linked = linkMemoryBridge(converter, caps);
    }
    gst_caps_unref(systemCaps);

    if (!converter->linked)
    {
        gchar *description = gst_caps_to_string(caps);
        lfLogError("No compatible video memory bridge for %s", description);
        GST_ELEMENT_ERROR(converter->bin, CORE, NEGOTIATION, ("No compatible video memory bridge for %s", description),
                          ("Install a GStreamer converter/download plugin supporting this memory type "
                           "and system-memory output"));
        g_free(description);
        return GST_PAD_PROBE_DROP;
    }

    return GST_PAD_PROBE_OK;
}

static gboolean queryInput(GstPad *pad, GstObject *parent, GstQuery *query)
{
    // The current bridge must not constrain negotiation of a replacement path.
    // Actual memory support is checked when the serialized CAPS event arrives.
    if (GST_QUERY_TYPE(query) == GST_QUERY_CAPS)
    {
        GstCaps *filter = NULL;
        gst_query_parse_caps(query, &filter);
        GstCaps *caps = gst_caps_from_string("video/x-raw(ANY)");
        if (filter != NULL)
        {
            GstCaps *intersection = gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
            gst_caps_unref(caps);
            caps = intersection;
        }
        gst_query_set_caps_result(query, caps);
        gst_caps_unref(caps);
        return TRUE;
    }
    if (GST_QUERY_TYPE(query) == GST_QUERY_ACCEPT_CAPS)
    {
        GstCaps *caps = NULL;
        gst_query_parse_accept_caps(query, &caps);
        GstCaps *raw = gst_caps_from_string("video/x-raw(ANY)");
        gst_query_set_accept_caps_result(query, gst_caps_is_subset(caps, raw));
        gst_caps_unref(raw);
        return TRUE;
    }
    return gst_pad_query_default(pad, parent, query);
}

GstElement *vcatVideoFrameConverterCreate(void)
{
    GstElement *bin = gst_bin_new("frame-converter");
    GstElement *input = gst_element_factory_make("identity", NULL);
    GstElement *filter = gst_element_factory_make("capsfilter", NULL);
    GstElement *cpu = gst_element_factory_make("videoconvert", NULL);
    if (bin == NULL || input == NULL || filter == NULL || cpu == NULL)
    {
        if (input != NULL)
            gst_object_unref(input);
        if (filter != NULL)
            gst_object_unref(filter);
        if (cpu != NULL)
            gst_object_unref(cpu);
        if (bin != NULL)
            gst_object_unref(bin);
        return NULL;
    }

    gst_bin_add_many(GST_BIN(bin), input, filter, cpu, NULL);
    if (!gst_element_link(filter, cpu))
    {
        gst_object_unref(bin);
        return NULL;
    }

    FrameConverter *converter = g_new0(FrameConverter, 1);
    converter->bin = bin;
    converter->input = input;
    converter->downloadFilter = filter;

    // Keep the input unlinked until CAPS arrives, so a source can expose its
    // native memory type. The streaming-thread probe links the appropriate path
    // before the CAPS event and the first buffer reach downstream elements.
    GstPad *pad = gst_element_get_static_pad(input, "sink");
    GstPad *ghost = gst_ghost_pad_new("sink", pad);
    gst_object_unref(pad);
    gst_pad_set_query_function(ghost, queryInput);
    gst_pad_add_probe(ghost, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, onInputCaps, converter, g_free);
    gst_element_add_pad(bin, ghost);
    pad = gst_element_get_static_pad(cpu, "src");
    ghost = gst_ghost_pad_new("src", pad);
    gst_object_unref(pad);
    gst_element_add_pad(bin, ghost);
    return bin;
}
