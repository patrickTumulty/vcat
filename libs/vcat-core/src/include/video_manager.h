
#ifndef VCAT_VIDEO_MANAGER_H
#define VCAT_VIDEO_MANAGER_H

#include "ascii_frame_mailbox.h"
#include "vcat_plugin_api.h"
#include "utils.h"
#include "video2ascii_converter.h"

/**
 * @brief The video side of the app: GStreamer, the pipeline and the converter.
 *
 * Creation initializes GStreamer, initializes the given video source plugin and
 * starts nothing. run()/stop() control the pipeline.
 */
typedef struct VideoManager VideoManager;

/**
 * @brief Creates the manager. NULL when the video source could not initialize.
 */
VideoManager *vcatVideoManagerCreate(VCatPluginHandle videoSource);

/**
 * @brief Stops the pipeline if needed, then frees the manager.
 */
void vcatVideoManagerFree(VideoManager *manager);

/**
 * @brief Starts the pipeline.
 */
void vcatVideoManagerRun(VideoManager *manager);

/**
 * @brief Stops the pipeline.
 */
void vcatVideoManagerStop(VideoManager *manager);

/**
 * @brief Tells the converter the terminal changed size.
 */
void vcatVideoManagerUpdateVideoBounds(VideoManager *manager, Rectangle videoBounds);

/**
 * @brief The mailbox the converter publishes to; readers borrow it, the manager owns it.
 */
AsciiFrameMailbox *vcatVideoManagerGetFrameMailbox(VideoManager *manager);

Video2AsciiConverter *vcatVideoManagerGetAsciiConverter(VideoManager *manager);

#endif // VCAT_VIDEO_MANAGER_H
