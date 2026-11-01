
#ifndef VCAT_VIDEO_PIPELINE_H
#define VCAT_VIDEO_PIPELINE_H

#include "vcat_plugin_api.h"
#include "video2ascii_converter.h"
#include "video_manager.h"

typedef struct PipelineContext
{
    bool resolutionSet;
    Rectangle videoSize;
    matrix_t pixelBuffer;
    GstElement *appsink;
    GstElement *pipeline;
    Video2AsciiConverter *converter;
    OnCapsChangeCB capsChangeCB;
} PipelineContext;

typedef struct VideoPipeline
{
    PipelineContext context;
} VideoPipeline;

VideoPipeline *vcatVideoPipelineCreate(VCatPluginHandle videoSource, Video2AsciiConverter *converter);

void vcatVideoPipelineFree(VideoPipeline *pipeline);

void vcatVideoPipelineStart(VideoPipeline *pipeline);

void vcatVideoPipelineStop(VideoPipeline *pipeline);

#endif // VCAT_VIDEO_PIPELINE_H
