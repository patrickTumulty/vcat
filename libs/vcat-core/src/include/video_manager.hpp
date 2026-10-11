
#pragma once

#include "utils.hpp"
#include "ascii_frame_mailbox.hpp"
#include <memory>

class VideoManagerImpl;

class VideoManager
{
  public:
    explicit VideoManager();
    ~VideoManager();

    void run();
    void stop();

    void updateVideoBounds(Rectangle videoBounds);
    std::shared_ptr<IAsciiFrameMailboxReader> accessAsciiFrameMailbox();

  private:
    std::unique_ptr<VideoManagerImpl> _impl;
};
