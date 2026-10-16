
#pragma once

#include "greedy_matrix.h"
#include "gst/gstelement.h"
#include "matrix.h"
#include "utils.hpp"
#include "video2ascii_converter.hpp"
extern "C"
{
#include "vcat_plugin_api.h"
}

struct PipelineContext
{
    bool resolutionSet = false;
    Rectangle videoSize{0, 0};
    int pixelStride = 3;
    matrix_t pixelBuffer = nullptr;
    GstElement *appsink = nullptr;
    GstElement *pipeline = nullptr;
    std::shared_ptr<Video2AsciiConverter> converter = nullptr;

    /// Owns a greedy matrix of the frame's pixels, resized as the video resolution changes.
    PipelineContext()
    {
        matrix_init_greedy(&pixelBuffer, 512, 512, sizeof(pixel));
    }

    ~PipelineContext()
    {
        matrix_free(pixelBuffer);
        pixelBuffer = nullptr;
    }

    PipelineContext(const PipelineContext &) = delete;
    PipelineContext &operator=(const PipelineContext &) = delete;
};

class VideoPipeline
{
  public:
    VideoPipeline(VCatPluginHandle videoSource, std::shared_ptr<Video2AsciiConverter> converter);
    ~VideoPipeline();

    void start();
    void stop();

  private:
    PipelineContext _context{};
};
