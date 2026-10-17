
#ifndef VCAT_VIDEO_MANAGER_H
#define VCAT_VIDEO_MANAGER_H

#include "ascii_frame_mailbox.h"
#include "utils.h"

/**
 * @brief The video side of the app: plugin discovery, the pipeline and the converter.
 *
 * Creation initializes GStreamer, picks the first video source plugin that can
 * initialize and starts nothing. run()/stop() control the pipeline.
 */
typedef struct VideoManager VideoManager;

/**
 * @brief Creates the manager. NULL when no video source plugin could initialize.
 */
VideoManager *vcatVideoManagerCreate(void);

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

#endif // VCAT_VIDEO_MANAGER_H
