
#include "video_manager.h"
#include "crox_types.h"
#include "gstreamer_init.h"
#include "logging_facade.h"
#include "vcat_plugin_api.h"
#include "video2ascii_converter.h"
#include "video_pipeline.h"
#include <stdlib.h>

struct VideoManager
{
    Video2AsciiConverter *converter;
    VideoPipeline *pipeline;
};

VideoManager *vcatVideoManagerCreate(VCatPluginHandle videoSource)
{
    vcatInitGStreamer();

    VideoManager *manager = calloc(1, sizeof(*manager));
    if (manager == NULL)
    {
        lfLogError("unable to allocate video manager");
        return NULL;
    }

    if (videoSource == NULL)
    {
        lfLogError("no video source plugin could initialize");
        free(manager);
        return NULL;
    }

    if (vcatVideoSourceInitialize(videoSource) != RC_OK)
    {
        lfLogError("Unable to initialize video source");
        return NULL;
    }

    manager->converter = vcatVideo2AsciiConverterCreate();
    manager->pipeline = vcatVideoPipelineCreate(videoSource, manager->converter);

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

Video2AsciiConverter *vcatVideoManagerGetAsciiConverter(VideoManager *manager)
{
    return manager != NULL ? manager->converter : NULL;
}

void vcatVideoManagerSetOnCapsChangeEvent(VideoManager *manager, OnCapsChangeCB callback)
{
    manager->pipeline->context.capsChangeCB = callback;
}
