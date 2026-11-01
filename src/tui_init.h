
#ifndef TUI_INIT_H
#define TUI_INIT_H

#include "crox_types.h"
#include "input_args.h"
#include "tui_session.h"
#include "video_manager.h"

rc_t tuiInit(VideoManager *videoManager, TUISession *tuiSession, const VideoSourceInfo *sourceInfo);
rc_t tuiFree(void);

#endif // TUI_INIT_H
