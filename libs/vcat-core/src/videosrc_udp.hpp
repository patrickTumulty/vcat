
#pragma once

#include "gst/gstelement.h"
#include "utils.hpp"
#include "videosrc.hpp"

struct UdpVideoSrcContext
{
    bool linked = false;
    GstElement *h265parse;
    GstElement *decoder;
    GstElement *h264parse;
    GstElement *h264src;
};

class UdpVideoSrc : public IVideoSrc
{
  public:
    explicit UdpVideoSrc(Ip ip, int port);

    GstElement *getSrcElement() const override
    {
        return _srcElement;
    }

    GstElement *getSrcBin() const override
    {
        return _srcBin;
    }

  private:
    UdpVideoSrcContext _srcContext;
    GstElement *_srcElement;
    GstElement *_srcBin;
};
