
#pragma once

#include "gst/gstelement.h"

class IVideoSrc
{
  public:
    virtual GstElement *getSrcElement() const = 0;
    virtual GstElement *getSrcBin() const = 0;
};

