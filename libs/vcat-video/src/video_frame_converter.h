#ifndef VCAT_VIDEO_FRAME_CONVERTER_H
#define VCAT_VIDEO_FRAME_CONVERTER_H

#include <gst/gst.h>

// Converts negotiated raw video to system-memory RGB, discovering an installed
// memory bridge when the source exposes hardware-backed frames.
GstElement *vcatVideoFrameConverterCreate(void);

#endif
