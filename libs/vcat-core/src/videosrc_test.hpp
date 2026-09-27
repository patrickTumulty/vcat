
#pragma once

#include "videosrc.hpp"

class TestVideoSrc : public IVideoSrc
{
  public:
    TestVideoSrc();

    GstElement *getSrcElement() const override
    {
        return _srcElement;
    }

    GstElement *getSrcBin() const override
    {
        return _srcBin;
    }

  private:
    GstElement *_srcElement;
    GstElement *_srcBin;
};
