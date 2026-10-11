#include "ascii_tui_renderer.hpp"
#include "logging.hpp"
#include "tui_session.hpp"
#include "utils.hpp"
#include "video_config.hpp"
#include "video_manager.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>

#include <atomic>
#include <csignal>
#include <memory>
#include <thread>

extern "C"
{
#include "vcat_plugin_api.h"
#include "vcat_plugin_manager.h"
}

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

    vcatPluginManagerInit();

    vcatPluginManagerLoadPlugins();

    VCatPluginHandle handles[10];
    int handlesLen = 0;
    vcatPluginManagerGetLoadedPlugins(handles, &handlesLen, 10);

    if (handlesLen == 0)
    {
        return 0;
    }

    for (int i = 0; i < handlesLen; i++)
    {
        vcatVideoSourceProcessInputArgs(handles[i], argc, argv);
    }

    logging::info("**** vcat: STARTING");

    try
    {
        vm = std::make_shared<VideoManager>();
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
