#ifndef TUI_INIT_H
#define TUI_INIT_H

#include "crox_types.h"
#include "input_args.h"
#include "tui_session.h"
#include "video_manager.h"

/// Composes the TUI: renderers, controller and overlays. userArgs seeds the
/// toggles a user picked on the command line; may be NULL for defaults.
rc_t tuiInit(VideoManager *videoManager,
             TUISession *tuiSession,
             const VideoSourceInfo *sourceInfo,
             const VcatUserArgs *userArgs);

rc_t tuiFree(void);

#endif // TUI_INIT_H
