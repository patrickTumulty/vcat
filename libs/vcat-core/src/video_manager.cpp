
#include "VideoConfig.hpp"
#include "VideoManager.hpp"
#include "gstreamer_init.hpp"
#include "video2ascii_converter.hpp"
#include "video_pipeline.hpp"
#include "videosrc_test.hpp"
#include "videosrc_udp.hpp"
#include <memory>

class VideoManagerImpl
{
  public:
    VideoManagerImpl() = default;

    std::shared_ptr<Video2AsciiConverter> converter = nullptr;
    std::shared_ptr<VideoPipeline> pipeline = nullptr;
};

VideoManager::VideoManager(VideoConfig config) : _config(config), _impl(std::make_unique<VideoManagerImpl>())
{
    initGStreamer();

    std::shared_ptr<IVideoSrc> videoSource;

    switch (_config.sourceType)
    {
    case UDP_MPEGTS:
        videoSource = std::make_shared<UdpVideoSrc>(_config.network);
        break;
    case TEST:
        videoSource = std::make_shared<TestVideoSrc>();
        break;
    case NONE:
    default:
        throw std::runtime_error("No video source specified");
    }

    _impl->converter = std::make_shared<Video2AsciiConverter>();
    _impl->pipeline = std::make_shared<VideoPipeline>(videoSource, _impl->converter);
}

VideoManager::~VideoManager() = default;

void VideoManager::run()
{
    _impl->pipeline->start();
}

void VideoManager::stop()
{
    _impl->pipeline->stop();
}
