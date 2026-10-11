#include "vcat_plugin_types.h"
#include <gst/gstelement.h>
#include <stdbool.h>
#include <unistd.h>

/**
 * @brief Get the name of the current plugin
 *
 * @return plugin name
 */
char *vcatPluginGetName()
{
    return "UDP Source";
}

/**
 * @brief Get the type of the current plugin
 *
 * @return plugin type
 */
PluginType vcatPluginGetType()
{
    return PLUGIN_TYPE_VIDEO_SOURCE;
}

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
