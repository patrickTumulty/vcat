
#ifndef VCAT_ASCII_TUI_RENDERER_H
#define VCAT_ASCII_TUI_RENDERER_H

#include "tui_session.h"
#include "video_manager.h"

typedef struct AsciiTUIRenderer AsciiTUIRenderer;

AsciiTUIRenderer *vcatAsciiTuiRendererCreate(VideoManager *videoManager);

void vcatAsciiTuiRendererFree(AsciiTUIRenderer *renderer);

ITUIRenderer *vcatAsciiTuiRendererAsITUIRenderer(AsciiTUIRenderer *renderer);

void vcatAsciiTuiRendererToggleFreezeFrame(AsciiTUIRenderer *renderer);

#endif // VCAT_ASCII_TUI_RENDERER_H
