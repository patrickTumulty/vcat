
#include "video_pipeline.hpp"
#include "gst/app/gstappsink.h"
#include "gst/gstpad.h"
#include "gst/gstutils.h"
#include "gst/video/gstvideometa.h"
#include "logging.hpp"
#include "video2ascii_converter.hpp"
#include "videosrc.hpp"
#include <gst/gstcaps.h>
#include <memory>
#include <stdexcept>

#define RETURN_IF_NULL(VAR)                                                                                            \
    if (!(VAR))                                                                                                        \
    {                                                                                                                  \
        logging::error("Unable to initialize {}", #VAR);                                                               \
        return nullptr;                                                                                                \
    }

namespace
{

GstFlowReturn onNewSample(GstElement *sink, gpointer userData)
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

        auto videoSize = context->videoSize;

        context->pixelBuffer.resize(videoSize.height, videoSize.width);

        GstVideoMeta *meta = gst_buffer_get_video_meta(buffer);

        context->pixelStride = meta ? meta->stride[0] : videoSize.width * 3;

        const gchar *format = gst_structure_get_string(s, "format");

        logging::info("Resolution {}x{} stride {} '{}'", videoSize.width, videoSize.height, context->pixelStride,
                      format);
    }

    if (!context->resolutionSet)
    {
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    auto videoSize = context->videoSize;

    for (int y = 0; y < videoSize.height; y++)
    {
        const uint8_t *line = map.data + y * context->pixelStride;
        for (int x = 0; x < videoSize.width; x++)
        {
            const uint8_t *p = line + x * 3;
            context->pixelBuffer.set({p[0], p[1], p[2]}, x, y);
        }
    }

    context->converter->processPixelBuffer(context->pixelBuffer);

    gst_buffer_unmap(buffer, &map);

    gst_sample_unref(sample);

    return GST_FLOW_OK;
}
} // namespace

VideoPipeline::VideoPipeline(std::shared_ptr<IVideoSrc> videoSrc, std::shared_ptr<Video2AsciiConverter> converter)
{
    std::string failMessage = "Unable to init video pipeline";

    _context.converter = converter;

    _context.pipeline = gst_pipeline_new("vcat-pipeline");

    verifyPtr(_context.pipeline, STR(_context.pipeline), failMessage);

    _context.appsink = gst_element_factory_make("appsink", "appsink");
    verifyPtr(_context.appsink, STR(_context.appsink), failMessage);

    GstElement *videoconvert = gst_element_factory_make("videoconvert", nullptr);
    verifyPtr(videoconvert, STR(videoconvert), failMessage);

    GstElement *capsfilter = gst_element_factory_make("capsfilter", nullptr);
    verifyPtr(capsfilter, STR(capsfilter), failMessage);

    g_object_set(_context.appsink,     //
                 "emit-signals", TRUE, //
                 "sync", FALSE,        //
                 NULL);

    GstCaps *caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGB", NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    g_signal_connect(_context.appsink, "new-sample", G_CALLBACK(onNewSample), &_context);

    gst_bin_add_many(GST_BIN(_context.pipeline), //
                     _context.appsink,           //
                     videoSrc->getSrcBin(),      //
                     capsfilter,                 //
                     videoconvert,               //
                     NULL);

    if (!gst_element_link_many(videoSrc->getSrcElement(), //
                               videoconvert,              //
                               capsfilter,                //
                               _context.appsink,          //
                               nullptr))
    {
        gst_object_unref(_context.pipeline);
        gst_object_unref(_context.appsink);
        throw std::runtime_error(std::format("{}: failed to link elements", failMessage));
    }
}

VideoPipeline::~VideoPipeline()
{
    stop();
}

void VideoPipeline::start()
{
    logging::info("** Starting video pipeline");
    gst_element_set_state(_context.pipeline, GST_STATE_PLAYING);
}

void VideoPipeline::stop()
{
    if (_context.pipeline == nullptr)
    {
        return;
    }

    logging::info("** Stopping video pipeline");

    gst_element_set_state(_context.pipeline, GST_STATE_NULL);

    gst_element_get_state(_context.pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

    gst_object_unref(_context.pipeline);

    _context.pipeline = nullptr;
}
