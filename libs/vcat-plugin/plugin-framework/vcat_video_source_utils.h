
#ifndef VCAT_VIDEO_SOURCE_UTILS_H
#define VCAT_VIDEO_SOURCE_UTILS_H

#include "crox_types.h"
#include <gst/gstbin.h>
#include <gst/gstcaps.h>
#include <gst/gstelement.h>
#include <gst/gstpad.h>
#include <gst/gstutils.h>
#include <stdbool.h>

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

/**
 * @brief a chain producing system-memory RGBA: link upstream video into
 * @p head and @p tail into the source bin's exposed queue.
 */
typedef struct
{
    GstElement *head;
    GstElement *tail;
} VcatRgbaChain;

/**
 * @brief build a convert-to-RGBA tail for already-decoded (raw system-memory) video
 *
 * @return RC_OK if successful, RC_ERROR when no converter could be created
 */
rc_t vcatRgbaChainCreate(VcatRgbaChain *chain, GstBin *bin);

/**
 * @brief build a pinned decode chain for @p codec: decoder + convert-to-RGBA tail
 *
 * @return RC_OK if successful, RC_ERROR when no candidate is installed
 */
rc_t vcatRgbaChainCreateForCodec(VcatRgbaChain *chain, GstBin *bin, VideoCodec codec);

/**
 * @brief Mirror of gst-plugins-base's private `GstAutoplugSelectResult`, the
 * return type of decodebin's "autoplug-select" signal (numeric values match).
 */
typedef enum
{
    VCAT_AUTOPLUG_SELECT_TRY = 0,
    VCAT_AUTOPLUG_SELECT_EXPOSE = 1,
    VCAT_AUTOPLUG_SELECT_SKIP = 2,
} VcatAutoplugSelectResult;

/**
 * @brief decodebin "autoplug-select" handler; SKIP decoders that can only
 * emit device-memory raw video (e.g. NVMM, CUDA): the RGBA tail requires
 * system memory, and decodebin plugs its next candidate instead.
 */
VcatAutoplugSelectResult vcatDecodebinSkipDeviceMemoryDecoders(GstElement *decodebin,
                                                               GstPad *pad,
                                                               GstCaps *caps,
                                                               GstElementFactory *factory);

/**
 * @brief routes dynamically produced H.264/H.265 pads into bitstream parsers
 * and pinned decode chains
 */
typedef struct
{
    GstBin *bin;
    GstElement *h264parse;
    GstElement *h265parse;
    VcatRgbaChain h264Chain;
    VcatRgbaChain h265Chain;
    GstElement *sink;
    bool routed;
} VcatH26xRouter;

typedef enum
{
    VIDEO_CONTAINER_UNKNOWN = 0,
    VIDEO_CONTAINER_MPEG_TS, // MPEG Transport Stream (.ts)
    VIDEO_CONTAINER_MP4,     // MPEG-4 Part 14 (.mp4)
    VIDEO_CONTAINER_MKV      // Matroska (.mkv)
} VideoContainer;

const char *videoContainerToString(VideoContainer container);

/**
 * @brief build an h264parse/h265parse router tail
 *
 * Creates the parsers ("config-interval"=1) and adds them to @p bin. Decode
 * chains are built lazily: a hardware decoder is only instantiated when a
 * matching codec pad actually arrives (or is prepared explicitly, see
 * vcatH26xRouterPrepareCodecChain).
 *
 * @return RC_OK if successful
 */
rc_t vcatH26xRouterCreate(VcatH26xRouter *router, GstBin *bin);

/**
 * @brief wire the parser of @p codec into its pinned decode chain
 *
 * Builds the codec's decode chain once, links the router's matching parser
 * into it, and outputs the chain tail which the caller links into the
 * exposed queue. Subsequent calls for the same codec return the existing
 * tail without rebuilding.
 *
 * @return RC_OK if successfully wired, RC_ERROR when no decoder for @p codec
 *         is available or wiring failed
 */
rc_t vcatH26xRouterPrepareCodecChain(VcatH26xRouter *router, VideoCodec codec, GstElement **tail);

/**
 * @brief route new H.264/H.265 elementary-stream pads of @p demuxer into
 * @p router
 *
 * Connects the demuxer's "pad-added": pads carrying video/x-h264 or
 * video/x-h265 are linked into the matching parser and its pinned decode
 * chain; the chain tail is linked into @p sink (the exposed queue). The first
 * video pad wins; later pads are skipped.
 */
void vcatH26xRouterRoutePads(VcatH26xRouter *router, GstElement *demuxer, GstElement *sink);

#endif // VCAT_VIDEO_SOURCE_UTILS_H
