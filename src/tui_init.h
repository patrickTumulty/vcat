
#ifndef TUI_INIT_H
#define TUI_INIT_H

#include "crox_types.h"
#include "tui_session.h"
#include "video_manager.h"

Rc tuiInit(VideoManager *videoManager, TUISession *tuiSession);
Rc tuiFree(void);

#endif // TUI_INIT_H
