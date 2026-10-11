
#include "video_manager.hpp"
#include "gstreamer_init.hpp"
#include "logging.hpp"
#include "video2ascii_converter.hpp"
#include "video_pipeline.hpp"
#include <memory>

extern "C"
{
#include "vcat_plugin_api.h"
#include "vcat_plugin_manager.h"
}

class VideoManagerImpl
{
  public:
    VideoManagerImpl() = default;

    std::shared_ptr<Video2AsciiConverter> converter = nullptr;
    std::shared_ptr<VideoPipeline> pipeline = nullptr;
};

VideoManager::VideoManager() : _impl(std::make_unique<VideoManagerImpl>())
{
    initGStreamer();

    VCatPluginHandle handles[10];
    int handlesLen = 0;
    vcatPluginManagerGetLoadedPlugins(handles, &handlesLen, 10);

    VCatPluginHandle videoSourceHandle;

    for (int i = 0; i < 10; i++)
    {
        VCatPluginHandle handle = handles[i];
        if (vcatVideoSourceCanInitialize(handle))
        {
            vcatVideoSourceInitialize(handle);
            videoSourceHandle = handle;
            break;
        }
    }

    _impl->converter = std::make_shared<Video2AsciiConverter>();
    _impl->pipeline = std::make_shared<VideoPipeline>(videoSourceHandle, _impl->converter);
}

VideoManager::~VideoManager()
{
    _impl->converter = nullptr;
    _impl->pipeline = nullptr;
};

void VideoManager::run()
{
    logging::info("*** Start Pipeline");
    _impl->pipeline->start();
}

void VideoManager::stop()
{
    logging::info("*** Stop Pipeline");
    _impl->pipeline->stop();
}

void VideoManager::updateVideoBounds(Rectangle videoBounds)
{
    _impl->converter->updateVideoBounds(videoBounds);
}

std::shared_ptr<IAsciiFrameMailboxReader> VideoManager::accessAsciiFrameMailbox()
{
    return _impl->converter->accessAsciiFrameMailbox();
}
