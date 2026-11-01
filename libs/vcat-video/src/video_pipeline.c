
#include "video_pipeline.h"
#include "greedy_matrix.h"
#include "logging_facade.h"
#include "vcat_plugin_api.h"
#include "video2ascii_converter.h"
#include "video_manager.h"
#include <gst/app/gstappsink.h>
#include <gst/gstcaps.h>
#include <gst/gstelement.h>
#include <gst/gstpad.h>
#include <gst/gstutils.h>
#include <gst/video/video.h>
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


/**
 * @brief Owns a greedy matrix of the frame's pixels, resized as the video resolution changes.
 */
static void pipelineContextInit(PipelineContext *context)
{
    context->resolutionSet = false;
    context->videoSize.height = 0;
    context->videoSize.width = 0;
    context->pixelBuffer = NULL;
    context->appsink = NULL;
    context->pipeline = NULL;
    context->converter = NULL;
    (void)matrix_init_greedy(&context->pixelBuffer, 512, 512, sizeof(Pixel));
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

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);
    GstVideoInfo videoInfo;
    GstVideoFrame frame;

    if (buffer == NULL || caps == NULL || !gst_video_info_from_caps(&videoInfo, caps) ||
        GST_VIDEO_INFO_FORMAT(&videoInfo) != GST_VIDEO_FORMAT_RGBA ||
        !gst_video_frame_map(&frame, &videoInfo, buffer, GST_MAP_READ))
    {
        lfLogError("Unable to map RGBA video frame");
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    const int width = GST_VIDEO_FRAME_WIDTH(&frame);
    const int height = GST_VIDEO_FRAME_HEIGHT(&frame);
    const int pixelStride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);

    if ((height != 0 && width != 0) && (context->videoSize.height != height || context->videoSize.width != width))
    {
        context->resolutionSet = true;

        context->videoSize.height = height;
        context->videoSize.width = width;

        const Rectangle videoSize = context->videoSize;

        if (context->pixelBuffer == NULL)
        {
            matrix_init_greedy(&context->pixelBuffer, videoSize.height, videoSize.width, sizeof(Pixel));
        }
        else
        {
            matrix_resize(context->pixelBuffer, videoSize.height, videoSize.width);
        }

        lfLogInfo("Resolution %dx%d stride %d 'RGBA'", videoSize.width, videoSize.height, pixelStride);
    }

    if (!context->resolutionSet)
    {
        gst_video_frame_unmap(&frame);
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    const Rectangle videoSize = context->videoSize;

    for (int y = 0; y < videoSize.height; y++)
    {
        const uint8_t *line = (const uint8_t *)GST_VIDEO_FRAME_PLANE_DATA(&frame, 0) + y * pixelStride;
        for (int x = 0; x < videoSize.width; x++)
        {
            const uint8_t *p = line + x * 4;
            Pixel *cell = matrix_get_cell(context->pixelBuffer, x, y);
            if (cell != NULL)
            {
                cell->r = p[0];
                cell->g = p[1];
                cell->b = p[2];
            }
        }
    }

    vcatVideo2AsciiConverterProcessPixelBuffer(context->converter, context->pixelBuffer);

    gst_video_frame_unmap(&frame);

    gst_sample_unref(sample);

    return GST_FLOW_OK;
}

static GstPadProbeReturn onCapsParsed(GstPad *pad, GstPadProbeInfo *info, gpointer userData)
{
    PipelineContext *context = (PipelineContext *)userData;

    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM))
        return GST_PAD_PROBE_OK;

    GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);

    if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
        return GST_PAD_PROBE_OK;

    GstCaps *caps = NULL;
    gst_event_parse_caps(event, &caps);

    const GstStructure *s = gst_caps_get_structure(caps, 0);

    gchar *capsString = gst_caps_to_string(caps);

    if (context->capsChangeCB != NULL && capsString != NULL)
    {
        context->capsChangeCB(capsString);
    }

    g_free(capsString);

    return GST_PAD_PROBE_OK;
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
    gst_bin_add(GST_BIN(context->pipeline), context->appsink);

    // Source bins already deliver system-memory RGBA; the caps only pin the layout for the frame map.
    GstCaps *caps = gst_caps_new_simple("video/x-raw",                   //
                                        "format", G_TYPE_STRING, "RGBA", //
                                        NULL);

    g_object_set(context->appsink,     //
                 "caps", caps,         //
                 "emit-signals", TRUE, //
                 "sync", FALSE,        //
                 NULL);

    gst_caps_unref(caps);

    g_signal_connect(context->appsink, "new-sample", G_CALLBACK(onNewSample), context);

    GstElement *videoSourceBin = vcatVideoSourceGetSourceBin(videoSource);

    gst_bin_add(GST_BIN(context->pipeline), videoSourceBin);

    GstElement *videoSourceElement = vcatVideoSourceGetSourceElement(videoSource);

    if (!gst_element_link(videoSourceElement, context->appsink))
    {
        lfLogError("%s : failed to link elements", failMessage);
        goto FAIL;
    }

    GstPad *sinkPad = gst_element_get_static_pad(context->appsink, "sink");
    gst_pad_add_probe(sinkPad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, onCapsParsed, context, NULL);
    gst_object_unref(sinkPad);

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
