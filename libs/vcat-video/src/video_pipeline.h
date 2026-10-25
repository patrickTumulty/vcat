
#ifndef VCAT_VIDEO_PIPELINE_H
#define VCAT_VIDEO_PIPELINE_H

#include "vcat_plugin_api.h"
#include "video2ascii_converter.h"
#include <gst/gstpipeline.h>

/**
 * @brief The GStreamer pipeline that pulls frames from a video source plugin.
 *
 * Created against a video source plugin handle; owns its pipeline until freed.
 */
typedef struct VideoPipeline VcatVideoPipeline;

/**
 * @brief Builds the pipeline: source bin -> videoconvert -> capsfilter -> appsink.
 *
 * @param[in] videoSource An initialized video source plugin handle.
 * @param[in] converter   Receives every captured frame; borrowed, not owned.
 * @return The pipeline, or NULL after logging what failed to come up.
 */
VcatVideoPipeline *vcatVideoPipelineCreate(VCatPluginHandle videoSource, Video2AsciiConverter *converter);

/**
 * @brief Stops the pipeline if it is playing, then frees it.
 */
void vcatVideoPipelineFree(VcatVideoPipeline *pipeline);

/**
 * @brief Switches the pipeline to playing.
 */
void vcatVideoPipelineStart(VcatVideoPipeline *pipeline);

/**
 * @brief Switches the pipeline to NULL state and releases the GStreamer objects.
 */
void vcatVideoPipelineStop(VcatVideoPipeline *pipeline);

#endif // VCAT_VIDEO_PIPELINE_H
