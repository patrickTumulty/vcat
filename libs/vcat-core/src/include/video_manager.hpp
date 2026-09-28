
#pragma once

#include "imatrix.hpp"
#include "utils.hpp"
#include "video_config.hpp"
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

    const std::unique_ptr<imatrix<char>> &getAsciiData() const;

  private:
    VideoConfig _config;
    std::unique_ptr<VideoManagerImpl> _impl;
};
