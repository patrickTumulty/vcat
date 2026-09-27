
#pragma once

#include "greedy_matrix.hpp"
#include "gst/gstelement.h"
#include "utils.hpp"
#include "video2ascii_converter.hpp"
#include "videosrc.hpp"

struct PipelineContext
{
    bool resolutionSet = false;
    Rectangle videoSize{0, 0};
    int pixelStride = 3;
    VideoSourceType videoSource = NONE;
    greedy_matrix<pixel> pixelBuffer = greedy_matrix<pixel>(512, 512);
    union {
        NetworkSource network{};
    };
    GstElement *appsink;
    GstElement *pipeline;
    std::shared_ptr<Video2AsciiConverter> converter;
};

class VideoPipeline
{
  public:
    VideoPipeline(std::shared_ptr<IVideoSrc> videoSrc, std::shared_ptr<Video2AsciiConverter> converter);
    ~VideoPipeline();

    void start();
    void stop();

  private:
    PipelineContext _context{};
};
