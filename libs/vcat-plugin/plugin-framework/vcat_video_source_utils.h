
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

/**
 * @brief create a 1-buffer downstream-leaky queue
 *
 * Keeps the render path latency bounded; the caller adds it to its bin.
 *
 * @return the queue, or NULL (logged)
 */
GstElement *vcatCreateLeakyQueue(void);

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
Rc vcatH26xRouterLinkParser(VcatH26xRouter *router, bool forH264);

/**
 * @brief route new H.264/H.265 elementary-stream pads of @p demuxer into
 * @p router
 *
 * Connects the demuxer's "pad-added": pads carrying video/x-h264 or
 * video/x-h265 are linked to the matching parser; the first one wins.
 */
void vcatH26xRouterRoutePads(VcatH26xRouter *router, GstElement *demuxer);

#endif // VCAT_VIDEO_SOURCE_UTILS_H
