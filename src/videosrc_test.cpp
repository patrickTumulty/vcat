
#include "videosrc_test.hpp"
#include "glib-object.h"
#include "gst/gstbin.h"
#include "gst/gstutils.h"
#include "gst/gstvalue.h"
#include "utils.hpp"
#include <format>
#include <gst/video/video.h>
#include <stdexcept>
#include <string>

TestVideoSrc::TestVideoSrc()
{
    std::string failMessage = "Unable to initialize test video source";

    _srcBin = gst_bin_new("video_src_bin");
    verifyElement(_srcBin, STR(_srcBin), failMessage);

    GstElement *source = gst_element_factory_make("videotestsrc", nullptr);
    verifyElement(source, STR(source), failMessage);

    GstElement *videoconvert = gst_element_factory_make("videoconvert", nullptr);
    verifyElement(videoconvert, STR(videoconvert), failMessage);

    GstElement *capsfilter = gst_element_factory_make("capsfilter", nullptr);
    verifyElement(capsfilter, STR(capsfilter), failMessage);

    g_object_set(G_OBJECT(source),                   //
                 "is-live", true,                    //
                 "pattern", 18, // ball=18, snow=1
                 nullptr);

    GstCaps *caps = gst_caps_new_simple("video/x-raw",                         //
                                        "format", G_TYPE_STRING, "RGB",        //
                                        "width", G_TYPE_INT, 1280,             //
                                        "height", G_TYPE_INT, 720,             //
                                        "framerate", GST_TYPE_FRACTION, 25, 1, //
                                        nullptr);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(_srcBin), //
                     source,           //
                     videoconvert,     //
                     capsfilter,       //
                     NULL);

    if (!gst_element_link_many(source,       //
                               videoconvert, //
                               capsfilter,   //
                               NULL))
    {
        throw std::runtime_error(std::format("{}: Failed to link decoder -> converter -> sink", failMessage));
    }

    _srcElement = capsfilter;
}
