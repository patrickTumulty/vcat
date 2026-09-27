
#pragma once

#include "video_config.hpp"
#include "utils.hpp"
#include <memory>

class VideoManagerImpl;

class VideoManager
{
  public:
    explicit VideoManager(VideoConfig config);
    ~VideoManager();

    void run();
    void stop();

    void updateVideoBounds(Rectangle videoBounds);

  private:
    VideoConfig _config;
    std::unique_ptr<VideoManagerImpl> _impl;
};
