#include "video_frame_converter.h"
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include <string.h>

// Model a vendor-neutral, opaque-memory download plugin. The test storage is
// CPU-backed; only the caps contract is synthetic. No GPU is needed to verify
// discovery, memory negotiation, switching, and the final RGB pixel values.
typedef struct
{
    GstBaseTransform parent;
} TestDownload;

typedef struct
{
    GstBaseTransformClass parent;
} TestDownloadClass;
G_DEFINE_TYPE(TestDownload, test_download, GST_TYPE_BASE_TRANSFORM)

static gint downloads;
static gint instances;

static GstCaps *testTransformCaps(GstBaseTransform *self, GstPadDirection direction,
                                 GstCaps *caps, GstCaps *filter)
{
    GstCaps *result = gst_caps_copy(caps);
    for (guint i = 0; i < gst_caps_get_size(result); ++i)
    {
        gst_caps_set_features(result, i, gst_caps_features_new(
            direction == GST_PAD_SINK ? GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY : "memory:VCatTestSurface", NULL));
    }
    if (filter != NULL)
    {
        GstCaps *intersection = gst_caps_intersect_full(filter, result, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(result);
        result = intersection;
    }
    return result;
}

static GstFlowReturn testTransform(GstBaseTransform *self, GstBuffer *buffer)
{
    g_atomic_int_inc(&downloads);
    return GST_FLOW_OK;
}

static void test_download_class_init(TestDownloadClass *klass)
{
    GstElementClass *element = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element, "Test surface download", "Filter/Video",
                                          "Download synthetic device memory", "vcat");
    GstCaps *caps = gst_caps_from_string(
        "video/x-raw(memory:VCatTestSurface),format=BGRx,width=[1,2147483647],height=[1,2147483647],"
        "framerate=[0/1,2147483647/1];"
        "video/x-raw,format=BGRx,width=[1,2147483647],height=[1,2147483647],framerate=[0/1,2147483647/1]");
    gst_element_class_add_pad_template(element, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps));
    gst_caps_unref(caps);
    caps = gst_caps_from_string("video/x-raw,format=BGRx,width=[1,2147483647],height=[1,2147483647],"
                                "framerate=[0/1,2147483647/1]");
    gst_element_class_add_pad_template(element, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));
    gst_caps_unref(caps);
    GST_BASE_TRANSFORM_CLASS(klass)->transform_caps = testTransformCaps;
    GST_BASE_TRANSFORM_CLASS(klass)->transform_ip = testTransform;
}

static void test_download_init(TestDownload *self)
{
    g_atomic_int_inc(&instances);
    gst_base_transform_set_in_place(GST_BASE_TRANSFORM(self), TRUE);
}

typedef struct
{
    GstElement *pipeline;
    GstElement *source;
    GstElement *sink;
} TestPipeline;

static TestPipeline createPipeline(void)
{
    TestPipeline test = {gst_pipeline_new(NULL), gst_element_factory_make("appsrc", NULL),
                         gst_element_factory_make("appsink", NULL)};
    GstElement *converter = vcatVideoFrameConverterCreate();
    g_assert_nonnull(converter);
    g_assert_nonnull(test.source);
    g_assert_nonnull(test.sink);
    GstCaps *caps = gst_caps_from_string("video/x-raw,format=RGB");
    g_object_set(test.sink, "caps", caps, "sync", FALSE, "async", FALSE, NULL);
    gst_caps_unref(caps);
    g_object_set(test.source, "format", GST_FORMAT_TIME, NULL);
    gst_bin_add_many(GST_BIN(test.pipeline), test.source, converter, test.sink, NULL);
    g_assert_true(gst_element_link_many(test.source, converter, test.sink, NULL));
    g_assert_cmpint(gst_element_set_state(test.pipeline, GST_STATE_PLAYING), !=, GST_STATE_CHANGE_FAILURE);
    return test;
}

static void pushFrame(TestPipeline *test, const char *memory, int width)
{
    GstVideoInfo info;
    gst_video_info_set_format(&info, GST_VIDEO_FORMAT_BGRx, width, 2);
    info.fps_n = 30;
    info.fps_d = 1;
    GstCaps *caps = gst_video_info_to_caps(&info);
    if (memory != NULL)
        gst_caps_set_features(caps, 0, gst_caps_features_new(memory, NULL));
    gst_app_src_set_caps(GST_APP_SRC(test->source), caps);
    gst_caps_unref(caps);

    GstBuffer *buffer = gst_buffer_new_allocate(NULL, info.size, NULL);
    GstMapInfo map;
    g_assert_true(gst_buffer_map(buffer, &map, GST_MAP_WRITE));
    for (gsize i = 0; i < map.size; ++i)
        map.data[i] = i % 4 == 2 ? 255 : 0;
    gst_buffer_unmap(buffer, &map);
    GST_BUFFER_PTS(buffer) = 0;
    GST_BUFFER_DURATION(buffer) = GST_SECOND / 30;
    g_assert_cmpint(gst_app_src_push_buffer(GST_APP_SRC(test->source), buffer), ==, GST_FLOW_OK);
}

