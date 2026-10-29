
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
 * @brief a functional chain producing plain system-memory RGBA.
 *
 * @p head is the element upstream video is linked into; @p tail is a
 * capsfilter whose src pad exposes `video/x-raw,format=RGBA` in system
 * memory. Link @p tail into the source bin's exposed queue.
 */
typedef struct
{
    GstElement *head;
    GstElement *tail;
} VcatRgbaChain;

/**
 * @brief build a convert-to-RGBA tail for already-decoded (raw system-memory) video
 *
 * Appends a video converter - a vendor-accelerated one when an installed
 * plugin provides it, CPU `videoconvert` otherwise - followed by a capsfilter
 * pinning `video/x-raw,format=RGBA`, and adds both to @p bin. The chain is not
 * linked to anything upstream; link the decoded video into `chain->head` and
 * `chain->tail` into the bin's queue.
 *
 * @return RC_OK if successful, RC_ERROR when no converter could be created
 */
Rc vcatRgbaChainCreate(VcatRgbaChain *chain, GstBin *bin);

/**
 * @brief build a pinned decode chain for @p codec: decoder + convert-to-RGBA tail
 *
 * Probes candidate decoder+converter pairs (platform hardware decoders first,
 * CPU `avdec_*` last via gst_element_factory probing at runtime: one binary,
 * no vendor build dependency) and adds the first complete match to @p bin,
 * linked decoder -> converter -> capsfilter(RGBA). The chosen mapping is
 * logged.
 *
 * @return RC_OK if successful, RC_ERROR when no candidate is installed
 */
Rc vcatRgbaChainCreateForCodec(VcatRgbaChain *chain, GstBin *bin, VideoCodec codec);

/**
 * @brief Mirror of gst-plugins-base's private `GstAutoplugSelectResult`, the
 * return type of decodebin's "autoplug-select" signal (numeric values match).
 */
typedef enum
{
    VCAT_AUTOPLUG_SELECT_TRY    = 0,
    VCAT_AUTOPLUG_SELECT_EXPOSE = 1,
    VCAT_AUTOPLUG_SELECT_SKIP   = 2,
} VcatAutoplugSelectResult;

/**
 * @brief decodebin autoplug-select handler rejecting device-memory-only video
 * decoders
 *
 * Connect on a decodebin whose decoded video flows into a system-memory RGBA
 * converter chain (see vcatRgbaChainCreate): a decoder that can only emit
 * device-memory frames (e.g. Jetson's NVMM or CUDA decoders) can never reach
 * the chain's capsfilter, so decodebin is asked to skip it and plug the next
 * candidate instead - CPU decoders are always available as the last resort.
 *
 * @return the GStreamer autoplug select result: SKIP for unsuitable decoders,
 *         TRY for everything else
 */
VcatAutoplugSelectResult vcatDecodebinAutoplugSelect(GstElement *decodebin, GstPad *pad, GstCaps *caps,
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
    VcatRgbaChain h264Chain; // Head is NULL until a codec pad is routed.
    VcatRgbaChain h265Chain;
    GstElement *sink;   // The exposee the chain tails link into.
    bool linked;        // A video pad has been routed end to end.
} VcatH26xRouter;

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
 * @brief build an h264parse/h265parse router tail
 *
 * Creates the parsers ("config-interval"=1) and adds them to @p bin. Decode
 * chains are built lazily: a hardware decoder is only instantiated when a
 * matching codec pad actually arrives (or is prepared explicitly, see
 * vcatH26xRouterPrepareCodecChain).
 *
 * @return RC_OK if successful
 */
Rc vcatH26xRouterCreate(VcatH26xRouter *router, GstBin *bin);

/**
 * @brief wire the parser of @p codec into its pinned decode chain
 *
 * Builds the codec's decode chain (vcatRgbaChainCreateForCodec) once, links
 * this router's matching parser into it, and outputs the chain tail which the
 * caller links into the exposed queue. Subsequent calls for the same codec
 * return the existing tail without rebuilding.
 *
 * @return RC_OK if successfully wired, RC_ERROR when no decoder for @p codec
 *         is available or wiring failed
 */
Rc vcatH26xRouterPrepareCodecChain(VcatH26xRouter *router, VideoCodec codec, GstElement **tail);

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
