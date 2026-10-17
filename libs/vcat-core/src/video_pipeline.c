
#include "video_pipeline.h"
#include "greedy_matrix.h"
#include "logging.h"
#include "logging_facade.h"
#include "vcat_plugin_api.h"
#include "video2ascii_converter.h"
#include <gst/app/gstappsink.h>
#include <gst/gstcaps.h>
#include <gst/gstpad.h>
#include <gst/gstutils.h>
#include <gst/video/gstvideometa.h>
#include <stdlib.h>

/**
 * @brief Logs the named element and jumps to the create function's fail label.
 */
#define VCAT_VERIFY_ELEMENT(ELEMENT, MESSAGE)                              \
    if (!(ELEMENT))                                                        \
    {                                                                      \
        lfLogError("%s : unable to create '%s'", (MESSAGE), STR(ELEMENT)); \
        goto FAIL;                                                         \
    }

typedef struct PipelineContext
{
    bool resolutionSet;
    Rectangle videoSize;
    int pixelStride;
    matrix_t pixelBuffer;
    GstElement *appsink;
    GstElement *pipeline;
    Video2AsciiConverter *converter;
} PipelineContext;

struct VideoPipeline
{
    PipelineContext context;
};

/**
 * @brief Owns a greedy matrix of the frame's pixels, resized as the video resolution changes.
 */
static void pipelineContextInit(PipelineContext *context)
{
    context->resolutionSet = false;
    context->videoSize.height = 0;
    context->videoSize.width = 0;
    context->pixelStride = 3;
    context->pixelBuffer = NULL;
    context->appsink = NULL;
    context->pipeline = NULL;
    context->converter = NULL;
    (void)matrix_init_greedy(&context->pixelBuffer, 512, 512, sizeof(pixel));
}

static void pipelineContextFree(PipelineContext *context)
{
    matrix_free(context->pixelBuffer);
    context->pixelBuffer = NULL;
}

static GstFlowReturn onNewSample(GstElement *sink, gpointer userData)
{
    PipelineContext *context = (PipelineContext *)userData;

    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));

    if (!sample)
        return GST_FLOW_ERROR;

    GstMapInfo map;
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    int height = 0, width = 0;

    if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
    {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    GstCaps *caps = gst_sample_get_caps(sample);
    GstStructure *s = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);

    if ((height != 0 && width != 0) && (context->videoSize.height != height || context->videoSize.width != width))
    {
        context->resolutionSet = true;

        context->videoSize.height = height;
        context->videoSize.width = width;

        const Rectangle videoSize = context->videoSize;

        if (context->pixelBuffer == NULL)
        {
            matrix_init_greedy(&context->pixelBuffer, videoSize.height, videoSize.width, sizeof(pixel));
        }
        else
        {
            matrix_resize(context->pixelBuffer, videoSize.height, videoSize.width);
        }

        GstVideoMeta *meta = gst_buffer_get_video_meta(buffer);

        context->pixelStride = meta ? meta->stride[0] : videoSize.width * 3;

        const gchar *format = gst_structure_get_string(s, "format");

        lfLogInfo("Resolution %dx%d stride %d '%s'", videoSize.width, videoSize.height, context->pixelStride,
                  format != NULL ? format : "NULL");
    }

    if (!context->resolutionSet)
    {
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    const Rectangle videoSize = context->videoSize;

    for (int y = 0; y < videoSize.height; y++)
    {
        const uint8_t *line = map.data + y * context->pixelStride;
        for (int x = 0; x < videoSize.width; x++)
        {
            const uint8_t *p = line + x * 3;
            pixel *cell = matrix_get_cell(context->pixelBuffer, x, y);
            if (cell != NULL)
            {
                cell->r = p[0];
                cell->g = p[1];
                cell->b = p[2];
            }
        }
    }

    vcatVideo2AsciiConverterProcessPixelBuffer(context->converter, context->pixelBuffer);

    gst_buffer_unmap(buffer, &map);

    gst_sample_unref(sample);

    return GST_FLOW_OK;
}

VideoPipeline *vcatVideoPipelineCreate(VCatPluginHandle videoSource, Video2AsciiConverter *converter)
{
    const char *failMessage = "Unable to init video pipeline";

    VideoPipeline *pipeline = calloc(1, sizeof(*pipeline));
    if (pipeline == NULL)
    {
        lfLogError("%s : out of memory", failMessage);
        return NULL;
    }

    PipelineContext *context = &pipeline->context;
    pipelineContextInit(context);
    context->converter = converter;

    context->pipeline = gst_pipeline_new("vcat-pipeline");
    VCAT_VERIFY_ELEMENT(context->pipeline, failMessage);

    context->appsink = gst_element_factory_make("appsink", "appsink");
    VCAT_VERIFY_ELEMENT(context->appsink, failMessage);

    GstElement *videoconvert = gst_element_factory_make("videoconvert", NULL);
    VCAT_VERIFY_ELEMENT(videoconvert, failMessage);

    GstElement *capsfilter = gst_element_factory_make("capsfilter", NULL);
    VCAT_VERIFY_ELEMENT(capsfilter, failMessage);

    g_object_set(context->appsink,     //
                 "emit-signals", TRUE, //
                 "sync", FALSE,        //
                 NULL);

    GstCaps *caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGB", NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    g_signal_connect(context->appsink, "new-sample", G_CALLBACK(onNewSample), context);

    gst_bin_add_many(GST_BIN(context->pipeline),               //
                     context->appsink,                         //
                     vcatVideoSourceGetSourceBin(videoSource), //
                     capsfilter,                               //
                     videoconvert,                             //
                     NULL);

    if (!gst_element_link_many(vcatVideoSourceGetSourceElement(videoSource), //
                               videoconvert,                                 //
                               capsfilter,                                   //
                               context->appsink,                             //
                               NULL))
    {
        lfLogError("%s : failed to link elements", failMessage);
        goto FAIL;
    }

    return pipeline;

FAIL:
    if (context->pipeline != NULL)
    {
        // The bin took ownership of the elements added above; destroying the
        // pipeline releases them all.
        gst_object_unref(context->pipeline);
        context->pipeline = NULL;
    }
    pipelineContextFree(context);
    free(pipeline);
    return NULL;
}

void vcatVideoPipelineFree(VideoPipeline *pipeline)
{
    if (pipeline == NULL)
    {
        return;
    }
    vcatVideoPipelineStop(pipeline);
    pipelineContextFree(&pipeline->context);
    free(pipeline);
}

void vcatVideoPipelineStart(VideoPipeline *pipeline)
{
    if (pipeline == NULL)
    {
        return;
    }

    lfLogInfo("** Starting video pipeline");
    gst_element_set_state(pipeline->context.pipeline, GST_STATE_PLAYING);
}

void vcatVideoPipelineStop(VideoPipeline *pipeline)
{
    if (pipeline == NULL || pipeline->context.pipeline == NULL)
    {
        return;
    }

    lfLogInfo("** Stopping video pipeline");

    gst_element_set_state(pipeline->context.pipeline, GST_STATE_NULL);

    gst_element_get_state(pipeline->context.pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

    gst_object_unref(pipeline->context.pipeline);

    pipeline->context.pipeline = NULL;
}
