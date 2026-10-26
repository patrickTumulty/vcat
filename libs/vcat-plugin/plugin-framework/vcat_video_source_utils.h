
#ifndef VCAT_VIDEO_SOURCE_UTILS_H
#define VCAT_VIDEO_SOURCE_UTILS_H

#include "crox_types.h"
#include <gst/gstbin.h>
#include <gst/gstelement.h>
#include <stdbool.h>

/**
 * @brief routes dynamically produced H.264/H.265 pads into bitstream parsers
 * and a decoder
 */
typedef struct
{
    GstElement *h264parse;
    GstElement *h265parse;
    GstElement *decoder;
    bool linked;
} VcatH26xRouter;

typedef enum
{
    VIDEO_CODEC_UNKNOWN = 0,
    VIDEO_CODEC_H264,  // H.264 / AVC
    VIDEO_CODEC_H265,  // H.265 / HEVC
    VIDEO_CODEC_AV1,   // AV1
    VIDEO_CODEC_VP8,   // VP8
    VIDEO_CODEC_VP9,   // VP9
    VIDEO_CODEC_MPEG2, // MPEG-2 Video
    VIDEO_CODEC_MJPEG, // Motion JPEG
} VideoCodec;

const char *videoCodecToString(VideoCodec codec);

typedef enum
{
    VIDEO_CONTAINER_UNKNOWN = 0,
    VIDEO_CONTAINER_MPEG_TS, // MPEG Transport Stream (.ts)
    VIDEO_CONTAINER_MP4,     // MPEG-4 Part 14 (.mp4)
    VIDEO_CONTAINER_MKV      // Matroska (.mkv)
} VideoContainer;

const char *videoContainerToString(VideoContainer container);

/**
 * @brief route decodebin's dynamic decoded-video pads into @p sink
 *
 * Connects decodebin's "pad-added": the first decoded pad is linked to
 * @p sink, later pads are skipped.
 */
void vcatRouteDecodebinPads(GstElement *decodebin, GstElement *sink);

/**
 * @brief build an h264parse/h265parse + decodebin tail
 *
 * Creates the parsers ("config-interval"=1) and a "decodebin", and adds them
 * to @p bin. The parsers are NOT linked into the decoder here: decodebin has
 * a single sink pad, so exactly one parser may feed it - call
 * vcatH26xRouterLinkParser once, when the first video pad is routed.
 *
 * @return RC_OK if successful
 */
Rc vcatH26xRouterCreate(VcatH26xRouter *router, GstBin *bin);

/**
 * @brief wire the selected parser into the decoder
 *
 * Only one parser can feed decodebin (single sink pad); safe to ignore
 * further calls after the first.
 *
 * @return RC_OK if successfully wired, RC_ERROR on a link failure
 */
Rc vcatH26xRouterLinkParser(VcatH26xRouter *router, GstElement *h26xparse);

/**
 * @brief route new H.264/H.265 elementary-stream pads of @p demuxer into
 * @p router
 *
 * Connects the demuxer's "pad-added": pads carrying video/x-h264 or
 * video/x-h265 are linked to the matching parser; the first one wins.
 */
void vcatH26xRouterRoutePads(VcatH26xRouter *router, GstElement *demuxer);

#endif // VCAT_VIDEO_SOURCE_UTILS_H
