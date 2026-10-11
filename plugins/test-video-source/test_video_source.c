#include "common_types.h"
#include "gst/gstbin.h"
#include "gst/gstelement.h"
#include "gst/gstutils.h"
#include "gst/gstvalue.h"
#include "logging_facade.h"
#include "vcat_plugin_types.h"
#include <stdbool.h>
#include <string.h>

static GstElement *gSourceBin = NULL;
static GstElement *gSourceElement = NULL;
static bool inputArgsPresent = false;

/**
 * @brief Get the name of the current plugin
 *
 * @return plugin name
 */
char *vcatPluginGetName()
{
    return "Test Source";
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
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "test") == 0)
        {
            inputArgsPresent = true;
        }
    }

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
    return inputArgsPresent;
}

#define verifyElement(E, MSG)                    \
    if (!(E))                                    \
    {                                            \
        lfLogError("%s: %s is NULL", (MSG), #E); \
        return RC_FAIL;                          \
    }

/**
 * @brief Video Source Initialize
 *
 * @return RC_OK if successful
 */
Rc videoSourceInitialize()
{
    char *failMessage = "Unable to initialize test video source";

    gSourceBin = gst_bin_new("video_src_bin");
    verifyElement(gSourceBin, failMessage);

    GstElement *source = gst_element_factory_make("videotestsrc", NULL);
    verifyElement(source, failMessage);

    GstElement *videoconvert = gst_element_factory_make("videoconvert", NULL);
    verifyElement(videoconvert, failMessage);

    GstElement *capsfilter = gst_element_factory_make("capsfilter", NULL);
    verifyElement(capsfilter, failMessage);

    g_object_set(G_OBJECT(source), //
                 "is-live", true,  //
                 "pattern", 18,    // ball=18, snow=1
                 NULL);

    GstCaps *caps = gst_caps_new_simple("video/x-raw",                         //
                                        "format", G_TYPE_STRING, "RGB",        //
                                        "width", G_TYPE_INT, 1280,             //
                                        "height", G_TYPE_INT, 720,             //
                                        "framerate", GST_TYPE_FRACTION, 25, 1, //
                                        NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(gSourceBin), //
                     source,              //
                     videoconvert,        //
                     capsfilter,          //
                     NULL);

    if (!gst_element_link_many(source,       //
                               videoconvert, //
                               capsfilter,   //
                               NULL))
    {
        lfLogError("%s: failed to link source -> videoconvert -> capsfilter", failMessage);
        return RC_FAIL;
    }

    gSourceElement = capsfilter;

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
    return gSourceElement;
}

/**
 * @brief Video Source Get GStreamer Source Bin
 *
 * @return RC_OK if successful
 */
GstElement *videoSourceGetSourceBin()
{
    return gSourceBin;
}
