
#pragma once

#include "video_config.hpp"
#include "videosrc.hpp"
#include <atomic>

struct UdpVideoSrcContext
{
    bool linked = false;
    GstElement *h264parse = nullptr;
    GstElement *h265parse = nullptr;
    GstElement *decoder = nullptr;
};

class UdpVideoSrc : public IVideoSrc
{
  public:
    explicit UdpVideoSrc(NetworkSource networkSource);

    GstElement *getSrcElement() const override
    {
        return _srcElement;
    }

    GstElement *getSrcBin() const override
    {
        return _srcBin;
    }

  private:
    UdpVideoSrcContext *_srcContext;
    GstElement *_srcElement;
    GstElement *_srcBin;
};
