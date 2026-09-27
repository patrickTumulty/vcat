#include "video2ascii_converter.hpp"
#include "video_pipeline.hpp"
#include "videosrc_test.hpp"
#include "videosrc_udp.hpp"
#include <cstdio>
#include <cstring>
#include <exception>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/gstbuffer.h>
#include <gst/gstbus.h>
#include <gst/gstelement.h>
#include <gst/gstelementfactory.h>
#include <gst/gstmessage.h>
#include <gst/gstpad.h>
#include <gst/gstsample.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/video.h>
#include <memory>
#include <ncurses.h>
#include <ncursesw/ncurses.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include "logging.hpp"
#include "tui_session.hpp"
#include "utils.hpp"
#include "videosrc.hpp"

namespace
{
void gstLogToSpdlog(GstDebugCategory *category, GstDebugLevel level, const gchar *file, const gchar *_function,
                    gint line, GObject *object, GstDebugMessage *message, gpointer userData)
{
    (void)userData;
    auto logger = spdlog::get("gst");

    const gchar *text = gst_debug_message_get(message);
    const gchar *name = category ? gst_debug_category_get_name(category) : "unknown";

    spdlog::source_loc loc{file == nullptr ? "" : file, static_cast<int>(line), _function == nullptr ? "" : _function};

    switch (level)
    {
    case GST_LEVEL_ERROR:
        logger->log(loc, spdlog::level::err, "[{}] {}", name, text);
        break;
    case GST_LEVEL_WARNING:
    case GST_LEVEL_FIXME:
        logger->log(loc, spdlog::level::warn, "[{}] {}", name, text);
        break;
    case GST_LEVEL_INFO:
        logger->log(loc, spdlog::level::info, "[{}] {}", name, text);
        break;
    case GST_LEVEL_DEBUG:
        logger->log(loc, spdlog::level::debug, "[{}] {}", name, text);
        break;
    case GST_LEVEL_LOG:
    case GST_LEVEL_TRACE:
    case GST_LEVEL_MEMDUMP:
    default:
        logger->log(loc, spdlog::level::trace, "[{}] {}", name, text);
        break;
    }

    (void)object;
}

void initGStreamer()
{
    gst_init(nullptr, nullptr);

    gst_debug_set_default_threshold(GST_LEVEL_INFO);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    gst_debug_remove_log_function(gst_debug_log_default);
    gst_debug_add_log_function(gstLogToSpdlog, nullptr, nullptr);
#pragma GCC diagnostic pop
}

} // namespace

int main(int argc, char *argv[])
{
    logging::init();

    Ip ip;
    int port = 0;
    VideoSourceType videoSourceType = NONE;
    NetworkSource networkSource;

    for (int i = 1; i < argc; i++)
    {
        if (sscanf(argv[i], "udp://%hhu.%hhu.%hhu.%hhu:%d", &ip.octet3, &ip.octet2, &ip.octet1, &ip.octet0, &port))
        {
            videoSourceType = UDP_MPEGTS;
        }
        else if (sscanf(argv[i], "udp://localhost:%d", &port))
        {
            ip = Ip::localhost();
            videoSourceType = UDP_MPEGTS;
        }
        else if (strcmp(argv[i], "test") == 0)
        {
            videoSourceType = TEST;
        }
    }

    spdlog::info("**** tplay: STARTING");

    if (videoSourceType == NONE)
    {
        return 0;
    }

    try
    {
        initGStreamer();

        std::shared_ptr<IVideoSrc> videoSource;

        switch (videoSourceType)
        {
        case UDP_MPEGTS:
            videoSource = std::make_shared<UdpVideoSrc>(ip, port);
            spdlog::info("udp://{}.{}.{}.{}:{}", ip.octet3, ip.octet2, ip.octet1, ip.octet0, port);
            break;
        case TEST:
            videoSource = std::make_shared<TestVideoSrc>();
            spdlog::info("test src");
            break;
        case NONE:
        default:
            return 0;
        }

        auto videoConverter = std::make_shared<Video2AsciiConverter>();
        auto videoPipeline = VideoPipeline(videoSource, videoConverter);
        auto tuiSession = TUISession();
        tuiSession.addTUISessionListener(videoConverter);

        videoPipeline.start();
        tuiSession.run();
    }
    catch (const std::exception &ex)
    {
        spdlog::error("Something went wrong! {}", ex.what());
    }

    spdlog::info("**** tplay: EXITING");

    return 0;
}
