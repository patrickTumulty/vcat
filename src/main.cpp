#include "gstreamer_init.hpp"
#include "logging.hpp"
#include "tui_session.hpp"
#include "utils.hpp"
#include "video2ascii_converter.hpp"
#include "video_pipeline.hpp"
#include "videosrc_test.hpp"
#include "videosrc_udp.hpp"
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>

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

    logging::info("**** vcat: STARTING");

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
            logging::info("udp://{}.{}.{}.{}:{}", ip.octet3, ip.octet2, ip.octet1, ip.octet0, port);
            break;
        case TEST:
            videoSource = std::make_shared<TestVideoSrc>();
            logging::info("test src");
            break;
        case NONE:
        default:
            return 0;
        }

        auto videoConverter = std::make_shared<Video2AsciiConverter>();
        auto videoPipeline = VideoPipeline(videoSource, videoConverter);
        auto tuiSession = TUISession();

        // TODO(ncurses): the converter can no longer be registered as a TUISessionListener,
        // because vcat-core has no awareness of the interface in vcat-tui yet.
        // tuiSession.addTUISessionListener(videoConverter);

        videoPipeline.start();
        tuiSession.run();
    }
    catch (const std::exception &ex)
    {
        logging::error("Something went wrong! {}", ex.what());
    }

    logging::info("**** vcat: EXITING");

    return 0;
}