static void checkFrame(TestPipeline *test, int width)
{
    GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(test->sink), 5 * GST_SECOND);
    g_assert_nonnull(sample);
    GstVideoInfo info;
    g_assert_true(gst_video_info_from_caps(&info, gst_sample_get_caps(sample)));
    g_assert_cmpint(GST_VIDEO_INFO_FORMAT(&info), ==, GST_VIDEO_FORMAT_RGB);
    g_assert_cmpint(GST_VIDEO_INFO_WIDTH(&info), ==, width);
    g_assert_true(gst_caps_features_contains(gst_caps_get_features(gst_sample_get_caps(sample), 0),
                                            GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY));
    GstVideoFrame frame;
    g_assert_true(gst_video_frame_map(&frame, &info, gst_sample_get_buffer(sample), GST_MAP_READ));
    for (int y = 0; y < 2; ++y)
    {
        const guint8 *row = (const guint8 *)GST_VIDEO_FRAME_PLANE_DATA(&frame, 0) +
                            y * GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);
        for (int x = 0; x < width; ++x)
        {
            g_assert_cmpint(row[x * 3], ==, 255);
            g_assert_cmpint(row[x * 3 + 1], ==, 0);
            g_assert_cmpint(row[x * 3 + 2], ==, 0);
        }
    }
    gst_video_frame_unmap(&frame);
    gst_sample_unref(sample);
}

static void freePipeline(TestPipeline *test)
{
    gst_element_set_state(test->pipeline, GST_STATE_NULL);
    gst_object_unref(test->pipeline);
}

static void testSystemMemory(void)
{
    g_atomic_int_set(&instances, 0);
    TestPipeline test = createPipeline();
    pushFrame(&test, NULL, 5);
    checkFrame(&test, 5);
    // A high-ranked hardware plugin accepting this format must not be used.
    g_assert_cmpint(g_atomic_int_get(&instances), ==, 0);
    freePipeline(&test);
}

static void testDeviceMemory(void)
{
    g_atomic_int_set(&downloads, 0);
    TestPipeline test = createPipeline();
    pushFrame(&test, "memory:VCatTestSurface", 5);
    checkFrame(&test, 5);
    g_assert_cmpint(g_atomic_int_get(&downloads), ==, 1);
    // Resolution changes retain the bridge; memory changes select a new path.
    pushFrame(&test, "memory:VCatTestSurface", 7);
    checkFrame(&test, 7);
    pushFrame(&test, NULL, 7);
    checkFrame(&test, 7);
    g_assert_cmpint(g_atomic_int_get(&downloads), ==, 2);
    pushFrame(&test, "memory:VCatTestSurface", 5);
    checkFrame(&test, 5);
    g_assert_cmpint(g_atomic_int_get(&downloads), ==, 3);
    freePipeline(&test);
}

static void testUnsupportedMemory(void)
{
    TestPipeline test = createPipeline();
    pushFrame(&test, "memory:VCatUnsupported", 5);
    GstBus *bus = gst_element_get_bus(test.pipeline);
    GstMessage *message = gst_bus_timed_pop_filtered(bus, 5 * GST_SECOND, GST_MESSAGE_ERROR);
    g_assert_nonnull(message);
    GError *error = NULL;
    gst_message_parse_error(message, &error, NULL);
    g_assert_nonnull(strstr(error->message, "No compatible video memory bridge"));
    g_error_free(error);
    gst_message_unref(message);
    gst_object_unref(bus);
    freePipeline(&test);
}

int main(int argc, char **argv)
{
    gst_init(&argc, &argv);
    g_test_init(&argc, &argv, NULL);
    g_assert_true(gst_element_register(NULL, "vcat-test-download", GST_RANK_PRIMARY + 100,
                                      test_download_get_type()));
    g_test_add_func("/frame-converter/system-memory", testSystemMemory);
    g_test_add_func("/frame-converter/device-memory", testDeviceMemory);
    g_test_add_func("/frame-converter/unsupported-memory", testUnsupportedMemory);
    return g_test_run();
}
