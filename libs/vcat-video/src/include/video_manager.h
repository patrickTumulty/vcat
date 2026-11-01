
#ifndef VCAT_VIDEO_MANAGER_H
#define VCAT_VIDEO_MANAGER_H

#include "ascii_frame_mailbox.h"
#include "utils.h"
#include "vcat_plugin_api.h"
#include "video2ascii_converter.h"

typedef struct VideoManager VideoManager;

VideoManager *vcatVideoManagerCreate(VCatPluginHandle videoSource);

void vcatVideoManagerFree(VideoManager *manager);

void vcatVideoManagerRun(VideoManager *manager);

void vcatVideoManagerStop(VideoManager *manager);

void vcatVideoManagerUpdateVideoBounds(VideoManager *manager, Rectangle videoBounds);

/// Runs on a GStreamer streaming thread; only borrows the string (freed by the probe).
typedef void (*OnCapsChangeCB)(const char *capsStr);

void vcatVideoManagerSetOnCapsChangeEvent(VideoManager *manager, OnCapsChangeCB callback);

AsciiFrameMailbox *vcatVideoManagerGetFrameMailbox(VideoManager *manager);

Video2AsciiConverter *vcatVideoManagerGetAsciiConverter(VideoManager *manager);

#endif // VCAT_VIDEO_MANAGER_H
