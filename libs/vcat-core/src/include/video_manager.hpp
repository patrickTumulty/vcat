
#pragma once

#include "imatrix.hpp"
#include "utils.hpp"
#include "video_config.hpp"
#include "recycling_queue.hpp"
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
    std::shared_ptr<IRecyclingQueueReader<imatrix<char>>> getAsciiDataQueue();

  private:
    VideoConfig _config;
    std::unique_ptr<VideoManagerImpl> _impl;
};
