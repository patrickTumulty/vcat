
#include "video_manager.h"
#include "gstreamer_init.h"
#include "logging.h"
#include "logging_facade.h"
#include "vcat_plugin_api.h"
#include "vcat_plugin_manager.h"
#include "video2ascii_converter.h"
#include "video_pipeline.h"
#include <stdlib.h>

/// How many plugin handles can be looked at once; the manager needs only one,
/// the first video source that can initialize.
#define MAX_PLUGINS 10

struct VideoManager
{
    Video2AsciiConverter *converter;
    VideoPipeline *pipeline;
};

VideoManager *vcatVideoManagerCreate(void)
{
    vcatInitGStreamer();

    VideoManager *manager = calloc(1, sizeof(*manager));
    if (manager == NULL)
    {
        lfLogError("unable to allocate video manager");
        return NULL;
    }

    VCatPluginHandle handles[MAX_PLUGINS] = {0};
    int handlesLen = 0;
    vcatPluginManagerGetLoadedPlugins(handles, &handlesLen, MAX_PLUGINS);

    VCatPluginHandle videoSourceHandle = NULL;

    for (int i = 0; i < MAX_PLUGINS; i++)
    {
        VCatPluginHandle handle = handles[i];
        if (vcatVideoSourceCanInitialize(handle))
        {
            vcatVideoSourceInitialize(handle);
            videoSourceHandle = handle;
            break;
        }
    }

    if (videoSourceHandle == NULL)
    {
        lfLogError("no video source plugin could initialize");
        free(manager);
        return NULL;
    }

    manager->converter = vcatVideo2AsciiConverterCreate();
    manager->pipeline = vcatVideoPipelineCreate(videoSourceHandle, manager->converter);

    if (manager->converter == NULL || manager->pipeline == NULL)
    {
        lfLogError("unable to create the video conversion stack");
        vcatVideoPipelineFree(manager->pipeline);
        vcatVideo2AsciiConverterFree(manager->converter);
        free(manager);
        return NULL;
    }

    return manager;
}

void vcatVideoManagerFree(VideoManager *manager)
{
    if (manager == NULL)
    {
        return;
    }
    vcatVideoPipelineFree(manager->pipeline);
    manager->pipeline = NULL;
    vcatVideo2AsciiConverterFree(manager->converter);
    manager->converter = NULL;
    free(manager);
}

void vcatVideoManagerRun(VideoManager *manager)
{
    if (manager == NULL)
    {
        return;
    }
    lfLogInfo("*** Start Pipeline");
    vcatVideoPipelineStart(manager->pipeline);
}

void vcatVideoManagerStop(VideoManager *manager)
{
    if (manager == NULL)
    {
        return;
    }
    lfLogInfo("*** Stop Pipeline");
    vcatVideoPipelineStop(manager->pipeline);
}

void vcatVideoManagerUpdateVideoBounds(VideoManager *manager, Rectangle videoBounds)
{
    if (manager == NULL)
    {
        return;
    }
    vcatVideo2AsciiConverterUpdateVideoBounds(manager->converter, videoBounds);
}

AsciiFrameMailbox *vcatVideoManagerGetFrameMailbox(VideoManager *manager)
{
    return manager != NULL ? vcatVideo2AsciiConverterGetFrameMailbox(manager->converter) : NULL;
}
