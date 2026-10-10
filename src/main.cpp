#include "ascii_tui_renderer.hpp"
#include "logging.hpp"
#include "tui_session.hpp"
#include "utils.hpp"
#include "video_config.hpp"
#include "video_manager.hpp"
#include <cstdio>
#include <cstring>
#include <exception>

#include <atomic>
#include <csignal>
#include <memory>
#include <thread>

namespace
{

std::shared_ptr<VideoManager> vm = nullptr;
std::unique_ptr<TUISession> tuiSession = nullptr;

std::atomic<bool> shutdownRequested{false};

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        shutdownRequested.store(true);
        shutdownRequested.notify_one();
    }
}

void watchForShutdown()
{
    shutdownRequested.wait(false);
    logging::info("Shutting down from signal interuppt");
    if (shutdownRequested && tuiSession != nullptr)
    {
        tuiSession->stop();
    }
}

} // namespace

int main(int argc, char *argv[])
{
    logging::logInit();

    Ip ip;
    VideoConfig config{};

    for (int i = 1; i < argc; i++)
    {
        if (sscanf(argv[i], "udp://%hhu.%hhu.%hhu.%hhu:%d", &ip.octet3, &ip.octet2, &ip.octet1, &ip.octet0,
                   &config.network.port))
        {
            config.sourceType = VideoSourceType::UDP_MPEGTS;
            config.network.ip = ip;
        }
        else if (sscanf(argv[i], "udp://localhost:%d", &config.network.port))
        {
            ip = Ip::localhost();
            config.sourceType = VideoSourceType::UDP_MPEGTS;
            config.network.ip = Ip::localhost();
        }
        else if (strcmp(argv[i], "test") == 0)
        {
            config.sourceType = VideoSourceType::TEST;
        }
    }

    logging::info("**** vcat: STARTING");

    if (config.sourceType == VideoSourceType::NONE)
    {
        return 0;
    }

    try
    {
        vm = std::make_shared<VideoManager>(config);
        tuiSession = std::make_unique<TUISession>();

        // Registered after construction (and after initscr, so ncurses keeps its own
        // handlers): the watcher can then never fire on a null session.
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        std::thread shutdownWatcher(watchForShutdown);

        tuiSession->registerRenderer(std::make_shared<AsciiTUIRenderer>(vm));
        vm->run();
        tuiSession->run();

        shutdownRequested.store(true);
        shutdownRequested.notify_one();
        shutdownWatcher.join();

        vm->stop();
    }
    catch (const std::exception &ex)
    {
        logging::error("Something went wrong! {}", ex.what());
    }

    logging::info("**** vcat: EXITING");

    return 0;
}
