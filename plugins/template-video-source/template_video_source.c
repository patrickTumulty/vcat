
#include "common_types.h"
#include "vcat_plugin_framework.h"
#include <gst/gstelement.h>
#include <stdbool.h>

VCAT_DEFINE_PLUGIN_NAME("Template")
VCAT_DEFINE_PLUGIN_TYPE(PLUGIN_TYPE_VIDEO_SOURCE)
VCAT_DEFINE_PLUGIN_VERSION("0.0.0")

/**
 * @brief process input args
 *
 * @param[in] argc argc
 * @param[in] argv argv
 *
 * @return RC_OK if successful
 */
Rc videoSourceProcessInputArgs(int argc, char *argv[])
{
    return RC_OK;
}

/**
 * @brief Check if the current plugin can initialize. Usually this is
 * is dependent on whether the correct input args were provided.
 *
 * @return true if the plugin can initialize
 */
bool videoSourceCanInitialize()
{
    return false;
}

/**
 * @brief Video Source Initialize
 *
 * @return RC_OK if successful
 */
Rc videoSourceInitialize()
{
    return RC_OK;
}

/**
 * @brief Video Source Cleanup
 *
 * @return RC_OK if successful
 */
Rc videoSourceCleanup()
{
    return RC_OK;
}

/**
 * @brief Video Source Get GStreamer Source Element
 *
 * @return RC_OK if successful
 */
GstElement *videoSourceGetSourceElement()
{
    return NULL;
}

/**
 * @brief Video Source Get GStreamer Source Bin
 *
 * @return RC_OK if successful
 */
GstElement *videoSourceGetSourceBin()
{
    return NULL;
}
