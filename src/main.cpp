#include "ascii_tui_renderer.hpp"
#include "logging.hpp"
#include "tui_session.hpp"
#include "utils.hpp"
#include "video_config.hpp"
#include "video_manager.hpp"
#include <cstdio>
#include <cstring>
#include <exception>

#include <csignal>
#include <memory>

namespace
{

std::shared_ptr<VideoManager> vm = nullptr;
std::unique_ptr<TUISession> tuiSession = nullptr;

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        tuiSession->stop();
        vm->stop();
    }
}

} // namespace

int main(int argc, char *argv[])
{
    logging::init();

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    Ip ip;
    int port = 0;
    VideoConfig vConfig{};

    for (int i = 1; i < argc; i++)
    {
        if (sscanf(argv[i], "udp://%hhu.%hhu.%hhu.%hhu:%d", &ip.octet3, &ip.octet2, &ip.octet1, &ip.octet0, &port))
        {
            vConfig.sourceType = VideoSourceType::UDP_MPEGTS;
        }
        else if (sscanf(argv[i], "udp://localhost:%d", &port))
        {
            ip = Ip::localhost();
            vConfig.sourceType = VideoSourceType::UDP_MPEGTS;
        }
        else if (strcmp(argv[i], "test") == 0)
        {
            vConfig.sourceType = VideoSourceType::TEST;
        }
    }

    logging::info("**** vcat: STARTING");

    if (vConfig.sourceType == VideoSourceType::NONE)
    {
        return 0;
    }

    try
    {
        vm = std::make_shared<VideoManager>(vConfig);
        tuiSession = std::make_unique<TUISession>();
        tuiSession->registerRenderer(std::make_shared<AsciiTUIRenderer>(vm));
        vm->run();
        tuiSession->run();
    }
    catch (const std::exception &ex)
    {
        logging::error("Something went wrong! {}", ex.what());
    }

    logging::info("**** vcat: EXITING");

    return 0;
}
