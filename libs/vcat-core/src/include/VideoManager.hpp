
#pragma once

#include "VideoConfig.hpp"
#include <memory>

class VideoManagerImpl;

class VideoManager
{
  public:
    explicit VideoManager(VideoConfig config);
    ~VideoManager();

    void run();
    void stop();

  private:
    VideoConfig _config;
    std::unique_ptr<VideoManagerImpl> _impl;
};
